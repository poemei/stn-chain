/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share_pending.h"
#include <string.h>

stn_pending_result stn_share_pending_admit_pair(
    stn_pending *pool,
    const uint8_t *share_transaction,size_t share_length,
    const uint8_t *issuance_transaction,size_t issuance_length,
    const stn_hash_provider *hash,
    uint8_t share_transaction_id[32],
    uint8_t issuance_transaction_id[32])
{
    stn_pending_result share_result,issuance_result;
    uint8_t share_id[32],issuance_id[32];

    if(pool==NULL || share_transaction==NULL || issuance_transaction==NULL ||
       hash==NULL || share_transaction_id==NULL || issuance_transaction_id==NULL){
        return STN_PENDING_INVALID;
    }

    /* A share pair consumes exactly two entries. Refuse before mutation when
     * the bounded pool cannot retain both sides. */
    if(pool->count>STN_PENDING_MAX_ENTRIES-2u ||
       share_length>STN_PENDING_MAX_BYTES-pool->bytes ||
       issuance_length>STN_PENDING_MAX_BYTES-pool->bytes-share_length){
        return STN_PENDING_CAPACITY;
    }

    share_result=stn_pending_insert(
        pool,share_transaction,share_length,hash,share_id);
    if(share_result!=STN_PENDING_ACCEPTED){
        return share_result;
    }

    issuance_result=stn_pending_insert(
        pool,issuance_transaction,issuance_length,hash,issuance_id);
    if(issuance_result!=STN_PENDING_ACCEPTED){
        (void)stn_pending_remove(pool,share_id);
        return issuance_result;
    }

    memcpy(share_transaction_id,share_id,32u);
    memcpy(issuance_transaction_id,issuance_id,32u);
    return STN_PENDING_ACCEPTED;
}
