/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_RPC_H
#define STN_RPC_H
#include "stn_block.h"
#include "stn_pending.h"
#define STN_RPC_HEADER_SIZE 24u
#define STN_RPC_MINER_IDENTITY_SIZE 69u
#define STN_RPC_MINING_SUBMISSION_PREFIX 137u
#define STN_RPC_SHARE_SUBMISSION_PREFIX 109u
#define STN_RPC_SHARE_SUBMISSION_SIZE (STN_RPC_SHARE_SUBMISSION_PREFIX + STN_BLOCK_HEADER_SIZE)
#define STN_RPC_MAX_PAYLOAD (STN_RPC_MINING_SUBMISSION_PREFIX+STN_BLOCK_MAX_SIZE)
#define STN_RPC_MAX_FRAME (STN_RPC_HEADER_SIZE+STN_RPC_MAX_PAYLOAD)
#define STN_RPC_READ 1u
#define STN_RPC_SUBMISSION 2u
#define STN_RPC_ADMIN 4u
#define STN_RPC_ACCEPTED_RECORD_PREFIX 76u
#define STN_RPC_CONTRACT_LIST_ENTRY_SIZE 90u
#define STN_RPC_CONTRACT_LIST_MAX 16u
#define STN_RPC_TRANSACTION_STATUS_SIZE 44u
typedef enum stn_rpc_code {
    STN_RPC_OK=0,STN_RPC_INVALID,STN_RPC_VERSION,STN_RPC_METHOD,
    STN_RPC_FORBIDDEN,STN_RPC_UNAVAILABLE,STN_RPC_NOT_FOUND,
    STN_RPC_REJECTED,STN_RPC_PROVIDER,STN_RPC_CAPACITY,STN_RPC_STALE,STN_RPC_END=11,STN_RPC_DETACHED=12,
    STN_RPC_CURRENT=13,STN_RPC_COMMON_ANCESTOR=14,STN_RPC_NO_COMMON_ANCESTOR=15,
    STN_RPC_RECOVER_AFTER_CURSOR=16,STN_RPC_RECOVER_FROM_START=17
} stn_rpc_code;
typedef enum stn_rpc_method {
    STN_RPC_INFO=1,STN_RPC_BLOCK_HEIGHT=2,STN_RPC_BLOCK_ID=3,
    STN_RPC_GET_ACCEPTED_RECORD=4,
    STN_RPC_GET_FIRST_ACCEPTED_RECORD=5,STN_RPC_GET_NEXT_ACCEPTED_RECORD=6,
    STN_RPC_GET_CURSOR_REORG_STATUS=7,STN_RPC_GET_CONSUMER_RECOVERY_PLAN=8,
    STN_RPC_DERIVE_ADDRESS=9,
    STN_RPC_BALANCE=10,
    STN_RPC_CONTRACT_STATE=11,
    /* CONTRACT_LIST request: exact canonical 69-byte stn0_ identity address.
     * Success: u16 count followed by count fixed 90-byte entries:
     * stnc0_ text[70], state u16, type u16, sequence u64, created_at u64.
     * Results are active/nonterminal first, newest first, bounded to 16. */
    STN_RPC_CONTRACT_LIST=12,
    /* Exact canonical 32-byte transaction identifier. OK returns height u64,
     * accepted block id[32], transaction position u32. NOT_FOUND means absent
     * from the current accepted history; it does not mean rejected or absent
     * from pending state. */
    STN_RPC_TRANSACTION_STATUS=13,
    STN_RPC_CONTRACT_RESPONSE=14, /* indexed accepted response; see response_query.h */
    STN_RPC_CHECK_INTELLIGENCE=0x1000,STN_RPC_SUBMIT_INTELLIGENCE=0x1001,
    STN_RPC_INTELLIGENCE_ID=0x1002,STN_RPC_INTELLIGENCE_CURSOR=0x1003,
    STN_RPC_PENDING=0x1004,STN_RPC_SUBMIT_TRANSACTION=0x1005,
    STN_RPC_SUBMIT_BLOCK_EVIDENCE=0x1006,
    STN_RPC_SUBMIT_HISTORY_EVIDENCE=0x1007,
    STN_RPC_SUBMIT_SUFFIX_EVIDENCE=0x1008,
    STN_RPC_SUFFIX_STAGE_BEGIN=0x1009,
    STN_RPC_SUFFIX_STAGE_APPEND=0x100a,
    STN_RPC_SUFFIX_STAGE_COMMIT=0x100b,
    STN_RPC_SUFFIX_STAGE_ABORT=0x100c,
    STN_RPC_MINING_CONTEXT=0x2000,STN_RPC_CHECK_WORK_BASE=0x2001,
    STN_RPC_MINING_TEMPLATE=0x2002,STN_RPC_SUBMIT_WORK=0x2003,STN_RPC_SUBMIT_SHARE=0x2004,
    STN_RPC_ADMIN_CONTROL=0x3000
} stn_rpc_method;
typedef enum stn_rpc_submission_result {
    STN_RPC_ADMITTED=0, STN_RPC_DUPLICATE=1, STN_RPC_POOL_FULL=2,
    STN_RPC_BAD_SUBMISSION=3, STN_RPC_UNSUPPORTED_SUBMISSION=4,
    STN_RPC_REPLAY=5, STN_RPC_UNAUTHORIZED=6,
    STN_RPC_ADMISSION_UNAVAILABLE=7, STN_RPC_ADMISSION_INTERNAL=8
} stn_rpc_submission_result;
typedef struct stn_rpc_message {
    uint16_t kind;
    uint16_t method;
    stn_rpc_code code;
    uint64_t request_id;
    const uint8_t *payload;
    size_t length;
} stn_rpc_message;
stn_rpc_code stn_rpc_payload_length(const uint8_t *bytes,size_t length,size_t *payload_length);
stn_rpc_code stn_rpc_decode(const uint8_t *bytes,size_t length,stn_rpc_message *out);
stn_rpc_code stn_rpc_encode(const stn_rpc_message *message,uint8_t *bytes,size_t capacity,size_t *written);
typedef stn_rpc_code (*stn_rpc_handler)(void *user,const stn_rpc_message *request,
    uint8_t *payload,size_t capacity,size_t *written);
typedef struct stn_rpc_service { void *user;stn_rpc_handler handle; } stn_rpc_service;
stn_rpc_code stn_rpc_dispatch(const uint8_t *request,size_t length,uint32_t allowed,
    const stn_rpc_service *service,uint8_t *response,size_t capacity,size_t *written);
#endif