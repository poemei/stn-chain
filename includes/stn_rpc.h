/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_RPC_H
#define STN_RPC_H
#include "stn_block.h"
#include "stn_pending.h"
#define STN_RPC_HEADER_SIZE 24u
#define STN_RPC_MAX_PAYLOAD (68u+STN_BLOCK_MAX_SIZE)
#define STN_RPC_MAX_FRAME (STN_RPC_HEADER_SIZE+STN_RPC_MAX_PAYLOAD)
#define STN_RPC_READ 1u
#define STN_RPC_SUBMISSION 2u
#define STN_RPC_ADMIN 4u
#define STN_RPC_ACCEPTED_RECORD_PREFIX 76u
typedef enum stn_rpc_code {
    STN_RPC_OK=0,STN_RPC_INVALID,STN_RPC_VERSION,STN_RPC_METHOD,
    STN_RPC_FORBIDDEN,STN_RPC_UNAVAILABLE,STN_RPC_NOT_FOUND,
    STN_RPC_REJECTED,STN_RPC_PROVIDER,STN_RPC_CAPACITY,STN_RPC_STALE
} stn_rpc_code;
typedef enum stn_rpc_method {
    STN_RPC_INFO=1,STN_RPC_BLOCK_HEIGHT=2,STN_RPC_BLOCK_ID=3,
    STN_RPC_GET_ACCEPTED_RECORD=4,
    STN_RPC_CHECK_INTELLIGENCE=0x1000,STN_RPC_SUBMIT_INTELLIGENCE=0x1001,
    STN_RPC_INTELLIGENCE_ID=0x1002,STN_RPC_INTELLIGENCE_CURSOR=0x1003,
    STN_RPC_PENDING=0x1004,STN_RPC_SUBMIT_TRANSACTION=0x1005,
    STN_RPC_MINING_CONTEXT=0x2000,STN_RPC_CHECK_WORK_BASE=0x2001,
    STN_RPC_MINING_TEMPLATE=0x2002,STN_RPC_SUBMIT_WORK=0x2003,
    STN_RPC_ADMIN_CONTROL=0x3000
} stn_rpc_method;
/* STNC submission-result v1 wire values; independent of internal enums. */
typedef enum stn_rpc_submission_result {
    STN_RPC_ADMITTED=0, STN_RPC_DUPLICATE=1, STN_RPC_POOL_FULL=2,
    STN_RPC_BAD_SUBMISSION=3, STN_RPC_UNSUPPORTED_SUBMISSION=4,
    STN_RPC_REPLAY=5, STN_RPC_UNAUTHORIZED=6,
    STN_RPC_ADMISSION_UNAVAILABLE=7, STN_RPC_ADMISSION_INTERNAL=8
} stn_rpc_submission_result;
typedef struct stn_rpc_message {
    uint16_t kind; /* 1 request, 2 response */
    uint16_t method;
    stn_rpc_code code;
    uint64_t request_id;
    const uint8_t *payload;
    size_t length;
} stn_rpc_message;
/* Fixed-header preflight for stream readers. It does not interpret magic,
 * version, kind or method, so dispatch retains their existing error semantics.
 * It only permits an exact 24-byte header and a declared payload within the
 * global bound before the caller reads that payload from an untrusted stream. */
stn_rpc_code stn_rpc_payload_length(const uint8_t *bytes,size_t length,size_t *payload_length);
/* STNC,u16 version=2,u16 kind,u16 method,u16 code,u64 ID,u32 length.
 * Big-endian; exact length; no implicit fields. Request code must be zero.
 * Decode checks method payload shape for known requests; unknown methods are
 * well-framed requests for dispatch to answer METHOD, never service calls.
 * Encode/decode output unchanged on failure except *written=0 for encode.
 * Objects/spans must be disjoint; decoded payload borrows immutable bytes. */
stn_rpc_code stn_rpc_decode(const uint8_t *bytes,size_t length,stn_rpc_message *out);
stn_rpc_code stn_rpc_encode(const stn_rpc_message *message,uint8_t *bytes,size_t capacity,size_t *written);
/* Internal service boundary. Only validated, authorized, known requests reach
 * the handler. Handler is trusted to obey capacity/lifetime and capability
 * semantics; it must not mutate read-only snapshots. Error responses carry no
 * payload. No authentication is implied by a local allowed-capability mask. */
typedef stn_rpc_code (*stn_rpc_handler)(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);
typedef struct stn_rpc_service { void *user;stn_rpc_handler handle; } stn_rpc_service;
/* Returns OK if a complete response was encoded (including protocol errors).
 * Invalid frames receive request_id=0/method=0; no unvalidated correlation is
 * echoed. Output is scratch until success; *written stays zero on failure.
 * No socket, persistence path, global state or allocation is involved. */
stn_rpc_code stn_rpc_dispatch(const uint8_t *request,size_t length,uint32_t allowed,
    const stn_rpc_service *service,uint8_t *response,size_t capacity,size_t *written);
#endif
