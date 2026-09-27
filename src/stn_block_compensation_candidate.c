/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_candidate.h"
#include "stn_issuance.h"
#include "stn_issuance_binding.h"
#include "stn_sha256.h"
#include <stdlib.h>
#include <string.h>

stn_data_status stn_block_compensation_candidate_apply(
    const stn_transaction *evidence_tx,
    const stn_transaction *issuance_tx,
    stn_block_compensation_replay *replay,
    const stn_compensation_state *compensation,
    stn_economic_state *economy,
    stn_block_compensation_evidence *accepted_evidence)
{
    stn_block_compensation_evidence evidence;
    stn_issuance_record issuance;
    stn_data_status status;

    if(evidence_tx==NULL || issuance_tx==NULL || replay==NULL ||
       compensation==NULL || economy==NULL)return STN_DATA_ARGUMENT;
    if(evidence_tx->version!=1u ||
       evidence_tx->type!=STN_TX_BLOCK_COMPENSATION_EVIDENCE ||
       evidence_tx->record_bytes==NULL ||
       evidence_tx->record_length!=STN_TX_BLOCK_COMPENSATION_SIZE)return STN_DATA_TYPE;
    if(issuance_tx->version!=1u || issuance_tx->type!=STN_TX_ISSUANCE ||
       issuance_tx->record_bytes==NULL ||
       issuance_tx->record_length!=STN_TX_ISSUANCE_SIZE)return STN_DATA_TYPE;

    status=stn_block_compensation_decode(evidence_tx->record_bytes,
        evidence_tx->record_length,&evidence);
    if(status!=STN_DATA_OK)return status;
    status=stn_issuance_decode(issuance_tx->record_bytes,
        issuance_tx->record_length,&issuance);
    if(status!=STN_DATA_OK)return status;
    if(issuance.reason!=STN_ISSUANCE_REASON_BLOCK)return STN_DATA_TYPE;

    status=stn_block_compensation_accept(replay,&evidence,&issuance,
        compensation,economy);
    if(status!=STN_DATA_OK)return status;
    if(accepted_evidence!=NULL)*accepted_evidence=evidence;
    return STN_DATA_OK;
}

static size_t pending_find(const stn_pending *pool,const uint8_t id[32])
{
    size_t at=0u;
    while(at<pool->count && memcmp(pool->entries[at].id,id,32u)<0)++at;
    return at;
}

static stn_pending_result pending_insert_block(stn_pending *pool,
    const uint8_t *bytes,size_t length,const stn_hash_provider *hash,
    const uint8_t signer[32],const uint8_t nonce[32],uint8_t id[32])
{
    stn_pending_entry entry={0};size_t at;
    if(pool==NULL || bytes==NULL || hash==NULL || id==NULL ||
       signer==NULL || nonce==NULL)return STN_PENDING_INVALID;
    if(stn_transaction_id(bytes,length,hash,entry.id)!=STN_DATA_OK)
        return STN_PENDING_PROVIDER;
    at=pending_find(pool,entry.id);
    if(at<pool->count && memcmp(pool->entries[at].id,entry.id,32u)==0){
        memcpy(id,entry.id,32u);return STN_PENDING_DUPLICATE;
    }
    if(pool->count>=STN_PENDING_MAX_ENTRIES ||
       length>STN_PENDING_MAX_BYTES-pool->bytes)return STN_PENDING_CAPACITY;
    entry.transaction=(uint8_t*)malloc(length);
    if(entry.transaction==NULL)return STN_PENDING_CAPACITY;
    memcpy(entry.transaction,bytes,length);entry.length=length;
    memcpy(entry.signer,signer,32u);memcpy(entry.nonce,nonce,32u);
    memmove(pool->entries+at+1u,pool->entries+at,
        (pool->count-at)*sizeof(entry));
    pool->entries[at]=entry;++pool->count;pool->bytes+=length;
    memcpy(id,entry.id,32u);return STN_PENDING_ACCEPTED;
}

static int accepted_block(const stn_storage_view *active,const uint8_t block_id[32])
{
    stn_hash_provider hash={stn_sha256,NULL};size_t i;uint8_t id[32];
    if(active==NULL || block_id==NULL)return 0;
    for(i=0u;i<active->count;++i){
        if(stn_chain_block_id(active->blocks[i].bytes,active->blocks[i].length,
            &hash,id)!=STN_DATA_OK)return 0;
        if(memcmp(id,block_id,32u)==0)return 1;
    }
    return 0;
}

