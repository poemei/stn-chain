/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CHAIN_H
#define STN_CHAIN_H
#include "stn_block.h"
#include "stn_validation.h"
#include "stn_pow.h"

#define STN_CHAIN_MAX_BATCH 64u

typedef struct stn_chain_context {
    uint8_t network_id[32];
    const uint8_t *genesis_bytes; /* Exact externally selected development anchor. */
    size_t genesis_length;
    stn_hash_provider hash_provider;
    /* NULL selects legacy v1 development-only profile (no PoW/work).
     * Non-NULL strictly requires v3 blocks, including genesis. No downgrade. */
    const stn_pow_policy *pow_policy;
} stn_chain_context;

typedef struct stn_chain_state {
    uint8_t network_id[32];
    uint8_t genesis_id[32];
    uint8_t tip_id[32];
    uint64_t height;
    uint64_t timestamp;
    int has_tip; /* 0 for canonical empty state, 1 for validated prefix. */
    uint8_t current_target[32];
    stn_work cumulative_work;
} stn_chain_state;

typedef enum stn_chain_reason {
    STN_CHAIN_NONE = 0, STN_CHAIN_ARGUMENT, STN_CHAIN_CONTEXT,
    STN_CHAIN_STATE, STN_CHAIN_STRUCTURE, STN_CHAIN_NETWORK,
    STN_CHAIN_GENESIS, STN_CHAIN_HEIGHT, STN_CHAIN_PARENT,
    STN_CHAIN_TIMESTAMP, STN_CHAIN_BODY, STN_CHAIN_HASH,
    STN_CHAIN_ZERO_ID, STN_CHAIN_BATCH_LIMIT, STN_CHAIN_TARGET,
    STN_CHAIN_POW, STN_CHAIN_WORK
} stn_chain_reason;

typedef struct stn_chain_report {
    stn_stage_status structure, link, body, identifier, pow;
    stn_stage_status target, work;
    stn_acceptance acceptance;
    stn_chain_reason reason;
    stn_data_status detail;
    size_t failing_index; /* SIZE_MAX when no input block failed. */
    uint64_t failing_height;
    int height_available; /* Only a structurally decoded header supplies height. */
} stn_chain_report;

typedef struct stn_block_span { const uint8_t *bytes; size_t length; } stn_block_span;

/* Context/input spans and provider state must stay immutable during calls.
 * No allocation, persistence, global state, ambient time, or replay database.
 * The initial state is EMPTY, not an accepted genesis. Anchor validity beyond
 * structure is checked when the exact genesis is submitted as a candidate.
 * Output unchanged on failure. Existing accepted state must originate from
 * this validation path; never deserialize metadata and assume it is trusted. */
stn_data_status stn_chain_initialize(const stn_chain_context *context, stn_chain_state *out);

/* Block ID = configured SHA-256 provider(domain || 168-byte canonical header).
 * Full structural validation first; integrity remains a separate requirement.
 * digest output must not overlap inputs; unchanged on failure. */
stn_data_status stn_chain_block_id(const uint8_t *bytes, size_t length,
    const stn_hash_provider *provider, uint8_t digest[32]);

/* Prior/output may be the same object. No other input/output overlap allowed.
 * All required local-development stages must pass before output assignment.
 * PoW/work are required in the v3 policy profile; NOT_RUN only in legacy v1.
 * UNDER_CONTEXT is local linkage/integrity only, NOT distributed consensus,
 * signature/authority acceptance, or global transaction replay safety. */
stn_chain_report stn_chain_validate_candidate(const stn_chain_context *context,
    const stn_chain_state *prior, const uint8_t *bytes, size_t length, stn_chain_state *out);

/* Validate a suffix from a previously validated prefix. To validate an entire
 * untrusted chain, initialize EMPTY then supply all blocks including genesis.
 * At most 64 blocks per call. Empty batches validate context/prior and are a
 * no-op success; no genesis/block is thereby accepted. Output only assigned
 * after the entire batch passes. Stops at first failure in input order. */
stn_chain_report stn_chain_validate_sequence(const stn_chain_context *context,
    const stn_chain_state *prior, const stn_block_span *blocks, size_t count,
    stn_chain_state *out);
#endif
