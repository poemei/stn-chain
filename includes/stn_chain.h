/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CHAIN_H
#define STN_CHAIN_H
#include "stn_block.h"
#include "stn_validation.h"
#include "stn_pow.h"
#include "stn_authority.h"
#include "stn_lifecycle.h"

#define STN_CHAIN_MAX_BATCH 64u

typedef struct stn_chain_context {
    uint8_t network_id[32];
    const uint8_t *genesis_bytes; /* Exact externally selected development anchor. */
    size_t genesis_length;
    /* Canonical genesis-declared authority roots, sorted public keys. */
    const uint8_t *genesis_authority_roots;
    size_t genesis_authority_root_count;
    const uint8_t *genesis_initial_identities;
    size_t genesis_initial_identity_count;
    /* Retained source compatibility only; not a consensus switch. Production
     * publication rules are selected by canonical genesis and candidate height. */
    const stn_validation_context *publication_validation;
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
    /* Derived from validated blocks only; never serialized as trusted state.
     * Current window's calculation fields (version/height/time/target). */
    stn_block_header target_history[60];
    size_t target_history_count;
    stn_lifecycle_state *lifecycle;
    uint64_t publication_activation_height;
} stn_chain_state;
/* One local reference per owning state. Plain structure copies are BORROWS,
 * never independent owners. share writes a fresh output; move replaces a
 * zero-initialized/owned destination and clears source. release clears state.
 * Borrowed states must not be released or replaced in place; they cannot outlive
 * the owner. Lifecycle pointers originate only from Chain initialization/
 * validation and must not be assigned independently. move requires non-NULL
 * arguments; self-share/self-move are no-ops. Ownership calls on the same
 * snapshot require external serialization.
 * initialize/validation outputs must be fresh (released first); validation also
 * supports replacing its owning prior in place, only after success. */
stn_data_status stn_chain_state_share(const stn_chain_state *source,stn_chain_state *out);
void stn_chain_state_move(stn_chain_state *out,stn_chain_state *source);
void stn_chain_state_release(stn_chain_state *state);
#ifdef STN_LIFECYCLE_TEST
size_t stn_chain_test_live_snapshots(void);
size_t stn_chain_test_clone_count(void);
void stn_chain_test_fail_after(size_t budget);
/* Test-only counter boundary injection; restore the true count before release. */
size_t stn_chain_test_references(stn_chain_state *state,size_t references);
#endif

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

#define STN_CHAIN_CURSOR_SIZE 45u
#define STN_CHAIN_CURSOR_VERSION 1u
typedef struct stn_chain_cursor {
    uint8_t version;
    uint64_t height;
    uint8_t block_id[32];
    uint32_t transaction_position;
} stn_chain_cursor;
typedef enum stn_cursor_result {
    STN_CURSOR_VALID=0, STN_CURSOR_DETACHED, STN_CURSOR_MALFORMED,
    STN_CURSOR_UNAVAILABLE, STN_CURSOR_PROVIDER
} stn_cursor_result;
/* Exact-size buffers; failures leave output unchanged. Inputs/output disjoint.
 * All uint64 heights and uint32 positions are structurally representable. */
stn_cursor_result stn_chain_cursor_encode(const stn_chain_cursor *cursor,
    uint8_t *bytes,size_t length);
stn_cursor_result stn_chain_cursor_decode(const uint8_t *bytes,size_t length,
    stn_chain_cursor *cursor);

typedef struct stn_block_span { const uint8_t *bytes; size_t length; } stn_block_span;

/* Caller holds the current immutable accepted history, including genesis.
 * Revalidates the entire history. Missing location (including a position beyond
 * the block's transaction count) is DETACHED, not malformed cursor syntax.
 * Unavailable/invalid history or provider failure is not evidence of detachment.
 * No eligibility, enumeration, mutation, or consumer recovery is performed. */
stn_cursor_result stn_chain_cursor_validate(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const stn_chain_cursor *cursor);

/* In-memory result, not a wire format. On a match, transaction borrows exact
 * canonical bytes from the supplied immutable accepted history. */
