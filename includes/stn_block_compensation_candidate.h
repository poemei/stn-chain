/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_BLOCK_COMPENSATION_CANDIDATE_H
#define STN_BLOCK_COMPENSATION_CANDIDATE_H

#include "stn_block_compensation_acceptance.h"
#include "stn_pending.h"
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

/* Mining-side pending integration for block compensation. Generic pending
 * admission predates BLOCK issuance and generic candidate assembly sorts by
 * transaction ID, which cannot preserve the consensus-required adjacent
 * evidence -> issuance pair. These wrappers preserve the generic path for all
 * other transactions while admitting and assembling BLOCK compensation as one
 * deterministic pair. */
stn_pending_result stn_block_compensation_pending_admit(
    stn_pending *pool,const uint8_t *transaction,size_t length,
    const stn_validation_context *context,const stn_storage_view *active,
    const stn_hash_provider *hash,stn_validation_report *report,uint8_t id[32]);

stn_data_status stn_block_compensation_pending_assemble(
    const stn_pending *pool,const stn_validation_context *context,
    const stn_storage_view *active,uint8_t *body,size_t capacity,
    size_t *written,uint32_t *count);

#endif
