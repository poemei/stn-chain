/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_MINING_H
#define STN_MINING_H
#include "stn_node_service.h"
#include "stn_storage.h"
#include "stn_pending.h"
#include "stn_block_compensation_candidate.h"
/* Mining is the producer of canonical block-compensation evidence and BLOCK
 * issuance. Route its pending admission and candidate assembly through the
 * pairing layer so consensus-required adjacency is preserved without changing
 * generic pending semantics for unrelated callers. */
#define stn_pending_admit_transaction stn_block_compensation_pending_admit
#define stn_pending_assemble stn_block_compensation_pending_assemble
/* Explicit operator-selected canonical body, not a mempool/selection policy.
 * Caller serializes calls and keeps inputs immutable. All buffers and inputs
 * must be disjoint. Default scratch remains caller-owned and never reallocates.
 * owns_buffers explicitly opts in to realloc of malloc-owned scratch; provider
 * read must report required bytes on CAPACITY for runtime resizing. Scratch is unpublished on failure; written remains zero.
 * Requests originate from validated RPC dispatch, or identical well-formed
 * internal messages. Work submission defensively checks its nested shape.
 * Production SHA-256 is required. Active state changes only after persistence.
 * A snapshot is freshly loaded for each call. No authoritative work/template
 * bytes are cached. A non-authoritative timestamp lease is retained only while
 * the exact semantic mining candidate remains unchanged, so repeated reads of
 * the same candidate produce the same immutable Work ID. */
typedef struct stn_suffix_stage {
    uint32_t prefix_count;
    uint32_t expected_count;
    uint32_t received_count;
    size_t received_bytes;
    stn_block_span *blocks;
    uint8_t **owned;
    int active;
} stn_suffix_stage;

typedef struct stn_mining_session {
    struct stn_mining_service *service;
    stn_suffix_stage suffix_stage;
} stn_mining_session;

typedef struct stn_mining_service {
    const stn_chain_context *chain;
    const stn_storage_provider *storage;
    const uint8_t *body;
    size_t body_length;
    uint32_t transaction_count;
    uint8_t *snapshot; size_t snapshot_capacity;
    uint8_t *template_bytes; size_t template_capacity;
    stn_storage_workspace workspace;
    stn_chain_state active;
    stn_pending *pending;
    const stn_validation_context *intelligence;
    uint8_t *pending_body;size_t pending_body_capacity;
    int owns_buffers; /* Opt in only for malloc/realloc-owned scratch. */
    uint64_t (*timestamp_now)(void *user);
    void *timestamp_user;
    uint8_t template_time_base[32];
    uint8_t template_time_target[32];
    uint8_t template_time_commitment[32];
    uint64_t template_time_height;
    uint32_t template_time_transaction_count;
    uint32_t template_time_body_length;
    uint64_t template_time_value;
    int template_time_valid;
    stn_suffix_stage suffix_stage;
} stn_mining_service;
#define STN_MINING_PREFIX 68u
#define STN_MINING_IDENTITY_SIZE STN_RPC_MINER_IDENTITY_SIZE
#define STN_MINING_SUBMISSION_PREFIX STN_RPC_MINING_SUBMISSION_PREFIX
#define STN_MINING_NONCE_OFFSET 152u
#define STN_MINING_NONCE_SIZE 8u
/* Transient staged recovery is deliberately bounded independently of the
 * accepted Chain length. Long recovery may be retried in bounded windows. */
#define STN_SUFFIX_STAGE_MAX_BLOCKS 4096u
#define STN_SUFFIX_STAGE_MAX_BYTES (64u * 1024u * 1024u)
/* Template response remains 68-byte prefix + block. Solved submission is
 * parent[32] + work ID[32] + block length[4] + canonical stn0_ identity[69]
 * + block. Only block bytes 152..159 may change: unsigned big-endian nonce. */
stn_rpc_code stn_mining_handle(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);
/* Bind one instance to one long-lived STNC connection. Staged suffix evidence
 * is connection-owned; accepted Chain state remains shared in service. Caller
 * serializes all session handlers against service mutations. */
void stn_mining_session_init(stn_mining_session *session,stn_mining_service *service);
void stn_mining_session_release(stn_mining_session *session);
stn_rpc_code stn_mining_session_handle(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);

/* Resolve the exact wallet destination for a mining identity from canonical
 * compensation-destination transactions already held in the local pending pool.
 * This helper does not make pending state authoritative. It exists only so a
 * verified qualifying share can retain its deterministic future issuance while
 * the explicit stn0_ -> stnw0_ relationship is waiting to enter accepted
 * history. Conflicting pending destinations fail closed. */
stn_data_status stn_mining_pending_compensation_lookup(const stn_pending *pending,
    const stn_address *mining_identity,stn_address *wallet);

/* In-process callers use the exact same validated mining service path as STNC.
 * These helpers do not bypass storage, pending admission, target validation or
 * consensus. Caller serializes access to the service. */
stn_rpc_code stn_mining_template_local(stn_mining_service *service,
    uint8_t *payload,size_t capacity,size_t *written);
stn_rpc_code stn_mining_submit_share_local(stn_mining_service *service,
    const stn_share_evidence *evidence,uint8_t share_id[STN_SHARE_ID_SIZE]);
stn_rpc_code stn_mining_submit_work_local(stn_mining_service *service,
    const stn_address *miner,const uint8_t parent[32],const uint8_t work_id[32],
    const uint8_t *block,size_t block_length);
#endif
