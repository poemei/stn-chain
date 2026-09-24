/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
/* Test-only linkage shim for stn_pow_verify. Production stn_chain_block_id
 * remains owned by stn_chain.c; this target does not exercise Chain state. */
#include "stn_chain.h"
#include <string.h>

stn_data_status stn_chain_block_id(const uint8_t *bytes,size_t length,
    const stn_hash_provider *provider,uint8_t digest[32])
{
    static const uint8_t domain[]="STN-CHAIN:BLOCK:ID:1";
    uint8_t temporary[32]={0};
    stn_data_status status;
    if(digest==NULL)return STN_DATA_ARGUMENT;
    status=stn_block_validate_structure(bytes,length);
    if(status!=STN_DATA_OK)return status;
    if(provider==NULL || provider->hash==NULL)return STN_DATA_UNRESOLVED;
    status=provider->hash(provider->user,domain,sizeof(domain),bytes,STN_BLOCK_HEADER_SIZE,temporary);
    if(status==STN_DATA_OK)memcpy(digest,temporary,32);
    else if(status!=STN_DATA_UNRESOLVED)status=STN_DATA_PROVIDER_ERROR;
    return status;
}
