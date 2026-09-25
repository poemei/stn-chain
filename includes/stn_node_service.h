/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_NODE_SERVICE_H
#define STN_NODE_SERVICE_H
#include "stn_rpc.h"
#include "stn_chain.h"
/* Node-owned immutable snapshot held for the complete dispatch. Caller owns
 * synchronization/lifetime; no persistence paths or cached work are exposed.
 * State-bearing queries reconstruct the complete immutable accepted history so
 * validation has the historical evidence required by consensus rules.
 * GET_ACCEPTED_RECORD reconstructs historical production eligibility and
 * returns exact canonical transaction evidence; pending is never consulted.
 * Intelligence context is a separate existing staged-validation snapshot.
 * DERIVE_ADDRESS is stateless and supports the three canonical typed address
 * namespaces without consulting or mutating accepted Chain state. */
typedef struct stn_node_service {
    const stn_chain_context *chain;
    const stn_block_span *blocks;
    size_t count;
    const stn_validation_context *intelligence;
    /* Optional caller-retained immutable evidence; zero-initialize when absent. */
    const stn_block_span *retained;
    size_t retained_count;
} stn_node_service;
/* Bind to stn_rpc_service.handle; requests must originate from RPC dispatch.
 * No accepted-state mutation or submission acceptance exists in this adapter.
 * Intelligence check uses the existing validator. Submission validates but
 * returns UNAVAILABLE even when all validation stages pass: no admission queue.
 * Mining context is tip/target evidence, not a template or payable job. */
stn_rpc_code stn_node_service_handle(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);
#endif
