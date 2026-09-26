/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation.h"
#include "stn_sha256.h"
#include <string.h>

static int zero32(const uint8_t value[32])
{
    size_t i;uint8_t any=0u;
    for(i=0u;i<32u;++i)any|=value[i];
    return any==0u;
}

stn_data_status stn_block_compensation_encode(
    const stn_block_compensation_evidence *evidence,
    uint8_t canonical[STN_BLOCK_COMPENSATION_CANONICAL_SIZE])
{
    uint8_t encoded[STN_BLOCK_COMPENSATION_CANONICAL_SIZE];
    if(evidence==NULL || canonical==NULL)return STN_DATA_ARGUMENT;
    if(evidence->miner.type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
    if(zero32(evidence->block_id) || zero32(evidence->miner.identifier))return STN_DATA_CONTENT;
    encoded[0]=STN_BLOCK_COMPENSATION_VERSION;
    memcpy(encoded+1u,evidence->block_id,32u);
    memcpy(encoded+33u,evidence->miner.identifier,32u);
    memcpy(canonical,encoded,sizeof(encoded));
    return STN_DATA_OK;
}

stn_data_status stn_block_compensation_decode(
    const uint8_t *canonical,size_t length,
    stn_block_compensation_evidence *out)
{
    stn_block_compensation_evidence decoded={0};
    if(canonical==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(length!=STN_BLOCK_COMPENSATION_CANONICAL_SIZE)return STN_DATA_LENGTH;
    if(canonical[0]!=STN_BLOCK_COMPENSATION_VERSION)return STN_DATA_VERSION;
    memcpy(decoded.block_id,canonical+1u,32u);
    decoded.miner.type=STN_ADDRESS_IDENTITY;
    memcpy(decoded.miner.identifier,canonical+33u,32u);
    if(zero32(decoded.block_id) || zero32(decoded.miner.identifier))return STN_DATA_CONTENT;
    *out=decoded;
    return STN_DATA_OK;
}

stn_data_status stn_block_compensation_id(
    const stn_block_compensation_evidence *evidence,
    uint8_t id[STN_BLOCK_COMPENSATION_ID_SIZE])
{
    static const uint8_t domain[]="STN-CHAIN:BLOCK-COMPENSATION:ID:1";
    uint8_t canonical[STN_BLOCK_COMPENSATION_CANONICAL_SIZE];
    stn_data_status status;
    if(evidence==NULL || id==NULL)return STN_DATA_ARGUMENT;
    status=stn_block_compensation_encode(evidence,canonical);
    if(status!=STN_DATA_OK)return status;
    return stn_sha256(NULL,domain,sizeof(domain),canonical,sizeof(canonical),id);
}
