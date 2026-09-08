/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_mining.h"
#include "stn_sha256.h"
#include "stn_wire_internal.h"
#include <string.h>
#include <stdlib.h>

static stn_rpc_code storage_code(stn_storage_status s)
{
    if(s==STN_STORAGE_OK){return STN_RPC_OK;}
    if(s==STN_STORAGE_STALE){return STN_RPC_STALE;}
    if(s==STN_STORAGE_CAPACITY){return STN_RPC_CAPACITY;}
    if(s==STN_STORAGE_VALIDATION || s==STN_STORAGE_FORMAT || s==STN_STORAGE_NOT_PREFERRED){return STN_RPC_REJECTED;}
    if(s==STN_STORAGE_NOT_FOUND || s==STN_STORAGE_UNRESOLVED){return STN_RPC_UNAVAILABLE;}
    return STN_RPC_PROVIDER;
}
static stn_rpc_submission_result submission_code(stn_pending_result result)
{
    switch(result){
    case STN_PENDING_ACCEPTED:return STN_RPC_ADMITTED;
    case STN_PENDING_DUPLICATE:return STN_RPC_DUPLICATE;
    case STN_PENDING_CAPACITY:return STN_RPC_POOL_FULL;
    case STN_PENDING_INVALID:case STN_PENDING_NETWORK:case STN_PENDING_TIME:return STN_RPC_BAD_SUBMISSION;
    case STN_PENDING_UNSUPPORTED:return STN_RPC_UNSUPPORTED_SUBMISSION;
    case STN_PENDING_REPLAY:return STN_RPC_REPLAY;
    case STN_PENDING_SIGNATURE:case STN_PENDING_AUTHORITY:return STN_RPC_UNAUTHORIZED;
    case STN_PENDING_UNAVAILABLE:return STN_RPC_ADMISSION_UNAVAILABLE;
    default:return STN_RPC_ADMISSION_INTERNAL;
    }
}
static int storage_bytes_required(const stn_storage_view *v,size_t extra,size_t *required)
{
    size_t total=STN_STORAGE_OVERHEAD,i;
    if(v==NULL || required==NULL){return 0;}
    for(i=0;i<v->count;++i){
        if(total>SIZE_MAX-4u || v->blocks[i].length>SIZE_MAX-total-4u){return 0;}
        total+=4u+v->blocks[i].length;
    }
    if(total>SIZE_MAX-4u || extra>SIZE_MAX-total-4u){return 0;}
    *required=total+4u+extra;return 1;
}
static int grow(uint8_t **p,size_t *capacity,size_t required,int owned)
{
    uint8_t *next;size_t cap;
    if(required<=*capacity){return 1;}
    if(!owned){return 0;}
    cap=*capacity ? *capacity : 4096u;
    while(cap<required){
        size_t doubled=cap<=SIZE_MAX/2u ? cap*2u : SIZE_MAX;
        if(doubled<=cap){cap=required;break;}
        cap=doubled;
    }
    next=(uint8_t*)realloc(*p,cap);
    if(next==NULL){return 0;}
    *p=next;*capacity=cap;return 1;
}
static int ensure_storage_capacity(stn_mining_service *s,size_t required)
{
    return
        grow(&s->workspace.current_bytes,&s->workspace.current_capacity,required,s->owns_buffers) &&
        grow(&s->workspace.next_bytes,&s->workspace.next_capacity,required,s->owns_buffers);
}
static stn_rpc_code template_build(stn_mining_service *s,const stn_storage_view *v,size_t *length,uint8_t id[32])
{
    static const uint8_t domain[]="STN-CHAIN:WORK:ID:1";
    stn_block b={0};size_t offset=0,i;stn_work increment,total;
    const uint8_t *body=s->body;size_t body_length=s->body_length;uint32_t transaction_count=s->transaction_count;
    if(s->pending!=NULL){
        stn_data_status assembled=stn_pending_assemble(s->pending,s->intelligence,v,s->pending_body,s->pending_body_capacity,&body_length,&transaction_count);
        if(assembled!=STN_DATA_OK){return assembled==STN_DATA_UNRESOLVED ? STN_RPC_UNAVAILABLE :
            assembled==STN_DATA_CAPACITY ? STN_RPC_CAPACITY : STN_RPC_PROVIDER;}
        body=s->pending_body;
    }
    if(v->state.height==UINT64_MAX){return STN_RPC_CAPACITY;}
    if(body==NULL || transaction_count==0){return STN_RPC_UNAVAILABLE;}
    if(stn_block_body_validate_structure(body,body_length,transaction_count)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    for(i=0;i<transaction_count;++i){
        size_t n=(size_t)stn_wire_read(body+offset,4);
        if(memcmp(body+offset+4+20,s->chain->network_id,32)!=0){return STN_RPC_REJECTED;}
        offset+=4+n;
    }
    if(stn_target_work(v->state.current_target,&increment)!=STN_DATA_OK ||
       stn_work_add(&v->state.cumulative_work,&increment,&total)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    b.header.version=3;memcpy(b.header.network_id,v->state.network_id,32);
    memcpy(b.header.previous_hash,v->state.tip_id,32);b.header.height=v->state.height+1;
    b.header.timestamp=v->state.timestamp;memcpy(b.header.reserved_target,v->state.current_target,32);
    b.header.transaction_count=transaction_count;b.header.body_length=(uint32_t)body_length;b.body=body;
    if(stn_block_body_commitment(b.body,body_length,transaction_count,&s->chain->hash_provider,b.header.transaction_commitment)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    if(s->template_capacity<168+body_length){return STN_RPC_CAPACITY;}
    if(stn_block_encode(&b,s->template_bytes,s->template_capacity,length)!=STN_DATA_OK ||
       stn_block_check_integrity(s->template_bytes,*length,&s->chain->hash_provider)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    return stn_sha256(NULL,domain,sizeof(domain),s->template_bytes,*length,id)==STN_DATA_OK ? STN_RPC_OK : STN_RPC_PROVIDER;
}
stn_rpc_code stn_mining_handle(void *user,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written)
{
    stn_mining_service *s=user;stn_storage_view v={0};stn_rpc_code code;size_t n=0,required=0;stn_chain_state accepted;
    uint8_t id[32],remove[STN_PENDING_MAX_ENTRIES]={0};stn_node_service query;
    if(written!=NULL){*written=0;}
    if(s==NULL || q==NULL || p==NULL || written==NULL || s->chain==NULL || s->storage==NULL ||
       s->storage->acquire==NULL || s->storage->release==NULL || s->storage->read==NULL || s->storage->replace==NULL ||
       s->chain->hash_provider.hash!=stn_sha256 || s->chain->pow_policy==NULL || s->template_bytes==NULL){return STN_RPC_PROVIDER;}
    if(s->owns_buffers){
        size_t needed=0;stn_storage_status probe=s->storage->acquire(s->storage->user);
        if(probe!=STN_STORAGE_OK){return storage_code(probe);}
        probe=s->storage->read(s->storage->user,s->snapshot,0,&needed);
        s->storage->release(s->storage->user);
        if(probe!=STN_STORAGE_OK && probe!=STN_STORAGE_CAPACITY){return storage_code(probe);}
        if(!grow(&s->snapshot,&s->snapshot_capacity,needed,1)){return STN_RPC_CAPACITY;}
    }
    code=storage_code(stn_storage_load(s->chain,s->storage,s->snapshot,s->snapshot_capacity,&v));
    if(code!=STN_RPC_OK){return code;}
    if(q->method==STN_RPC_SUBMIT_TRANSACTION){
        stn_validation_report report;stn_pending_result result;
        if(cap<36){code=STN_RPC_CAPACITY;goto done;}
        if(q->length>STN_TX_MAX_SIZE){code=STN_RPC_INVALID;goto done;}
        result=s->pending==NULL ? STN_PENDING_UNAVAILABLE :
            stn_pending_admit_transaction(s->pending,q->payload,q->length,
                s->intelligence,&v,&s->chain->hash_provider,&report,id);
        memset(p,0,36);stn_wire_write(p,2,1);stn_wire_write(p+2,2,submission_code(result));
        if(result==STN_PENDING_ACCEPTED || result==STN_PENDING_DUPLICATE){memcpy(p+4,id,32);}
        *written=36;code=STN_RPC_OK;goto done;
    }
    if(s->pending!=NULL){
        /* Work construction observes pending state; cleanup is a separate
         * operation even if the active history makes an entry ineligible. */
        if(q->method!=STN_RPC_MINING_TEMPLATE && q->method!=STN_RPC_MINING_CONTEXT &&
           q->method!=STN_RPC_CHECK_WORK_BASE){
            if(stn_pending_inclusions(s->pending,&v,remove)!=STN_DATA_OK){code=STN_RPC_PROVIDER;goto done;}
            stn_pending_prune(s->pending,remove);
        }
        if(q->method==STN_RPC_SUBMIT_INTELLIGENCE){
            stn_validation_context unavailable={0};stn_validation_report r;stn_pending_result result;uint16_t fields[10];size_t i;
            if(cap<56){code=STN_RPC_CAPACITY;goto done;}
            memcpy(unavailable.expected_network,s->chain->network_id,32);
            result=stn_pending_admit(s->pending,q->payload,q->length,
                s->intelligence!=NULL ? s->intelligence : &unavailable,&v,&s->chain->hash_provider,&r,id);
            stn_wire_write(p,2,1);stn_wire_write(p+2,2,result);
            fields[0]=(uint16_t)r.structure;fields[1]=(uint16_t)r.payload;fields[2]=(uint16_t)r.network;
            fields[3]=(uint16_t)r.time;fields[4]=(uint16_t)r.signature;fields[5]=(uint16_t)r.authority;
            fields[6]=(uint16_t)r.replay;fields[7]=(uint16_t)r.acceptance;fields[8]=(uint16_t)r.envelope_error;fields[9]=(uint16_t)r.payload_error;
            for(i=0;i<10;++i){stn_wire_write(p+4+2*i,2,fields[i]);}memcpy(p+24,id,32);
            *written=56;code=STN_RPC_OK;goto done;
        }
        if(q->method==STN_RPC_PENDING){
            n=16;if(cap<n){code=STN_RPC_CAPACITY;goto done;}
            stn_wire_write(p,4,s->pending->count);stn_wire_write(p+4,4,STN_PENDING_MAX_ENTRIES);stn_wire_write(p+8,4,s->pending->bytes);
            stn_wire_write(p+n-4,4,STN_PENDING_MAX_BYTES);
            *written=n;code=STN_RPC_OK;goto done;
        }
    }
    if(q->method==STN_RPC_SUBMIT_WORK){
        if(q->payload==NULL || q->length<68+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY ||
           q->length>68+STN_BLOCK_MAX_SIZE || stn_wire_read(q->payload+64,4)!=q->length-68){code=STN_RPC_INVALID;goto done;}
        if(memcmp(q->payload,v.state.tip_id,32)!=0){code=STN_RPC_STALE;goto done;}
    }
    if(q->method!=STN_RPC_MINING_TEMPLATE && q->method!=STN_RPC_SUBMIT_WORK){
        query.chain=s->chain;query.blocks=v.blocks;query.count=v.count;query.intelligence=s->intelligence;
        code=stn_node_service_handle(&query,q,p,cap,written);
        if(code==STN_RPC_OK && (q->method==STN_RPC_MINING_CONTEXT || q->method==STN_RPC_CHECK_WORK_BASE)){
            stn_wire_write(p+72,4,template_build(s,&v,&n,id)==STN_RPC_OK ? 1 : 0);
        }
        goto done;
    }
    code=template_build(s,&v,&n,id);if(code!=STN_RPC_OK){
        if(s->pending!=NULL && q->method==STN_RPC_SUBMIT_WORK && code==STN_RPC_UNAVAILABLE){code=STN_RPC_STALE;}
        goto done;}
    if(q->method==STN_RPC_MINING_TEMPLATE){
        if(cap<68+n){code=STN_RPC_CAPACITY;goto done;}
        memcpy(p,v.state.tip_id,32);memcpy(p+32,id,32);stn_wire_write(p+64,4,n);
        memcpy(p+68,s->template_bytes,n);*written=68+n;code=STN_RPC_OK;goto done;
    }
    if(cap<72){code=STN_RPC_CAPACITY;goto done;}
    if(s->pending!=NULL && memcmp(q->payload+32,id,32)!=0){code=STN_RPC_STALE;goto done;}
    if(q->length!=68+n || memcmp(q->payload+32,id,32)!=0 ||
       memcmp(q->payload+68,s->template_bytes,STN_MINING_NONCE_OFFSET)!=0 ||
       memcmp(q->payload+68+STN_MINING_NONCE_OFFSET+STN_MINING_NONCE_SIZE,
              s->template_bytes+STN_MINING_NONCE_OFFSET+STN_MINING_NONCE_SIZE,
              n-STN_MINING_NONCE_OFFSET-STN_MINING_NONCE_SIZE)!=0){code=STN_RPC_REJECTED;goto done;}
    if(s->pending!=NULL){
        stn_block_span block={q->payload+68,n};stn_storage_view inclusion={0};inclusion.blocks=&block;inclusion.count=1;
        if(stn_pending_inclusions(s->pending,&inclusion,remove)!=STN_DATA_OK){code=STN_RPC_PROVIDER;goto done;}
    }
    if(!storage_bytes_required(&v,n,&required) || !ensure_storage_capacity(s,required)){code=STN_RPC_CAPACITY;goto done;}
    accepted=v.state;
    code=storage_code(stn_storage_extend(s->chain,s->storage,q->payload+68,n,&s->workspace,&accepted));
    if(code!=STN_RPC_OK){goto done;}
    s->active=accepted;
    if(s->pending!=NULL){stn_pending_prune(s->pending,remove);}
    memcpy(p,s->active.tip_id,32);stn_wire_write(p+32,8,s->active.height);
    memcpy(p+40,s->active.cumulative_work.bytes,32);*written=72;
done:
    stn_storage_view_release(&v);return code;
}
