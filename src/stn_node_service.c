/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_node_service.h"
#include "stn_wire_internal.h"
#include <string.h>
#include <stdlib.h>

/* The caller has revalidated the complete immutable accepted history. The two
 * replay projections deliberately differ: legacy qualifying publications affect
 * query eligibility, never historical consensus acceptance of later actions. */
static stn_rpc_code accepted_record(const stn_node_service *s,const uint8_t key[32],
    uint64_t activation,uint8_t *out,size_t capacity,size_t *written)
{
    stn_lifecycle_state historical,observed;stn_replay_state eligible;
    stn_block block;stn_transaction tx;size_t i,j,offset,total=0,grants=0,gb,rb;
    uint8_t *memory,initial[32]={0},id[32],block_id[32];stn_rpc_code code=STN_RPC_NOT_FOUND;
    for(i=0;i<s->count;++i){
        if(stn_block_decode(s->blocks[i].bytes,s->blocks[i].length,&block)!=STN_DATA_OK)return STN_RPC_PROVIDER;
        if(total>SIZE_MAX-block.header.transaction_count)return STN_RPC_CAPACITY;
        total+=block.header.transaction_count;offset=0;
        for(j=0;j<block.header.transaction_count;++j){
            size_t n=(size_t)stn_wire_read(block.body+offset,4);offset+=4;
            if(stn_transaction_decode(block.body+offset,n,&tx)!=STN_DATA_OK)return STN_RPC_PROVIDER;
            if(tx.type==STN_TX_AUTHORITY_GRANT)++grants;
            offset+=n;
        }
    }
    if(grants>SIZE_MAX/STN_AUTHORITY_GRANT_SIZE || total>SIZE_MAX/(2u*STN_REPLAY_ID_SIZE))return STN_RPC_CAPACITY;
    gb=grants*STN_AUTHORITY_GRANT_SIZE;rb=total*STN_REPLAY_ID_SIZE;
    if(gb>SIZE_MAX-2u*rb)return STN_RPC_CAPACITY;
    memory=malloc(gb+2u*rb);if(memory==NULL)return STN_RPC_PROVIDER;
    if(s->chain->genesis_initial_identity_count)memcpy(initial,s->chain->genesis_initial_identities,32);
    stn_lifecycle_initialize(&historical,memory,grants,memory+gb,total,initial);
    stn_lifecycle_set_initial_identities(&historical,s->chain->genesis_initial_identities,s->chain->genesis_initial_identity_count);
    stn_replay_state_initialize(&eligible,memory+gb+rb,total);
    for(i=0;i<s->count;++i){
        if(stn_block_decode(s->blocks[i].bytes,s->blocks[i].length,&block)!=STN_DATA_OK){code=STN_RPC_PROVIDER;goto done;}
        offset=0;
        for(j=0;j<block.header.transaction_count;++j){
            size_t n=(size_t)stn_wire_read(block.body+offset,4);stn_lifecycle_result result;
            offset+=4;
            if(stn_transaction_decode(block.body+offset,n,&tx)!=STN_DATA_OK){code=STN_RPC_PROVIDER;goto done;}
            if(tx.type==STN_TX_PUBLICATION){
                observed=historical;observed.replay=eligible;
                result=stn_lifecycle_check_publication(&observed,tx.record_bytes,tx.record_length,&s->chain->hash_provider);
                if(result==STN_LIFECYCLE_PROVIDER || result==STN_LIFECYCLE_CAPACITY){code=STN_RPC_PROVIDER;goto done;}
                if(result==STN_LIFECYCLE_OK){
                    if(stn_record_id(tx.record_bytes,tx.record_length,&s->chain->hash_provider,id)!=STN_DATA_OK){code=STN_RPC_PROVIDER;goto done;}
                    if(memcmp(id,key,32)==0){
                        if(capacity<76 || n>capacity-76){code=STN_RPC_CAPACITY;goto done;}
                        if(stn_chain_block_id(s->blocks[i].bytes,s->blocks[i].length,&s->chain->hash_provider,block_id)!=STN_DATA_OK){code=STN_RPC_PROVIDER;goto done;}
                        memcpy(out,id,32);stn_wire_write(out+32,8,block.header.height);memcpy(out+40,block_id,32);
                        stn_wire_write(out+72,4,n);memcpy(out+76,block.body+offset,n);*written=76+n;code=STN_RPC_OK;goto done;
                    }
                    if(stn_lifecycle_apply_transaction(&observed,&tx,s->chain->genesis_authority_roots,s->chain->genesis_authority_root_count,&s->chain->hash_provider)!=STN_LIFECYCLE_OK){code=STN_RPC_PROVIDER;goto done;}
                    eligible=observed.replay;
                }
                if(block.header.height<activation){offset+=n;continue;}
            }
            result=stn_lifecycle_apply_transaction(&historical,&tx,s->chain->genesis_authority_roots,s->chain->genesis_authority_root_count,&s->chain->hash_provider);
            if(result!=STN_LIFECYCLE_OK){code=STN_RPC_PROVIDER;goto done;}
            if(stn_replay_state_consume(&eligible,historical.replay.consumed+(historical.replay.consumed_count-1)*STN_REPLAY_ID_SIZE)==STN_REPLAY_MALFORMED){code=STN_RPC_PROVIDER;goto done;}
            offset+=n;
        }
    }
done:
    free(memory);return code;
}
static stn_rpc_code snapshot(const stn_node_service *s,stn_chain_state *state)
{
    stn_chain_state current,next;stn_chain_report r;size_t i;
    if(s->chain==NULL || s->chain->pow_policy==NULL || s->blocks==NULL || s->count==0){return STN_RPC_UNAVAILABLE;}
    if(stn_chain_initialize(s->chain,&current)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    for(i=0;i<s->count;++i){
        r=stn_chain_validate_candidate(s->chain,&current,s->blocks[i].bytes,s->blocks[i].length,&next);
        if(r.acceptance==STN_ACCEPTANCE_UNRESOLVED){return STN_RPC_UNAVAILABLE;}
        if(r.acceptance==STN_ACCEPTANCE_ERROR){return STN_RPC_PROVIDER;}
        if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){return STN_RPC_REJECTED;}
        current=next;
    }
    *state=current;return STN_RPC_OK;
}
stn_rpc_code stn_node_service_handle(void *user,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written)
{
    const stn_node_service *s=user;stn_chain_state state;stn_rpc_code code;size_t index;
    if(written!=NULL){*written=0;}
    if(s==NULL || q==NULL || p==NULL || written==NULL){return STN_RPC_PROVIDER;}
    if(q->method==STN_RPC_PENDING || q->method==STN_RPC_ADMIN_CONTROL || q->method==STN_RPC_INTELLIGENCE_ID ||
        q->method==STN_RPC_INTELLIGENCE_CURSOR || q->method==STN_RPC_MINING_TEMPLATE){return STN_RPC_UNAVAILABLE;}
    if(q->method==STN_RPC_CHECK_INTELLIGENCE || q->method==STN_RPC_SUBMIT_INTELLIGENCE){
        stn_validation_report r;uint16_t fields[10];size_t i;
        code=snapshot(s,&state);if(code!=STN_RPC_OK){return code;}
        if(state.height==UINT64_MAX || state.height+1>=state.publication_activation_height){
            stn_record record;stn_lifecycle_result checked=stn_lifecycle_check_publication(state.lifecycle,q->payload,q->length,&s->chain->hash_provider);
            memset(&r,0,sizeof(r));
            r.acceptance=checked==STN_LIFECYCLE_OK ? STN_ACCEPTANCE_UNDER_CONTEXT :
                checked==STN_LIFECYCLE_PROVIDER || checked==STN_LIFECYCLE_CAPACITY ? STN_ACCEPTANCE_ERROR : STN_ACCEPTANCE_REJECTED;
            if(checked==STN_LIFECYCLE_OK && (stn_record_decode(q->payload,q->length,&record)!=STN_RECORD_OK || memcmp(record.network_id,state.network_id,32)!=0)){
                r.acceptance=STN_ACCEPTANCE_REJECTED;r.network=STN_STAGE_REJECT;
            }
        }else{
            if(s->intelligence==NULL || memcmp(s->intelligence->expected_network,s->chain->network_id,32)!=0){return STN_RPC_UNAVAILABLE;}
            r=stn_validate_intelligence_record(q->payload,q->length,s->intelligence);
        }
        if(q->method==STN_RPC_SUBMIT_INTELLIGENCE){
            return r.acceptance==STN_ACCEPTANCE_ERROR ? STN_RPC_PROVIDER :
                r.acceptance==STN_ACCEPTANCE_REJECTED ? STN_RPC_REJECTED : STN_RPC_UNAVAILABLE;
        }
        if(cap<20){return STN_RPC_CAPACITY;}
        fields[0]=(uint16_t)r.structure;fields[1]=(uint16_t)r.payload;fields[2]=(uint16_t)r.network;
        fields[3]=(uint16_t)r.time;fields[4]=(uint16_t)r.signature;fields[5]=(uint16_t)r.authority;
        fields[6]=(uint16_t)r.replay;fields[7]=(uint16_t)r.acceptance;
        fields[8]=(uint16_t)r.envelope_error;fields[9]=(uint16_t)r.payload_error;
        for(i=0;i<10;++i){stn_wire_write(p+i*2,2,fields[i]);}*written=20;return STN_RPC_OK;
    }
    if(q->method==STN_RPC_GET_ACCEPTED_RECORD && (q->payload==NULL || q->length!=32))return STN_RPC_INVALID;
    code=snapshot(s,&state);if(code!=STN_RPC_OK){return q->method==STN_RPC_GET_ACCEPTED_RECORD && code==STN_RPC_REJECTED ? STN_RPC_PROVIDER : code;}
    switch(q->method){
    case STN_RPC_GET_ACCEPTED_RECORD:
        return accepted_record(s,q->payload,state.publication_activation_height,p,cap,written);
    case STN_RPC_INFO:
        if(cap<184){return STN_RPC_CAPACITY;}
        memcpy(p,state.network_id,32);memcpy(p+32,state.genesis_id,32);stn_wire_write(p+64,8,state.height);
        memcpy(p+72,state.tip_id,32);memcpy(p+104,state.cumulative_work.bytes,STN_WORK_SIZE);memcpy(p+144,state.current_target,32);
        stn_wire_write(p+176,4,1);stn_wire_write(p+180,4,s->count);*written=184;return STN_RPC_OK;
    case STN_RPC_BLOCK_HEIGHT:
        if(stn_wire_read(q->payload,8)>=s->count){return STN_RPC_NOT_FOUND;}
        index=(size_t)stn_wire_read(q->payload,8);break;
    case STN_RPC_BLOCK_ID:
        for(index=0;index<s->count;++index){
            uint8_t id[32];
            if(stn_chain_block_id(s->blocks[index].bytes,s->blocks[index].length,&s->chain->hash_provider,id)!=STN_DATA_OK){return STN_RPC_PROVIDER;}
            if(memcmp(id,q->payload,32)==0){break;}
        }
        if(index==s->count){return STN_RPC_NOT_FOUND;}break;
    case STN_RPC_CHECK_WORK_BASE:case STN_RPC_SUBMIT_WORK:
        if(memcmp(q->payload,state.tip_id,32)!=0){return STN_RPC_STALE;}
        if(q->method==STN_RPC_SUBMIT_WORK){return STN_RPC_UNAVAILABLE;}
        /* Matching tip is freshness only, not a valid template or solution. */
        /* fall through */
    case STN_RPC_MINING_CONTEXT:
        if(cap<76){return STN_RPC_CAPACITY;}
        memcpy(p,state.tip_id,32);memcpy(p+32,state.current_target,32);stn_wire_write(p+64,8,state.height);
        stn_wire_write(p+72,4,0);*written=76;return STN_RPC_OK;
    default:return STN_RPC_METHOD;
    }
    if(s->blocks[index].length>cap){return STN_RPC_CAPACITY;}
    memcpy(p,s->blocks[index].bytes,s->blocks[index].length);*written=s->blocks[index].length;return STN_RPC_OK;
}
