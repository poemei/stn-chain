/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_NODE_SERVICE_H
#define STN_NODE_SERVICE_H
#include "stn_rpc.h"
#include "stn_chain.h"
/* Node-owned immutable snapshot held for the complete dispatch. Caller owns
 * synchronization/lifetime; no persistence paths or cached work are exposed.
 * Each supported query independently validates full history incrementally.
 * Intelligence context is a separate existing staged-validation snapshot. */
typedef struct stn_node_service {
    const stn_chain_context *chain;
    const stn_block_span *blocks;
    size_t count;
    const stn_validation_context *intelligence;
} stn_node_service;
/* Bind to stn_rpc_service.handle; requests must originate from RPC dispatch.
 * No accepted-state mutation or submission acceptance exists in this adapter.
 * Intelligence check uses the existing validator. Submission validates but
 * returns UNAVAILABLE even when all validation stages pass: no admission queue.
 * Mining context is tip/target evidence, not a template or payable job. */
stn_rpc_code stn_node_service_handle(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);
#endif
