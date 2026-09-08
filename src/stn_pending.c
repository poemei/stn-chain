/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pending.h"
#include "stn_wire_internal.h"
#include <stdlib.h>
#include <string.h>
void stn_pending_init(stn_pending *p)
{
    if(p!=NULL){memset(p,0,sizeof(*p));}
}
size_t stn_pending_count(const stn_pending *p)
{
    return p!=NULL ? p->count : 0;
}
static size_t find_id(const stn_pending *p,const uint8_t id[32])
{
    size_t at=0;
    while(at<p->count && memcmp(p->entries[at].id,id,32)<0){++at;}
    return at;
}
stn_pending_result stn_pending_insert(stn_pending *p,const uint8_t *bytes,
    size_t length,const stn_hash_provider *hash,uint8_t id[32])
{
    stn_pending_entry entry={0};stn_transaction tx;stn_record record;size_t at;
    if(p==NULL || id==NULL){return STN_PENDING_INVALID;}
    if(stn_transaction_decode(bytes,length,&tx)!=STN_DATA_OK ||
       stn_record_decode(tx.record_bytes,tx.record_length,&record)!=STN_RECORD_OK){return STN_PENDING_INVALID;}
    if(stn_transaction_id(bytes,length,hash,entry.id)!=STN_DATA_OK){return STN_PENDING_PROVIDER;}
    at=find_id(p,entry.id);
    if(at<p->count && memcmp(p->entries[at].id,entry.id,32)==0){return STN_PENDING_DUPLICATE;}
    if(p->count>=STN_PENDING_MAX_ENTRIES || length>STN_PENDING_MAX_BYTES-p->bytes){return STN_PENDING_CAPACITY;}
    entry.transaction=malloc(length);
    if(entry.transaction==NULL){return STN_PENDING_CAPACITY;}
    memcpy(entry.transaction,bytes,length);entry.length=length;
    memcpy(entry.signer,record.signer_public_key,32);memcpy(entry.nonce,record.nonce,32);
    memmove(p->entries+at+1,p->entries+at,(p->count-at)*sizeof(entry));
    p->entries[at]=entry;++p->count;p->bytes+=length;
    memcpy(id,entry.id,32);return STN_PENDING_ACCEPTED;
}
stn_pending_result stn_pending_lookup(const stn_pending *p,const uint8_t id[32],
    uint8_t *output,size_t capacity,size_t *written)
{
    size_t at;
    if(written!=NULL){*written=0;}
    if(p==NULL || id==NULL || output==NULL || written==NULL){return STN_PENDING_INVALID;}
    at=find_id(p,id);
    if(at==p->count || memcmp(p->entries[at].id,id,32)!=0){return STN_PENDING_NOT_FOUND;}
    if(capacity<p->entries[at].length){return STN_PENDING_CAPACITY;}
    memcpy(output,p->entries[at].transaction,p->entries[at].length);
    *written=p->entries[at].length;return STN_PENDING_ACCEPTED;
}
stn_pending_result stn_pending_remove(stn_pending *p,const uint8_t id[32])
{
    size_t at;
    if(p==NULL || id==NULL){return STN_PENDING_INVALID;}
    at=find_id(p,id);
    if(at==p->count || memcmp(p->entries[at].id,id,32)!=0){return STN_PENDING_NOT_FOUND;}
    p->bytes-=p->entries[at].length;free(p->entries[at].transaction);
    memmove(p->entries+at,p->entries+at+1,(p->count-at-1)*sizeof(p->entries[0]));
    --p->count;memset(p->entries+p->count,0,sizeof(p->entries[0]));
    return STN_PENDING_ACCEPTED;
}
stn_pending_result stn_pending_enumerate(const stn_pending *p,size_t start,
    uint8_t (*ids)[32],size_t capacity,size_t *written)
{
    size_t i,n;
    if(written!=NULL){*written=0;}
    if(p==NULL || written==NULL || (capacity!=0 && ids==NULL)){return STN_PENDING_INVALID;}
    n=start<p->count ? p->count-start : 0;if(n>capacity){n=capacity;}
    for(i=0;i<n;++i){memcpy(ids[i],p->entries[start+i].id,32);}
    *written=n;return STN_PENDING_ACCEPTED;
}
void stn_pending_clear(stn_pending *p)
{
    size_t i;if(p==NULL){return;}
    for(i=0;i<p->count;++i){free(p->entries[i].transaction);}memset(p,0,sizeof(*p));
}
static int same_nonce(const stn_pending_entry *e,const stn_record *r)
{return memcmp(e->signer,r->signer_public_key,32)==0 && memcmp(e->nonce,r->nonce,32)==0;}
static stn_data_status scan(const stn_pending *p,const stn_storage_view *v,uint8_t *mask,int replay,const stn_hash_provider *hash)
{
    size_t i,j,k,offset;stn_block b;stn_transaction tx;stn_record r;uint8_t id[32];
    if(p==NULL || v==NULL || mask==NULL || (v->count!=0 && v->blocks==NULL)){return STN_DATA_ARGUMENT;}
    memset(mask,0,STN_PENDING_MAX_ENTRIES);
    for(i=0;i<v->count;++i){
        if(stn_block_decode(v->blocks[i].bytes,v->blocks[i].length,&b)!=STN_DATA_OK){return STN_DATA_CONTENT;}
        offset=0;
        for(j=0;j<b.header.transaction_count;++j){
            size_t n=(size_t)stn_wire_read(b.body+offset,4);
            if(stn_transaction_decode(b.body+offset+4,n,&tx)!=STN_DATA_OK ||
               stn_record_decode(tx.record_bytes,tx.record_length,&r)!=STN_RECORD_OK){return STN_DATA_CONTENT;}
            if(!replay && stn_transaction_id(b.body+offset+4,n,hash,id)!=STN_DATA_OK){return STN_DATA_PROVIDER_ERROR;}
            for(k=0;k<p->count;++k){if(replay ? same_nonce(&p->entries[k],&r) : memcmp(p->entries[k].id,id,32)==0){mask[k]=1;}}
            offset+=4+n;
        }
    }
    return STN_DATA_OK;
}
stn_data_status stn_pending_inclusions(const stn_pending *p,const stn_storage_view *v,const stn_hash_provider *hash,uint8_t remove[STN_PENDING_MAX_ENTRIES])
{
    uint8_t mask[STN_PENDING_MAX_ENTRIES];stn_data_status s;
    if(remove==NULL){return STN_DATA_ARGUMENT;}
    s=scan(p,v,mask,0,hash);if(s==STN_DATA_OK){memcpy(remove,mask,sizeof(mask));}return s;
}
void stn_pending_prune(stn_pending *p,const uint8_t remove[STN_PENDING_MAX_ENTRIES])
{
    size_t i,n=0;
    for(i=0;i<p->count;++i){
        if(remove[i]){p->bytes-=p->entries[i].length;free(p->entries[i].transaction);}
        else{p->entries[n++]=p->entries[i];}
    }
    memset(p->entries+n,0,(p->count-n)*sizeof(p->entries[0]));p->count=n;
}
static stn_pending_result disposition(const stn_validation_report *r)
{
    if(r->acceptance==STN_ACCEPTANCE_UNRESOLVED){return STN_PENDING_UNAVAILABLE;}
    if(r->acceptance==STN_ACCEPTANCE_ERROR){return STN_PENDING_PROVIDER;}
    if(r->acceptance==STN_ACCEPTANCE_UNDER_CONTEXT){return STN_PENDING_ACCEPTED;}
    if(r->envelope_error==STN_RECORD_UNSUPPORTED){return STN_PENDING_UNSUPPORTED;}
    if(r->payload_error==STN_INTELLIGENCE_VERSION_ERROR){return STN_PENDING_UNSUPPORTED;}
    if(r->network==STN_STAGE_REJECT){return STN_PENDING_NETWORK;}
    if(r->time==STN_STAGE_REJECT){return STN_PENDING_TIME;}
    if(r->signature==STN_STAGE_REJECT){return STN_PENDING_SIGNATURE;}
    if(r->authority==STN_STAGE_REJECT){return STN_PENDING_AUTHORITY;}
    if(r->replay==STN_STAGE_REJECT){return STN_PENDING_REPLAY;}
    return STN_PENDING_INVALID;
}
stn_pending_result stn_pending_admit(stn_pending *p,const uint8_t *record,size_t length,
    const stn_validation_context *c,const stn_storage_view *active,const stn_hash_provider *hash,
    stn_validation_report *report,uint8_t id[32])
{
    stn_pending_result result;stn_pending_entry e={0};stn_record r;stn_transaction tx;
    uint8_t encoded[STN_TX_MAX_SIZE],mask[STN_PENDING_MAX_ENTRIES];size_t n,i;
    stn_pending probe={0};
    if(report==NULL || id==NULL){return STN_PENDING_PROVIDER;}
    memset(report,0,sizeof(*report));memset(id,0,32);
    if(p==NULL || active==NULL || hash==NULL){report->acceptance=STN_ACCEPTANCE_ERROR;return STN_PENDING_PROVIDER;}
    if(c==NULL){report->acceptance=STN_ACCEPTANCE_UNRESOLVED;return STN_PENDING_UNAVAILABLE;}
    if(memcmp(c->expected_network,active->state.network_id,32)!=0){report->acceptance=STN_ACCEPTANCE_UNRESOLVED;return STN_PENDING_UNAVAILABLE;}
    *report=stn_validate_intelligence_record(record,length,c);result=disposition(report);
    if(result!=STN_PENDING_ACCEPTED){return result;}
    if(stn_record_decode(record,length,&r)!=STN_RECORD_OK){return STN_PENDING_INVALID;}
    tx.version=1;tx.type=STN_TX_PUBLICATION;tx.record_bytes=record;tx.record_length=(uint32_t)length;
    if(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)!=STN_DATA_OK ||
       stn_transaction_id(encoded,n,hash,e.id)!=STN_DATA_OK){return STN_PENDING_PROVIDER;}
    memcpy(id,e.id,32);memcpy(e.signer,r.signer_public_key,32);memcpy(e.nonce,r.nonce,32);e.length=n;
    i=find_id(p,e.id);
    if(i<p->count && memcmp(p->entries[i].id,e.id,32)==0){return STN_PENDING_DUPLICATE;}
    for(i=0;i<p->count;++i){
        if(same_nonce(&p->entries[i],&r)){return STN_PENDING_REPLAY;}
    }
    /* This check supplements, never replaces, the configured replay hook.
     * Exact accepted IDs necessarily share this network/signer/nonce tuple. */
    probe.entries[0]=e;probe.count=1;
    if(scan(&probe,active,mask,1,NULL)!=STN_DATA_OK){return STN_PENDING_PROVIDER;}
    if(mask[0]){return STN_PENDING_REPLAY;}
    return stn_pending_insert(p,encoded,n,hash,id);
}
stn_pending_result stn_pending_admit_transaction(stn_pending *p,const uint8_t *bytes,
    size_t length,const stn_validation_context *c,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32])
{
    stn_transaction tx;stn_data_status status;
    if(report==NULL || id==NULL){return STN_PENDING_PROVIDER;}
    memset(report,0,sizeof(*report));memset(id,0,32);
    status=stn_transaction_decode(bytes,length,&tx);
    if(status!=STN_DATA_OK){
        report->structure=STN_STAGE_REJECT;report->acceptance=STN_ACCEPTANCE_REJECTED;
        if(status==STN_DATA_VERSION || status==STN_DATA_TYPE){return STN_PENDING_UNSUPPORTED;}
        /* The transaction decoder already checked the outer frame before
         * CONTENT. Preserve the record validator's unsupported distinction. */
        if(status==STN_DATA_CONTENT){
            stn_record record;
            report->envelope_error=stn_record_decode(bytes+STN_TX_HEADER_SIZE,
                length-STN_TX_HEADER_SIZE,&record);
            if(report->envelope_error==STN_RECORD_UNSUPPORTED){return STN_PENDING_UNSUPPORTED;}
        }
        return STN_PENDING_INVALID;
    }
    return stn_pending_admit(p,tx.record_bytes,tx.record_length,c,active,hash,report,id);
}
stn_data_status stn_pending_assemble(const stn_pending *p,const stn_validation_context *c,const stn_storage_view *active,
    uint8_t *body,size_t capacity,size_t *written,uint32_t *count)
{
    uint8_t replay[STN_PENDING_MAX_ENTRIES],ids[STN_PENDING_MAX_ENTRIES][32];
    stn_transaction_span selected[STN_BLOCK_MAX_TRANSACTIONS];uint32_t n=0;size_t i,total=0,available;
    if(written!=NULL){*written=0;}if(count!=NULL){*count=0;}
    if(p==NULL || body==NULL || written==NULL || count==NULL){return STN_DATA_ARGUMENT;}
    if(c==NULL){return STN_DATA_UNRESOLVED;}
    if(active==NULL || memcmp(c->expected_network,active->state.network_id,32)!=0){return STN_DATA_UNRESOLVED;}
    if(scan(p,active,replay,1,NULL)!=STN_DATA_OK){return STN_DATA_CONTENT;}
    if(stn_pending_enumerate(p,0,ids,STN_PENDING_MAX_ENTRIES,&available)!=STN_PENDING_ACCEPTED){return STN_DATA_CONTENT;}
    for(i=0;i<available && n<STN_BLOCK_MAX_TRANSACTIONS;++i){
        size_t at=find_id(p,ids[i]);
        const stn_pending_entry *e=&p->entries[at];stn_transaction tx;stn_validation_report r;
        if(replay[at]){continue;}
        if(stn_transaction_decode(e->transaction,e->length,&tx)!=STN_DATA_OK){return STN_DATA_CONTENT;}
        r=stn_validate_intelligence_record(tx.record_bytes,tx.record_length,c);
        if(r.acceptance==STN_ACCEPTANCE_ERROR){return STN_DATA_PROVIDER_ERROR;}
        if(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){continue;}
        if(e->length+4>STN_BLOCK_MAX_BODY-total){break;}
        /* A caller capacity failure must not silently change the chosen block. */
        if(e->length+4>capacity-total){return STN_DATA_CAPACITY;}
        selected[n].bytes=e->transaction;selected[n].length=(uint32_t)e->length;++n;total+=4+e->length;
    }
    if(n==0){return STN_DATA_UNRESOLVED;}
    {stn_data_status s=stn_block_body_encode(selected,n,body,capacity,written);if(s==STN_DATA_OK){*count=n;}return s;}
}
