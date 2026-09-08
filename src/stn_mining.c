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
static int storage_bytes_required(const stn_storage_view *v,size_t extra,size_t *required)
{
    size_t total=STN_STORAGE_OVERHEAD,i;
    if(v==NULL || required==NULL){return 0;}
    for(i=0;i<v->count;++i){
        if(v->blocks[i].length>SIZE_MAX-total-4u){return 0;}
        total+=4u+v->blocks[i].length;
    }
    if(extra>SIZE_MAX-total-4u){return 0;}
    *required=total+4u+extra;return 1;
}
static int grow(uint8_t **p,size_t *capacity,size_t required)
{
    uint8_t *next;size_t cap;
    if(required<=*capacity){return 1;}
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
    return grow(&s->snapshot,&s->snapshot_capacity,required) &&
        grow(&s->workspace.current_bytes,&s->workspace.current_capacity,required) &&
        grow(&s->workspace.next_bytes,&s->workspace.next_capacity,required);
}
static stn_rpc_code template_build(stn_mining_service *s,const stn_storage_view *v,size_t *length,uint8_t id[32])
{
    static const uint8_t domain[]="STN-CHAIN:WORK:ID:1";
    stn_block b={0};size_t offset=0,i;stn_work increment,total;
    if(v->state.height==UINT64_MAX){return STN_RPC_CAPACITY;}
    if(s->body==NULL || s->transaction_count==0){return STN_RPC_UNAVAILABLE;}
    if(stn_block_body_validate_structure(s->body,s->body_length,s->transaction_count)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    for(i=0;i<s->transaction_count;++i){
        size_t n=(size_t)stn_wire_read(s->body+offset,4);
        if(memcmp(s->body+offset+4+20,s->chain->network_id,32)!=0){return STN_RPC_REJECTED;}
        offset+=4+n;
    }
    if(stn_target_work(v->state.current_target,&increment)!=STN_DATA_OK ||
       stn_work_add(&v->state.cumulative_work,&increment,&total)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    b.header.version=3;memcpy(b.header.network_id,v->state.network_id,32);
    memcpy(b.header.previous_hash,v->state.tip_id,32);b.header.height=v->state.height+1;
    b.header.timestamp=v->state.timestamp;memcpy(b.header.reserved_target,v->state.current_target,32);
    b.header.transaction_count=s->transaction_count;b.header.body_length=(uint32_t)s->body_length;b.body=s->body;
    if(stn_block_body_commitment(b.body,s->body_length,s->transaction_count,&s->chain->hash_provider,b.header.transaction_commitment)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    if(s->template_capacity<168+s->body_length){return STN_RPC_CAPACITY;}
    if(stn_block_encode(&b,s->template_bytes,s->template_capacity,length)!=STN_DATA_OK ||
       stn_block_check_integrity(s->template_bytes,*length,&s->chain->hash_provider)!=STN_DATA_OK){return STN_RPC_REJECTED;}
    return stn_sha256(NULL,domain,sizeof(domain),s->template_bytes,*length,id)==STN_DATA_OK ? STN_RPC_OK : STN_RPC_PROVIDER;
}
stn_rpc_code stn_mining_handle(void *user,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written)
{
    stn_mining_service *s=user;stn_storage_view v={0};stn_rpc_code code;size_t n=0,required=0;
    uint8_t id[32];stn_node_service query;
    if(written!=NULL){*written=0;}
    if(s==NULL || q==NULL || p==NULL || written==NULL || s->chain==NULL || s->storage==NULL ||
       s->chain->hash_provider.hash!=stn_sha256 || s->chain->pow_policy==NULL || s->template_bytes==NULL){return STN_RPC_PROVIDER;}
    code=storage_code(stn_storage_load(s->chain,s->storage,s->snapshot,s->snapshot_capacity,&v));
    if(code!=STN_RPC_OK){return code;}
    if(q->method==STN_RPC_SUBMIT_WORK){
        if(q->payload==NULL || q->length<68+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY ||
           q->length>68+STN_BLOCK_MAX_SIZE || stn_wire_read(q->payload+64,4)!=q->length-68){code=STN_RPC_INVALID;goto done;}
        if(memcmp(q->payload,v.state.tip_id,32)!=0){code=STN_RPC_STALE;goto done;}
    }
    if(q->method!=STN_RPC_MINING_TEMPLATE && q->method!=STN_RPC_SUBMIT_WORK){
        query.chain=s->chain;query.blocks=v.blocks;query.count=v.count;query.intelligence=NULL;
        code=stn_node_service_handle(&query,q,p,cap,written);
        if(code==STN_RPC_OK && (q->method==STN_RPC_MINING_CONTEXT || q->method==STN_RPC_CHECK_WORK_BASE)){
            stn_wire_write(p+72,4,template_build(s,&v,&n,id)==STN_RPC_OK ? 1 : 0);
        }
        goto done;
    }
    code=template_build(s,&v,&n,id);if(code!=STN_RPC_OK){goto done;}
    if(q->method==STN_RPC_MINING_TEMPLATE){
        if(cap<68+n){code=STN_RPC_CAPACITY;goto done;}
        memcpy(p,v.state.tip_id,32);memcpy(p+32,id,32);stn_wire_write(p+64,4,n);
        memcpy(p+68,s->template_bytes,n);*written=68+n;code=STN_RPC_OK;goto done;
    }
    if(cap<72){code=STN_RPC_CAPACITY;goto done;}
    if(q->length!=68+n || memcmp(q->payload+32,id,32)!=0 ||
       memcmp(q->payload+68,s->template_bytes,STN_MINING_NONCE_OFFSET)!=0 ||
       memcmp(q->payload+68+STN_MINING_NONCE_OFFSET+STN_MINING_NONCE_SIZE,
              s->template_bytes+STN_MINING_NONCE_OFFSET+STN_MINING_NONCE_SIZE,
              n-STN_MINING_NONCE_OFFSET-STN_MINING_NONCE_SIZE)!=0){code=STN_RPC_REJECTED;goto done;}
    if(!storage_bytes_required(&v,n,&required) || !ensure_storage_capacity(s,required)){code=STN_RPC_CAPACITY;goto done;}
    code=storage_code(stn_storage_extend(s->chain,s->storage,q->payload+68,n,&s->workspace,&s->active));
    if(code!=STN_RPC_OK){goto done;}
    memcpy(p,s->active.tip_id,32);stn_wire_write(p+32,8,s->active.height);
    memcpy(p+40,s->active.cumulative_work.bytes,32);*written=72;
done:
    stn_storage_view_release(&v);return code;
}
