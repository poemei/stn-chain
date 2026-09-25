/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_node_service.h"
#include "stn_fork.h"
#include "stn_wire_internal.h"
#include "stn_address.h"
#include <string.h>
#include <stdlib.h>

static stn_rpc_code snapshot(const stn_node_service *s,stn_chain_state *state)
{
    stn_chain_report r;
    if(s->chain==NULL || s->chain->pow_policy==NULL || s->blocks==NULL || s->count==0){return STN_RPC_UNAVAILABLE;}
    r=stn_chain_reconstruct_history(s->chain,s->blocks,s->count,state);
    if(r.acceptance==STN_ACCEPTANCE_UNRESOLVED){return STN_RPC_UNAVAILABLE;}
    if(r.acceptance==STN_ACCEPTANCE_ERROR){return STN_RPC_PROVIDER;}
    if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){return STN_RPC_REJECTED;}
    return STN_RPC_OK;
}

static stn_rpc_code handle(void *user,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written,stn_chain_state *state)
{
    const stn_node_service *s=user;stn_rpc_code code;size_t index;
    if(written!=NULL){*written=0;}
    if(s==NULL || q==NULL || p==NULL || written==NULL){return STN_RPC_PROVIDER;}

    if(q->method==STN_RPC_DERIVE_ADDRESS){
        stn_address address;
        stn_address_type type;
        stn_data_status status;
        size_t source_length;
        size_t encoded=0;
        char text[STN_ADDRESS_TEXT_CAPACITY];

        if(q->payload==NULL || q->length<6){return STN_RPC_INVALID;}
        type=(stn_address_type)stn_wire_read(q->payload,2);
        if(type!=STN_ADDRESS_IDENTITY &&
            type!=STN_ADDRESS_CONTRACT &&
            type!=STN_ADDRESS_WALLET){return STN_RPC_INVALID;}

        source_length=(size_t)stn_wire_read(q->payload+2,4);
        if(source_length!=q->length-6){return STN_RPC_INVALID;}

        status=stn_address_derive(
            type,
            q->payload+6,
            source_length,
            &address);

        if(status!=STN_DATA_OK){
            return status==STN_DATA_CAPACITY || status==STN_DATA_LENGTH
                ? STN_RPC_CAPACITY
                : STN_RPC_PROVIDER;
        }

        status=stn_address_encode(
            &address,
            text,
            sizeof(text),
            &encoded);

        if(status!=STN_DATA_OK ||
            encoded!=(type==STN_ADDRESS_IDENTITY ? 69u : 70u)){
            return status==STN_DATA_CAPACITY
                ? STN_RPC_CAPACITY
                : STN_RPC_PROVIDER;
        }

        if(cap<encoded){return STN_RPC_CAPACITY;}

        memcpy(p,text,encoded);
        *written=encoded;
        return STN_RPC_OK;
    }

    if(q->method==STN_RPC_PENDING || q->method==STN_RPC_ADMIN_CONTROL || q->method==STN_RPC_INTELLIGENCE_ID ||
        q->method==STN_RPC_INTELLIGENCE_CURSOR || q->method==STN_RPC_MINING_TEMPLATE){return STN_RPC_UNAVAILABLE;}
    if(q->method==STN_RPC_CHECK_INTELLIGENCE || q->method==STN_RPC_SUBMIT_INTELLIGENCE){
        stn_validation_report r;uint16_t fields[10];size_t i;
        code=snapshot(s,state);if(code!=STN_RPC_OK){return code;}
        if(state->height==UINT64_MAX || state->height+1>=state->publication_activation_height){
            stn_record record;stn_lifecycle_result checked=stn_lifecycle_check_publication(state->lifecycle,q->payload,q->length,&s->chain->hash_provider);
            memset(&r,0,sizeof(r));
            r.acceptance=checked==STN_LIFECYCLE_OK ? STN_ACCEPTANCE_UNDER_CONTEXT :
                checked==STN_LIFECYCLE_PROVIDER || checked==STN_LIFECYCLE_CAPACITY ? STN_ACCEPTANCE_ERROR : STN_ACCEPTANCE_REJECTED;
            if(checked==STN_LIFECYCLE_OK && (stn_record_decode(q->payload,q->length,&record)!=STN_RECORD_OK || memcmp(record.network_id,state->network_id,32)!=0)){
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
    if(q->method==STN_RPC_GET_CURSOR_REORG_STATUS || q->method==STN_RPC_GET_CONSUMER_RECOVERY_PLAN){
        stn_chain_cursor cursor;stn_cursor_ancestor boundary;stn_consumer_recovery_plan plan;
        stn_rpc_code result;size_t needed;
        if(stn_chain_cursor_decode(q->payload,q->length,&cursor)!=STN_CURSOR_VALID)return STN_RPC_INVALID;
        if(q->method==STN_RPC_GET_CURSOR_REORG_STATUS){
            stn_cursor_reorg_result status=stn_chain_resolve_cursor_reorg(s->chain,s->blocks,s->count,s->retained,s->retained_count,&cursor,&boundary);
            if(status==STN_CURSOR_REORG_CURRENT)return STN_RPC_CURRENT;
            if(status==STN_CURSOR_REORG_NO_COMMON_ANCESTOR)return STN_RPC_NO_COMMON_ANCESTOR;
            if(status==STN_CURSOR_REORG_MALFORMED)return STN_RPC_INVALID;
            if(status==STN_CURSOR_REORG_UNAVAILABLE)return STN_RPC_UNAVAILABLE;
            if(status!=STN_CURSOR_REORG_COMMON_ANCESTOR)return STN_RPC_PROVIDER;
            result=STN_RPC_COMMON_ANCESTOR;needed=40;
        }else{
            stn_consumer_recovery_result status=stn_chain_build_consumer_recovery_plan(s->chain,s->blocks,s->count,s->retained,s->retained_count,&cursor,&plan);
            if(status==STN_RECOVERY_CURRENT)return STN_RPC_CURRENT;
            if(status==STN_RECOVERY_UNAVAILABLE)return STN_RPC_UNAVAILABLE;
            if(status==STN_RECOVERY_MALFORMED)return STN_RPC_INVALID;
            if(status!=STN_RECOVERY_FROM_START && status!=STN_RECOVERY_AFTER_CURSOR)return STN_RPC_PROVIDER;
            boundary=plan.rollback;result=status==STN_RECOVERY_FROM_START?STN_RPC_RECOVER_FROM_START:STN_RPC_RECOVER_AFTER_CURSOR;
            needed=status==STN_RECOVERY_FROM_START?40:85;
        }
        if(cap<needed)return STN_RPC_CAPACITY;
        stn_wire_write(p,8,boundary.height);memcpy(p+8,boundary.block_id,32);
        if(needed==85 && stn_chain_cursor_encode(&plan.resume,p+40,45)!=STN_CURSOR_VALID)return STN_RPC_PROVIDER;
        *written=needed;return result;
    }    if(q->method==STN_RPC_GET_FIRST_ACCEPTED_RECORD || q->method==STN_RPC_GET_NEXT_ACCEPTED_RECORD){
        stn_chain_record_match match;stn_chain_cursor cursor,position;
        if(q->method==STN_RPC_GET_FIRST_ACCEPTED_RECORD){
            stn_first_result result;
            if(q->length!=0)return STN_RPC_INVALID;
            result=stn_chain_first_record(s->chain,s->blocks,s->count,&match,&position);
            if(result==STN_FIRST_END)return STN_RPC_END;
            if(result==STN_FIRST_UNAVAILABLE)return STN_RPC_UNAVAILABLE;
            if(result==STN_FIRST_CAPACITY)return STN_RPC_CAPACITY;
            if(result!=STN_FIRST_RECORD)return STN_RPC_PROVIDER;
        }else{
            stn_next_result result;
            if(stn_chain_cursor_decode(q->payload,q->length,&cursor)!=STN_CURSOR_VALID)return STN_RPC_INVALID;
            result=stn_chain_next_record(s->chain,s->blocks,s->count,&cursor,&match,&position);
            if(result==STN_NEXT_END)return STN_RPC_END;
            if(result==STN_NEXT_DETACHED)return STN_RPC_DETACHED;
            if(result==STN_NEXT_MALFORMED)return STN_RPC_INVALID;
            if(result==STN_NEXT_UNAVAILABLE)return STN_RPC_UNAVAILABLE;
            if(result==STN_NEXT_CAPACITY)return STN_RPC_CAPACITY;
            if(result!=STN_NEXT_RECORD)return STN_RPC_PROVIDER;
        }
        if(cap<125 || match.transaction.length>cap-125)return STN_RPC_CAPACITY;
        memcpy(p,match.record_id,32);stn_wire_write(p+32,8,match.height);memcpy(p+40,match.block_id,32);
        stn_wire_write(p+72,4,position.transaction_position);
        if(stn_chain_cursor_encode(&position,p+76,45)!=STN_CURSOR_VALID)return STN_RPC_PROVIDER;
        stn_wire_write(p+121,4,match.transaction.length);memcpy(p+125,match.transaction.bytes,match.transaction.length);
        *written=125+match.transaction.length;return STN_RPC_OK;
    }    if(q->method==STN_RPC_GET_ACCEPTED_RECORD){
        stn_chain_record_match match;stn_data_status status;
        if(q->payload==NULL || q->length!=32)return STN_RPC_INVALID;
        status=stn_chain_lookup_record(s->chain,s->blocks,s->count,q->payload,&match);
        if(status==STN_DATA_UNRESOLVED)return STN_RPC_UNAVAILABLE;
        if(status==STN_DATA_CAPACITY)return STN_RPC_CAPACITY;
        if(status!=STN_DATA_OK)return STN_RPC_PROVIDER;
        if(!match.found)return STN_RPC_NOT_FOUND;
        if(cap<76 || match.transaction.length>cap-76)return STN_RPC_CAPACITY;
        memcpy(p,match.record_id,32);stn_wire_write(p+32,8,match.height);memcpy(p+40,match.block_id,32);
        stn_wire_write(p+72,4,match.transaction.length);memcpy(p+76,match.transaction.bytes,match.transaction.length);
        *written=76+match.transaction.length;return STN_RPC_OK;
    }
    code=snapshot(s,state);if(code!=STN_RPC_OK)return code;
    switch(q->method){
    case STN_RPC_INFO:
        if(cap<184){return STN_RPC_CAPACITY;}
        memcpy(p,state->network_id,32);memcpy(p+32,state->genesis_id,32);stn_wire_write(p+64,8,state->height);
        memcpy(p+72,state->tip_id,32);memcpy(p+104,state->cumulative_work.bytes,STN_WORK_SIZE);memcpy(p+144,state->current_target,32);
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
        if(memcmp(q->payload,state->tip_id,32)!=0){return STN_RPC_STALE;}
        if(q->method==STN_RPC_SUBMIT_WORK){return STN_RPC_UNAVAILABLE;}
        /* Matching tip is freshness only, not a valid template or solution. */
        /* fall through */
    case STN_RPC_MINING_CONTEXT:
        if(cap<76){return STN_RPC_CAPACITY;}
        memcpy(p,state->tip_id,32);memcpy(p+32,state->current_target,32);stn_wire_write(p+64,8,state->height);
        stn_wire_write(p+72,4,0);*written=76;return STN_RPC_OK;
    default:return STN_RPC_METHOD;
    }
    if(s->blocks[index].length>cap){return STN_RPC_CAPACITY;}
    memcpy(p,s->blocks[index].bytes,s->blocks[index].length);*written=s->blocks[index].length;return STN_RPC_OK;
}

stn_rpc_code stn_node_service_handle(void *user,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written)
{
    stn_chain_state state={0};
    stn_rpc_code code=handle(user,q,p,cap,written,&state);
    stn_chain_state_release(&state);
    return code;
}