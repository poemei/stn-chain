/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_COMPENSATION_CANDIDATE_H
#define STN_BLOCK_COMPENSATION_CANDIDATE_H

#include "stn_block_compensation_acceptance.h"
#include "stn_transaction.h"

/* Validate and apply the adjacent canonical block-compensation evidence and
 * issuance transactions as one deterministic candidate-state transition.
 * This helper does not establish that block_id belongs to accepted history;
 * the Chain candidate processor remains responsible for that consensus fact. */
stn_data_status stn_block_compensation_candidate_apply(
    const stn_transaction *evidence_tx,
    const stn_transaction *issuance_tx,
    stn_block_compensation_replay *replay,
    const stn_compensation_state *compensation,
    stn_economic_state *economy,
    stn_block_compensation_evidence *accepted_evidence);

#endif
