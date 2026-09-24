/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_issuance.h"
#include <string.h>

static int valid_amount(uint8_t reason,uint64_t units)
{
    return (reason==STN_ISSUANCE_REASON_SHARE && units==STN_ISSUANCE_SHARE_UNITS) ||
           (reason==STN_ISSUANCE_REASON_BLOCK && units==STN_ISSUANCE_BLOCK_UNITS);
}

static void write64be(uint8_t out[8],uint64_t value)
{
    size_t i;
    for(i=0;i<8u;++i){
        out[7u-i]=(uint8_t)(value&0xffu);
        value>>=8;
    }
}

static uint64_t read64be(const uint8_t in[8])
{
    uint64_t value=0u;
    size_t i;
    for(i=0;i<8u;++i){
        value=(value<<8)|in[i];
    }
    return value;
}

stn_data_status stn_issuance_encode(
    const stn_issuance_record *record,
    uint8_t canonical[STN_ISSUANCE_CANONICAL_SIZE])
{
    uint8_t result[STN_ISSUANCE_CANONICAL_SIZE];

    if(record==NULL || canonical==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(record->destination.mining_identity.type!=STN_ADDRESS_IDENTITY ||
       record->destination.wallet.type!=STN_ADDRESS_WALLET){
        return STN_DATA_TYPE;
    }
    if(!valid_amount(record->reason,record->units)){
        return STN_DATA_CONTENT;
    }

    result[0]=(uint8_t)STN_ISSUANCE_VERSION;
    result[1]=record->reason;
    write64be(result+2u,record->units);
    memcpy(result+10u,record->evidence_id,STN_ISSUANCE_EVIDENCE_ID_SIZE);
    memcpy(result+42u,record->destination.mining_identity.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(result+74u,record->destination.wallet.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(canonical,result,sizeof(result));
    return STN_DATA_OK;
}

stn_data_status stn_issuance_decode(
    const uint8_t *canonical,
    size_t length,
    stn_issuance_record *out)
{
    stn_issuance_record result={0};

    if(canonical==NULL || out==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(length!=STN_ISSUANCE_CANONICAL_SIZE){
        return STN_DATA_LENGTH;
    }
    if(canonical[0]!=(uint8_t)STN_ISSUANCE_VERSION){
        return STN_DATA_VERSION;
    }

    result.reason=canonical[1];
    result.units=read64be(canonical+2u);
    if(!valid_amount(result.reason,result.units)){
        return STN_DATA_CONTENT;
    }
    memcpy(result.evidence_id,canonical+10u,STN_ISSUANCE_EVIDENCE_ID_SIZE);
    result.destination.mining_identity.type=STN_ADDRESS_IDENTITY;
    memcpy(result.destination.mining_identity.identifier,canonical+42u,STN_ADDRESS_ID_SIZE);
    result.destination.wallet.type=STN_ADDRESS_WALLET;
    memcpy(result.destination.wallet.identifier,canonical+74u,STN_ADDRESS_ID_SIZE);
    *out=result;
    return STN_DATA_OK;
}
