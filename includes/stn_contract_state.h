/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_STATE_H
#define STN_CONTRACT_STATE_H

#include "stn_contract_consensus.h"
#include "stn_contract_lineage.h"

#define STN_CONTRACT_STATE_MAX_CONTRACTS 128u
#define STN_CONTRACT_STATE_MAX_VOTES     (STN_CONTRACT_STATE_MAX_CONTRACTS * STN_CONTRACT_MAX_PARTICIPANTS)

typedef struct stn_contract_state_entry {
    uint8_t contract_id[STN_ADDRESS_ID_SIZE];
    const uint8_t *canonical_draft;
    size_t canonical_draft_length;
    stn_contract current;
    size_t vote_offset;
    size_t vote_count;
    size_t eligible_count;
    size_t required_count;
} stn_contract_state_entry;

typedef struct stn_contract_state_store {
    stn_contract_state_entry *entries;
    size_t entry_count;
    size_t entry_capacity;
    uint8_t *votes;
    size_t vote_count;
    size_t vote_capacity;
} stn_contract_state_store;

void stn_contract_state_initialize(
    stn_contract_state_store *state,
    stn_contract_state_entry *entries,
    size_t entry_capacity,
    uint8_t *votes,
    size_t vote_capacity);

stn_contract_status stn_contract_state_register(
    stn_contract_state_store *state,
    const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    size_t *index);

stn_contract_status stn_contract_state_find(
    const stn_contract_state_store *state,
    const uint8_t contract_id[STN_ADDRESS_ID_SIZE],
    size_t *index);

stn_contract_status stn_contract_state_apply_vote(
    stn_contract_state_store *state,
    size_t index,
    const uint8_t *canonical_current,
    size_t canonical_current_length,
    uint64_t expected_sequence,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE]);

#endif
