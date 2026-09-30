/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#ifndef STN_CONTRACT_RESPONSE_QUERY_H
#define STN_CONTRACT_RESPONSE_QUERY_H
#include "stn_contract_snapshot.h"
#include "stn_rpc.h"
/* Request: canonical contract address[70], zero-based response index u32 BE.
 * Reply: total u32 BE, canonical response length u32 BE, exact STRP record.
 * An existing contract with no responses returns total=length=0 at index zero.
 * Unknown contract/out-of-range index returns NOT_FOUND. Caller holds snapshot. */
int stn_contract_response_query_request_valid(const uint8_t *bytes,size_t length);
int stn_contract_response_query_reply_valid(const uint8_t *bytes,size_t length);
stn_rpc_code stn_contract_response_query(const stn_contract_snapshot *snapshot,
    const uint8_t *request,size_t length,uint8_t *out,size_t capacity,size_t *written);
#endif
