/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_REWARD_H
#define STN_BLOCK_REWARD_H

#include "stn_block_compensation.h"
#include "stn_compensation_state.h"
#include "stn_issuance.h"

/* Build the canonical economic consequence of one accepted solved block.
 * The caller supplies the accepted block ID and submitting stn0_ identity.
 * The accepted compensation state supplies the exact stnw0_ destination.
 * No state is mutated and no issuance is accepted by this primitive. */
stn_data_status stn_block_reward_build(
    const uint8_t block_id[STN_BLOCK_COMPENSATION_BLOCK_ID_SIZE],
    const stn_address *miner,
    const stn_compensation_state *compensation,
    stn_block_compensation_evidence *evidence,
    stn_issuance_record *issuance);

#endif
