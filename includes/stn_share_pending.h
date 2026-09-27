/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_SHARE_PENDING_H
#define STN_SHARE_PENDING_H
#include "stn_pending.h"

/* Admit a qualifying share evidence transaction and its canonical SHARE
 * issuance as one local pending operation. The pool is unchanged unless both
 * entries can be retained. This is pending bookkeeping only; normal candidate
 * eligibility still enforces accepted compensation and evidence ordering. */
stn_pending_result stn_share_pending_admit_pair(
    stn_pending *pool,
    const uint8_t *share_transaction,size_t share_length,
    const uint8_t *issuance_transaction,size_t issuance_length,
    const stn_hash_provider *hash,
    uint8_t share_transaction_id[32],
    uint8_t issuance_transaction_id[32]);

#endif
