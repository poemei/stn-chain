/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_REWARD_H
#define STN_BLOCK_REWARD_H

#include "stn_block_compensation.h"
#include "stn_compensation_state.h"
#include "stn_issuance.h"
#include "stn_transaction.h"

#define STN_BLOCK_REWARD_EVIDENCE_TX_SIZE (STN_TX_HEADER_SIZE + STN_BLOCK_COMPENSATION_CANONICAL_SIZE)
#define STN_BLOCK_REWARD_ISSUANCE_TX_SIZE (STN_TX_HEADER_SIZE + STN_ISSUANCE_CANONICAL_SIZE)

/* Build the canonical economic consequence of one accepted solved block.
 * The caller supplies the accepted block ID and submitting stn0_ identity.
 * The accepted compensation state supplies the exact stnw0_ destination.
 * The canonical block compensation is 100 STNC (10000 centi-STNC units).
 * No state is mutated and no issuance is accepted by this primitive. */
stn_data_status stn_block_reward_build(
    const uint8_t block_id[STN_BLOCK_COMPENSATION_BLOCK_ID_SIZE],
    const stn_address *miner,
    const stn_compensation_state *compensation,
    stn_block_compensation_evidence *evidence,
    stn_issuance_record *issuance);

/* Encode the paired canonical transactions produced by stn_block_reward_build.
 * Evidence is emitted first; issuance is emitted second. These bytes remain
 * candidate evidence only until normal Chain validation accepts them. */
stn_data_status stn_block_reward_encode_transactions(
    const stn_block_compensation_evidence *evidence,
    const stn_issuance_record *issuance,
    uint8_t *evidence_tx,size_t evidence_capacity,size_t *evidence_written,
    uint8_t *issuance_tx,size_t issuance_capacity,size_t *issuance_written);

#endif
