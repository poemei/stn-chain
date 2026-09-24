/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer.h"

#include <string.h>

static stn_data_status transfer_valid(const stn_transfer *transfer)
{
    if(transfer==NULL)return STN_DATA_ARGUMENT;
    if(transfer->source.type!=STN_ADDRESS_WALLET ||
       transfer->destination.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if(transfer->units==0u)return STN_DATA_CONTENT;
    if(memcmp(transfer->source.identifier,transfer->destination.identifier,
              STN_ADDRESS_ID_SIZE)==0)return STN_DATA_CONTENT;
    return STN_DATA_OK;
}

stn_data_status stn_transfer_encode(
    const stn_transfer *transfer,
    uint8_t canonical[STN_TRANSFER_CANONICAL_SIZE])
{
    uint8_t result[STN_TRANSFER_CANONICAL_SIZE];
    uint64_t units;
    size_t i;
    stn_data_status status;
    if(transfer==NULL || canonical==NULL)return STN_DATA_ARGUMENT;
    status=transfer_valid(transfer);
    if(status!=STN_DATA_OK)return status;
    result[0]=STN_TRANSFER_VERSION;
    memcpy(result+1u,transfer->source.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(result+33u,transfer->destination.identifier,STN_ADDRESS_ID_SIZE);
    units=transfer->units;
    for(i=0u;i<8u;++i){
        result[72u-i]=(uint8_t)(units&0xffu);
        units>>=8;
    }
    memcpy(canonical,result,sizeof(result));
    return STN_DATA_OK;
}

stn_data_status stn_transfer_decode(
    const uint8_t *canonical,
    size_t length,
    stn_transfer *out)
{
    stn_transfer result;
    size_t i;
    stn_data_status status;
    if(canonical==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(length!=STN_TRANSFER_CANONICAL_SIZE)return STN_DATA_LENGTH;
    if(canonical[0]!=STN_TRANSFER_VERSION)return STN_DATA_CONTENT;
    memset(&result,0,sizeof(result));
    result.source.type=STN_ADDRESS_WALLET;
    result.destination.type=STN_ADDRESS_WALLET;
    memcpy(result.source.identifier,canonical+1u,STN_ADDRESS_ID_SIZE);
    memcpy(result.destination.identifier,canonical+33u,STN_ADDRESS_ID_SIZE);
    for(i=0u;i<8u;++i)result.units=(result.units<<8)|canonical[65u+i];
    status=transfer_valid(&result);
    if(status!=STN_DATA_OK)return status;
    *out=result;
    return STN_DATA_OK;
}
