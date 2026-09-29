/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_QUERY_H
#define STN_CONTRACT_QUERY_H

#include "stn_contract_state.h"
#include "stn_address.h"
#include "stn_contract_snapshot.h"
#include "stn_rpc.h"

#define STN_CONTRACT_QUERY_MAX_RESULTS 16u

typedef struct stn_contract_query_result {
    uint8_t contract_id[STN_ADDRESS_ID_SIZE];
    uint16_t state;
    uint16_t type;
    uint64_t sequence;
    uint64_t created_at;
} stn_contract_query_result;

/*
 * Returns contracts containing identity, active/nonterminal first, then newest
 * created_at, then contract identifier lexicographically for deterministic ties.
 * Results are bounded to STN_CONTRACT_QUERY_MAX_RESULTS.
 */
stn_contract_status stn_contract_query_identity(
    const stn_contract_state_store *store,
    const uint8_t identity[STN_ADDRESS_ID_SIZE],
    stn_contract_query_result *results,
    size_t capacity,
    size_t *count);

/* No history replay or pending lookup. Caller holds this validated immutable
 * snapshot for the whole call. Supports CONTRACT_LIST and CONTRACT_STATE. */
stn_rpc_code stn_contract_query_handle(const stn_contract_snapshot *contracts,
    const stn_rpc_message *request,uint8_t *payload,size_t capacity,size_t *written);

#endif
