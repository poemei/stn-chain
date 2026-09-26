/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_COMPENSATION_ACCEPTANCE_H
#define STN_BLOCK_COMPENSATION_ACCEPTANCE_H

#include "stn_block_compensation_replay.h"
#include "stn_compensation_state.h"
#include "stn_economic_state.h"
#include "stn_issuance_binding.h"

/* Apply the deterministic economic consequence of accepted solved-block
 * evidence. Evidence replay, issuance binding, compensation destination and
 * economic state are validated as one atomic transition. On failure both
 * replay and economic state remain unchanged. */
stn_data_status stn_block_compensation_accept(
    stn_block_compensation_replay *replay,
    const stn_block_compensation_evidence *evidence,
    const stn_issuance_record *issuance,
    const stn_compensation_state *compensation,
    stn_economic_state *economy);

#endif
