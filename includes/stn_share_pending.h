/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SHARE_PENDING_H
#define STN_SHARE_PENDING_H
#include "stn_block_compensation_candidate.h"

/* Mining admission wrapper. A qualifying share reserves room for its SHARE
 * issuance before mutation. SHARE issuance may bind to matching evidence
 * already pending, but candidate eligibility remains governed by accepted
 * compensation/evidence ordering. Other transaction classes retain the block
 * compensation-aware admission path. */
stn_pending_result stn_share_pending_admit(
    stn_pending *pool,const uint8_t *transaction,size_t length,
    const stn_validation_context *context,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32]);

#endif
