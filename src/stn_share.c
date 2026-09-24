/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share.h"
#include "stn_sha256.h"
#include <string.h>

static void write64be(uint8_t out[8],uint64_t value)
{
    size_t i;
    for(i=0;i<8u;++i){
        out[7u-i]=(uint8_t)(value&0xffu);
        value>>=8;
    }
}

stn_data_status stn_share_encode(
    const stn_share_evidence *share,
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE])
{
    uint8_t result[STN_SHARE_CANONICAL_SIZE];

    if(share==NULL || canonical==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(share->miner.type!=STN_ADDRESS_IDENTITY){
        return STN_DATA_TYPE;
    }

    result[0]=(uint8_t)STN_SHARE_VERSION;
    memcpy(result+1,share->work_id,STN_SHARE_WORK_ID_SIZE);
    memcpy(result+33,share->miner.identifier,STN_ADDRESS_ID_SIZE);
    write64be(result+65,share->nonce);

    memcpy(canonical,result,sizeof(result));
    return STN_DATA_OK;
}

stn_data_status stn_share_id(
    const stn_share_evidence *share,
    uint8_t id[STN_SHARE_ID_SIZE])
{
    static const uint8_t domain[]="STN-CHAIN:SHARE:ID:1";
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE];
    uint8_t result[STN_SHARE_ID_SIZE];
    stn_data_status status;

    if(share==NULL || id==NULL){
        return STN_DATA_ARGUMENT;
    }

    status=stn_share_encode(share,canonical);
    if(status!=STN_DATA_OK){
        return status;
    }

    status=stn_sha256(
        NULL,
        domain,
        sizeof(domain),
        canonical,
        sizeof(canonical),
        result);
    if(status==STN_DATA_OK){
        memcpy(id,result,sizeof(result));
    }
    return status;
}
