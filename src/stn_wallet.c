/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_wallet.h"

#include <string.h>

stn_data_status stn_wallet_derive(
    const stn_address *identity,
    stn_address *wallet)
{
    if(identity==NULL || wallet==NULL)return STN_DATA_ARGUMENT;
    if(identity->type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
    return stn_address_derive(STN_ADDRESS_WALLET,
        identity->identifier,STN_ADDRESS_ID_SIZE,wallet);
}

static stn_data_status binding_valid(const stn_wallet_binding *binding)
{
    stn_address expected;
    stn_data_status status;
    if(binding==NULL)return STN_DATA_ARGUMENT;
    if(binding->identity.type!=STN_ADDRESS_IDENTITY ||
       binding->wallet.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    status=stn_wallet_derive(&binding->identity,&expected);
    if(status!=STN_DATA_OK)return status;
    if(memcmp(expected.identifier,binding->wallet.identifier,
              STN_ADDRESS_ID_SIZE)!=0)return STN_DATA_CONTENT;
    return STN_DATA_OK;
}

stn_data_status stn_wallet_binding_encode(
    const stn_wallet_binding *binding,
    uint8_t canonical[STN_WALLET_BINDING_CANONICAL_SIZE])
{
    uint8_t result[STN_WALLET_BINDING_CANONICAL_SIZE];
    stn_data_status status;
    if(binding==NULL || canonical==NULL)return STN_DATA_ARGUMENT;
    status=binding_valid(binding);
    if(status!=STN_DATA_OK)return status;
    result[0]=STN_WALLET_VERSION;
    memcpy(result+1u,binding->identity.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(result+33u,binding->wallet.identifier,STN_ADDRESS_ID_SIZE);
    memcpy(canonical,result,sizeof(result));
    return STN_DATA_OK;
}

stn_data_status stn_wallet_binding_decode(
    const uint8_t *canonical,
    size_t length,
    stn_wallet_binding *out)
{
    stn_wallet_binding result;
    stn_data_status status;
    if(canonical==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(length!=STN_WALLET_BINDING_CANONICAL_SIZE)return STN_DATA_LENGTH;
    if(canonical[0]!=STN_WALLET_VERSION)return STN_DATA_CONTENT;
    memset(&result,0,sizeof(result));
    result.identity.type=STN_ADDRESS_IDENTITY;
    result.wallet.type=STN_ADDRESS_WALLET;
    memcpy(result.identity.identifier,canonical+1u,STN_ADDRESS_ID_SIZE);
    memcpy(result.wallet.identifier,canonical+33u,STN_ADDRESS_ID_SIZE);
    status=binding_valid(&result);
    if(status!=STN_DATA_OK)return status;
    *out=result;
    return STN_DATA_OK;
}
