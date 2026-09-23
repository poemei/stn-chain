/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_CONSENSUS_H
#define STN_CONTRACT_CONSENSUS_H

#include "stn_contract.h"

#define STN_CONTRACT_VOTE_KEY_SIZE (STN_ADDRESS_ID_SIZE + STN_IDENTITY_PUBLIC_KEY_SIZE)

typedef struct stn_contract_vote_state {
    uint8_t contract_id[STN_ADDRESS_ID_SIZE];
    uint8_t *accepted;
    size_t accepted_count;
    size_t accepted_capacity;
    size_t eligible_count;
    size_t required_count;
} stn_contract_vote_state;

/* Establish immutable Contract identity and majority rule from the exact
 * canonical DRAFT. Eligible voters are unique APPROVER participants.
 * Majority is floor(eligible/2)+1. No votes are accepted here. */
stn_contract_status stn_contract_vote_state_initialize(
    stn_contract_vote_state *state,
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    uint8_t *accepted,
    size_t accepted_capacity);

/* Validate that actor is an APPROVER participant in the immutable DRAFT. */
stn_contract_status stn_contract_vote_eligible(
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE]);

/* Consume one accepted approval vote. The same identity may vote only once
 * for this Contract origin. reached receives 1 only when strict majority has
 * been accumulated. Output/state remain unchanged on failure. */
stn_contract_status stn_contract_vote_accept(
    stn_contract_vote_state *state,
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    int *reached);

/*
 * Apply one consensus-accepted APPROVE vote to Contract state.
 *
 * REVIEW enters APPROVALS while votes accumulate. APPROVALS remains
 * APPROVALS until strict majority is reached. The vote that reaches majority
 * advances the Contract to ATTESTATION. Sequence advances exactly once for
 * every accepted vote. Duplicate/ineligible votes do not mutate either state.
 */
stn_contract_status stn_contract_vote_apply(
    const stn_contract *current,
    uint64_t expected_sequence,
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],
    stn_contract_vote_state *vote_state,
    stn_contract *next);

#endif
