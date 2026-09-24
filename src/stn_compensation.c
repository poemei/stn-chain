/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_compensation.h"
#include <string.h>

stn_data_status stn_compensation_destination_encode(
    const stn_compensation_destination *destination,
    uint8_t canonical[STN_COMPENSATION_DESTINATION_SIZE])
{
    uint8_t result[STN_COMPENSATION_DESTINATION_SIZE];

    if(destination==NULL || canonical==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(destination->mining_identity.type!=STN_ADDRESS_IDENTITY ||
       destination->wallet.type!=STN_ADDRESS_WALLET){
        return STN_DATA_TYPE;
    }

    result[0]=(uint8_t)STN_COMPENSATION_DESTINATION_VERSION;
    memcpy(result+1u,destination->mining_identity.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(result+33u,destination->wallet.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(canonical,result,sizeof(result));
    return STN_DATA_OK;
}

stn_data_status stn_compensation_destination_decode(
    const uint8_t *canonical,
    size_t length,
    stn_compensation_destination *out)
{
    stn_compensation_destination result={0};

    if(canonical==NULL || out==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(length!=STN_COMPENSATION_DESTINATION_SIZE){
        return STN_DATA_LENGTH;
    }
    if(canonical[0]!=(uint8_t)STN_COMPENSATION_DESTINATION_VERSION){
        return STN_DATA_VERSION;
    }

    result.mining_identity.type=STN_ADDRESS_IDENTITY;
    memcpy(result.mining_identity.identifier,canonical+1u,STN_ADDRESS_ID_SIZE);
    result.wallet.type=STN_ADDRESS_WALLET;
    memcpy(result.wallet.identifier,canonical+33u,STN_ADDRESS_ID_SIZE);
    *out=result;
    return STN_DATA_OK;
}