typedef struct stn_chain_record_match {
    int found;
    uint8_t record_id[32];
    uint64_t height;
    uint8_t block_id[32];
    stn_transaction_span transaction;
} stn_chain_record_match;

/* Revalidate the complete accepted history, then select the earliest eligible
 * production record using the qualified historical authority/replay rules.
 * OK with found=0 means absence; failures leave out unchanged. Inputs and out
 * must be disjoint. Caller selects/holds the immutable accepted snapshot and
 * keeps it alive while using the borrowed transaction. No pending, persistence
 * or accepted-state mutation. Temporary eligibility storage is call-local. */
stn_data_status stn_chain_lookup_record(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const uint8_t record_id[32],
    stn_chain_record_match *out);

typedef enum stn_next_result {
    STN_NEXT_RECORD=0, STN_NEXT_END, STN_NEXT_DETACHED, STN_NEXT_MALFORMED,
    STN_NEXT_UNAVAILABLE, STN_NEXT_PROVIDER, STN_NEXT_CAPACITY
} stn_next_result;
/* Exclusive traversal from an existing valid position. Uses historical lookup
 * eligibility, including all replay evidence before the cursor. No record-ID
 * deduplication. Output assigned only for NEXT_RECORD; encode position with the
 * existing 45-byte cursor codec. Borrowed transaction lifetime matches lookup.
 * All inputs/outputs disjoint and immutable history held by caller. */
stn_next_result stn_chain_next_record(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,const stn_chain_cursor *after,
    stn_chain_record_match *out,stn_chain_cursor *position);
typedef enum stn_first_result {
    STN_FIRST_RECORD=0, STN_FIRST_END, STN_FIRST_ARGUMENT,
    STN_FIRST_UNAVAILABLE, STN_FIRST_PROVIDER, STN_FIRST_CAPACITY
} stn_first_result;
/* Search from the beginning of current accepted history, without a cursor.
 * Same eligibility, borrowed-byte lifetime and disjointness rules as next.
 * Only FIRST_RECORD assigns outputs. END means no eligible accepted record;
 * absent/unavailable history is a separate failure, not END. */
stn_first_result stn_chain_first_record(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,stn_chain_record_match *out,
    stn_chain_cursor *position);
/* Context/input spans and provider state must stay immutable during calls.
 * Lifecycle snapshots are locally allocated; no persistence or ambient time.
 * The initial state is EMPTY, not an accepted genesis. Anchor validity beyond
 * structure is checked when the exact genesis is submitted as a candidate.
 * Output unchanged on failure. Existing accepted state must originate from
 * this validation path; never deserialize metadata and assume it is trusted. */
stn_data_status stn_chain_initialize(const stn_chain_context *context, stn_chain_state *out);
stn_data_status stn_chain_publication_activation(const stn_chain_context *context,
    uint64_t *height);
/* Same branch-derived rule for validation and mining. Unchanged on failure. */
stn_data_status stn_chain_required_target(const stn_chain_context *context,
    const stn_chain_state *prior,uint8_t target[32]);

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

/* Full-history reconstruction owns its private evolving state. It can reuse
 * exclusive lifecycle storage internally; any failure destroys the entire
 * temporary reconstruction and leaves out unchanged. No trusted state input.
 * Successful out must be fresh, with the same ownership rule as validation.
 * No STN_CHAIN_MAX_BATCH limit: callers bound the supplied history resources. */
stn_chain_report stn_chain_reconstruct_history(const stn_chain_context *context,
    const stn_block_span *blocks,size_t count,stn_chain_state *out);

/* Validate a suffix from a previously validated prefix. To validate an entire
 * untrusted chain, initialize EMPTY then supply all blocks including genesis.
 * At most 64 blocks per call. Empty batches validate context/prior and are a
 * no-op success; no genesis/block is thereby accepted. Output only assigned
 * after the entire batch passes. Stops at first failure in input order. */
stn_chain_report stn_chain_validate_sequence(const stn_chain_context *context,
    const stn_chain_state *prior, const stn_block_span *blocks, size_t count,
    stn_chain_state *out);
#endif


