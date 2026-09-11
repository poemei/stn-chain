/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_LIFECYCLE_H
#define STN_LIFECYCLE_H
#include "stn_chain.h"
#include "stn_authority.h"
#include "stn_replay.h"

#define STN_LIFECYCLE_REPLAY_DOMAIN "STN-CHAIN:LIFECYCLE:REPLAY:1"
#define STN_LIFECYCLE_MAX_GRANT_SIZE STN_AUTHORITY_GRANT_SIZE

typedef enum stn_lifecycle_result {
    STN_LIFECYCLE_OK=0,
    STN_LIFECYCLE_ARGUMENT,
    STN_LIFECYCLE_MALFORMED,
    STN_LIFECYCLE_INVALID,
    STN_LIFECYCLE_REPLAY,
    STN_LIFECYCLE_CAPACITY,
    STN_LIFECYCLE_PROVIDER
} stn_lifecycle_result;

typedef struct stn_lifecycle_state {
    uint8_t *grant_bytes;
    size_t grant_capacity, grant_count;
    stn_authority_state authority;
    stn_identity_rotation_state rotation;
    stn_replay_state replay;
    uint8_t initial_identity[STN_IDENTITY_PUBLIC_KEY_SIZE];
} stn_lifecycle_state;

/* Buffers are caller-owned; no protocol-level grant or replay ceiling is
 * imposed. Accepted history is the only source of lifecycle state. */
void stn_lifecycle_initialize(stn_lifecycle_state *state,
    uint8_t *grant_bytes, size_t grant_capacity, uint8_t *replay_bytes,
    size_t replay_capacity, const uint8_t initial_identity[32]);

stn_lifecycle_result stn_lifecycle_replay_nonce(uint16_t transaction_type,
    const uint8_t *statement, size_t statement_length,
    const stn_hash_provider *provider, uint8_t nonce[32]);

stn_lifecycle_result stn_lifecycle_apply_transaction(stn_lifecycle_state *state,
    const stn_transaction *transaction, const uint8_t *genesis_roots,
    size_t root_count, const stn_hash_provider *provider);

/* Rebuild from blocks already selected as accepted history, in block order and
 * existing transaction order. Output becomes usable only on success. */
stn_lifecycle_result stn_lifecycle_rebuild(stn_lifecycle_state *state,
    const stn_block_span *blocks, size_t block_count,
    const uint8_t *genesis_roots, size_t root_count,
    const stn_hash_provider *provider);

#endif
