/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share_pending.h"
#include "stn_share.h"
#include "stn_issuance.h"
#include "stn_issuance_binding.h"
#include "stn_block_reward.h"
#include <string.h>

static int pending_share_matches(const stn_pending *pool,
    const stn_issuance_record *issuance,const stn_compensation_state *compensation)
{
    size_t i;
    if(pool==NULL || issuance==NULL || compensation==NULL)return 0;
    for(i=0u;i<pool->count;++i){
        stn_transaction tx;stn_share_evidence share;
        if(stn_transaction_decode(pool->entries[i].transaction,
            pool->entries[i].length,&tx)!=STN_DATA_OK)continue;
        if(tx.type!=STN_TX_SHARE_EVIDENCE)continue;
        if(stn_share_decode(tx.record_bytes,tx.record_length,&share)!=STN_DATA_OK)continue;
        if(stn_issuance_bind_share(issuance,&share,compensation)==STN_DATA_OK)return 1;
    }
    return 0;
}

stn_pending_result stn_share_pending_admit(
    stn_pending *pool,const uint8_t *bytes,size_t length,
    const stn_validation_context *context,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32])
{
    stn_transaction tx;stn_pending_result result;
    if(report==NULL || id==NULL)return STN_PENDING_PROVIDER;
    if(stn_transaction_decode(bytes,length,&tx)!=STN_DATA_OK)
        return stn_block_compensation_pending_admit(pool,bytes,length,context,
            active,hash,report,id);

    if(tx.type==STN_TX_SHARE_EVIDENCE){
        size_t issuance_length=STN_TX_HEADER_SIZE+STN_ISSUANCE_CANONICAL_SIZE;
        if(pool==NULL)return STN_PENDING_UNAVAILABLE;
        size_t reward_length=STN_BLOCK_REWARD_EVIDENCE_TX_SIZE+
            STN_BLOCK_REWARD_ISSUANCE_TX_SIZE;
        /* Keep the share pair atomic and leave room for a solved block's
         * reward pair. Current-height shares cannot enter that block, so a
         * full share queue must not cause its durable acceptance to return
         * CAPACITY while constructing the reward. */
        if(pool->count>STN_PENDING_MAX_ENTRIES-4u ||
           length>STN_PENDING_MAX_BYTES-pool->bytes ||
           issuance_length>STN_PENDING_MAX_BYTES-pool->bytes-length ||
           reward_length>STN_PENDING_MAX_BYTES-pool->bytes-length-issuance_length){
            memset(report,0,sizeof(*report));
            report->acceptance=STN_ACCEPTANCE_REJECTED;
            return STN_PENDING_CAPACITY;
        }
        return stn_block_compensation_pending_admit(pool,bytes,length,context,
            active,hash,report,id);
    }

    if(tx.type==STN_TX_ISSUANCE){
        stn_issuance_record issuance;
        if(stn_issuance_decode(tx.record_bytes,tx.record_length,&issuance)==STN_DATA_OK &&
           issuance.reason==STN_ISSUANCE_REASON_SHARE && pool!=NULL && active!=NULL &&
           active->state.compensation!=NULL &&
           pending_share_matches(pool,&issuance,active->state.compensation)){
            memset(report,0,sizeof(*report));
            result=stn_pending_insert(pool,bytes,length,hash,id);
            report->structure=result==STN_PENDING_ACCEPTED?STN_STAGE_PASS:STN_STAGE_REJECT;
            report->acceptance=result==STN_PENDING_ACCEPTED?
                STN_ACCEPTANCE_UNDER_CONTEXT:STN_ACCEPTANCE_REJECTED;
            return result;
        }
    }

    return stn_block_compensation_pending_admit(pool,bytes,length,context,
        active,hash,report,id);
}