static int pending_block_evidence(const stn_pending *pool,
    const stn_issuance_record *issuance,const stn_compensation_state *compensation,
    stn_block_compensation_evidence *matched)
{
    size_t i;
    for(i=0u;i<pool->count;++i){
        stn_transaction tx;stn_block_compensation_evidence evidence;
        if(stn_transaction_decode(pool->entries[i].transaction,
            pool->entries[i].length,&tx)!=STN_DATA_OK)continue;
        if(tx.type!=STN_TX_BLOCK_COMPENSATION_EVIDENCE)continue;
        if(stn_block_compensation_decode(tx.record_bytes,tx.record_length,
            &evidence)!=STN_DATA_OK)continue;
        if(stn_issuance_bind_block(issuance,&evidence,compensation)==STN_DATA_OK){
            if(matched!=NULL)*matched=evidence;return 1;
        }
    }
    return 0;
}

stn_pending_result stn_block_compensation_pending_admit(
    stn_pending *pool,const uint8_t *bytes,size_t length,
    const stn_validation_context *context,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32])
{
    stn_transaction tx;stn_pending_result result;
    if(report==NULL || id==NULL)return STN_PENDING_PROVIDER;
    memset(report,0,sizeof(*report));memset(id,0,32u);
    if(stn_transaction_decode(bytes,length,&tx)!=STN_DATA_OK)
        return stn_pending_admit_transaction(pool,bytes,length,context,active,
            hash,report,id);

    if(tx.type==STN_TX_BLOCK_COMPENSATION_EVIDENCE){
        stn_block_compensation_evidence evidence;
        if(pool==NULL || active==NULL || hash==NULL ||
           stn_block_compensation_decode(tx.record_bytes,tx.record_length,
               &evidence)!=STN_DATA_OK ||
           !accepted_block(active,evidence.block_id)){
            report->acceptance=STN_ACCEPTANCE_REJECTED;return STN_PENDING_INVALID;
        }
        result=pending_insert_block(pool,bytes,length,hash,
            evidence.miner.identifier,evidence.block_id,id);
        report->structure=result==STN_PENDING_ACCEPTED?STN_STAGE_PASS:STN_STAGE_REJECT;
        report->acceptance=result==STN_PENDING_ACCEPTED?
            STN_ACCEPTANCE_UNDER_CONTEXT:STN_ACCEPTANCE_REJECTED;
        return result;
    }

    if(tx.type==STN_TX_ISSUANCE){
        stn_issuance_record issuance;stn_block_compensation_evidence evidence;
        if(stn_issuance_decode(tx.record_bytes,tx.record_length,&issuance)==STN_DATA_OK &&
           issuance.reason==STN_ISSUANCE_REASON_BLOCK){
            if(pool==NULL || active==NULL || hash==NULL ||
               active->state.compensation==NULL ||
               !pending_block_evidence(pool,&issuance,active->state.compensation,
                   &evidence)){
                report->acceptance=STN_ACCEPTANCE_UNRESOLVED;
                return STN_PENDING_UNAVAILABLE;
            }
            result=pending_insert_block(pool,bytes,length,hash,
                issuance.destination.mining_identity.identifier,
                issuance.evidence_id,id);
            report->structure=result==STN_PENDING_ACCEPTED?STN_STAGE_PASS:STN_STAGE_REJECT;
            report->acceptance=result==STN_PENDING_ACCEPTED?
                STN_ACCEPTANCE_UNDER_CONTEXT:STN_ACCEPTANCE_REJECTED;
            return result;
        }
    }

    return stn_pending_admit_transaction(pool,bytes,length,context,active,
        hash,report,id);
}

static int replayed_block_compensation(const stn_storage_view *active,
    const stn_block_compensation_evidence *evidence)
{
    uint8_t id[STN_BLOCK_COMPENSATION_ID_SIZE];size_t i;
    if(active==NULL || active->state.block_compensation==NULL || evidence==NULL)
        return 0;
    if(stn_block_compensation_id(evidence,id)!=STN_DATA_OK)return 1;
    for(i=0u;i<active->state.block_compensation->count;++i)
        if(memcmp(active->state.block_compensation->ids[i],id,sizeof(id))==0)return 1;
    return 0;
}

static int block_issuance_type(const stn_pending_entry *entry)
{
    stn_transaction tx;stn_issuance_record issuance;
    if(stn_transaction_decode(entry->transaction,entry->length,&tx)!=STN_DATA_OK)
        return 0;
    return tx.type==STN_TX_ISSUANCE &&
        stn_issuance_decode(tx.record_bytes,tx.record_length,&issuance)==STN_DATA_OK &&
        issuance.reason==STN_ISSUANCE_REASON_BLOCK;
}

