/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_node_service.h"
#include "stn_wire_internal.h"
#include <string.h>
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
    code=snapshot(s,&state);if(code!=STN_RPC_OK){return code;}
    switch(q->method){
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
