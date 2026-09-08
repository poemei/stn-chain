/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_MINING_H
#define STN_MINING_H
#include "stn_node_service.h"
#include "stn_storage.h"
/* Explicit operator-selected canonical body, not a mempool/selection policy.
 * Caller serializes calls and keeps inputs immutable. All buffers and inputs
 * must be disjoint. Default scratch remains caller-owned and never reallocates.
 * owns_buffers explicitly opts in to realloc of malloc-owned scratch; provider
 * read must report required bytes on CAPACITY for runtime resizing. Scratch is unpublished on failure; written remains zero.
 * Requests originate from validated RPC dispatch, or identical well-formed
 * internal messages. Work submission defensively checks its nested shape.
 * Production SHA-256 is required. Active state changes only after persistence.
 * A snapshot is freshly loaded for each call; no process-local work cache. */
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
    int owns_buffers; /* Opt in only for malloc/realloc-owned scratch. */
} stn_mining_service;
#define STN_MINING_PREFIX 68u
#define STN_MINING_NONCE_OFFSET 152u
#define STN_MINING_NONCE_SIZE 8u
/* Same 68-byte prefix + block for template response and solved request.
 * Only block bytes 152..159 may change: unsigned big-endian nonce. */
stn_rpc_code stn_mining_handle(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);
#endif