static stn_data_status append_transaction(uint8_t *body,size_t capacity,
    size_t *written,uint32_t *count,const stn_pending_entry *entry)
{
    size_t at=*written;uint32_t n;
    if(entry->length>UINT32_MAX || entry->length+4u>capacity-at ||
       *count>=STN_BLOCK_MAX_TRANSACTIONS)return STN_DATA_CAPACITY;
    n=(uint32_t)entry->length;
    body[at]=(uint8_t)(n>>24);body[at+1u]=(uint8_t)(n>>16);
    body[at+2u]=(uint8_t)(n>>8);body[at+3u]=(uint8_t)n;
    memcpy(body+at+4u,entry->transaction,entry->length);
    *written=at+4u+entry->length;++*count;return STN_DATA_OK;
}

static int pair_fits(size_t written,uint32_t count,size_t capacity,
    const stn_pending_entry *evidence,const stn_pending_entry *issuance)
{
    size_t first,second;
    if(evidence==NULL || issuance==NULL)return 0;
    if(evidence->length>UINT32_MAX || issuance->length>UINT32_MAX)return 0;
    if(count>STN_BLOCK_MAX_TRANSACTIONS-2u)return 0;
    if(evidence->length>SIZE_MAX-4u || issuance->length>SIZE_MAX-4u)return 0;
    first=evidence->length+4u;second=issuance->length+4u;
    if(first>capacity-written)return 0;
    return second<=capacity-written-first;
}

stn_data_status stn_block_compensation_pending_assemble(
    const stn_pending *pool,const stn_validation_context *context,
    const stn_storage_view *active,uint8_t *body,size_t capacity,
    size_t *written,uint32_t *count)
{
    stn_pending generic={0};size_t i,j;stn_data_status status;
    if(written!=NULL)*written=0u;if(count!=NULL)*count=0u;
    if(pool==NULL || active==NULL || body==NULL || written==NULL || count==NULL)
        return STN_DATA_ARGUMENT;

    /* Borrow generic entries without mutating or freeing the source pool. */
    for(i=0u;i<pool->count;++i){
        stn_transaction tx;
        if(stn_transaction_decode(pool->entries[i].transaction,
            pool->entries[i].length,&tx)!=STN_DATA_OK)return STN_DATA_CONTENT;
        if(tx.type==STN_TX_BLOCK_COMPENSATION_EVIDENCE ||
           block_issuance_type(&pool->entries[i]))continue;
        generic.entries[generic.count++]=pool->entries[i];
        generic.bytes+=pool->entries[i].length;
    }

    status=stn_pending_assemble(&generic,context,active,body,capacity,written,count);
    if(status!=STN_DATA_OK)return status;

    /* BLOCK compensation is consensus-atomic. Append an eligible evidence and
     * its bound issuance only when the complete pair fits. A full candidate is
     * not an RPC failure: the untouched pair remains pending for a later block. */
    for(i=0u;i<pool->count;++i){
        stn_transaction evidence_tx;stn_block_compensation_evidence evidence;
        const stn_pending_entry *issuance_entry=NULL;
        if(stn_transaction_decode(pool->entries[i].transaction,
            pool->entries[i].length,&evidence_tx)!=STN_DATA_OK)return STN_DATA_CONTENT;
        if(evidence_tx.type!=STN_TX_BLOCK_COMPENSATION_EVIDENCE)continue;
        if(stn_block_compensation_decode(evidence_tx.record_bytes,
            evidence_tx.record_length,&evidence)!=STN_DATA_OK)return STN_DATA_CONTENT;
        if(!accepted_block(active,evidence.block_id) ||
           replayed_block_compensation(active,&evidence))continue;

        for(j=0u;j<pool->count;++j){
            stn_transaction issuance_tx;stn_issuance_record issuance;
            if(stn_transaction_decode(pool->entries[j].transaction,
                pool->entries[j].length,&issuance_tx)!=STN_DATA_OK)return STN_DATA_CONTENT;
            if(issuance_tx.type!=STN_TX_ISSUANCE ||
               stn_issuance_decode(issuance_tx.record_bytes,
                   issuance_tx.record_length,&issuance)!=STN_DATA_OK ||
               issuance.reason!=STN_ISSUANCE_REASON_BLOCK)continue;
            if(stn_issuance_bind_block(&issuance,&evidence,
                active->state.compensation)==STN_DATA_OK){
                issuance_entry=&pool->entries[j];break;
            }
        }
        if(issuance_entry==NULL)continue;
        if(!pair_fits(*written,*count,capacity,&pool->entries[i],issuance_entry))
            continue;
        status=append_transaction(body,capacity,written,count,&pool->entries[i]);
        if(status!=STN_DATA_OK)return status;
        status=append_transaction(body,capacity,written,count,issuance_entry);
        if(status!=STN_DATA_OK)return status;
    }
    return STN_DATA_OK;
}
