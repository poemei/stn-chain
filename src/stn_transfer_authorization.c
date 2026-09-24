/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_authorization.h"
#include "stn_wallet.h"

#include <string.h>

stn_data_status stn_transfer_authorization_statement(
    const stn_transfer *transfer,
    const uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE],
    uint8_t statement[STN_TRANSFER_AUTHORIZATION_STATEMENT_SIZE])
{
    stn_address identity={0},wallet={0};
    uint8_t canonical[STN_TRANSFER_CANONICAL_SIZE];
    uint8_t result[STN_TRANSFER_AUTHORIZATION_STATEMENT_SIZE];

    if(transfer==NULL || controller==NULL || statement==NULL)return STN_DATA_ARGUMENT;
    if(stn_identity_derive(controller,identity.identifier)!=STN_IDENTITY_VALID)
        return STN_DATA_CONTENT;
    identity.type=STN_ADDRESS_IDENTITY;
    if(stn_wallet_derive(&identity,&wallet)!=STN_DATA_OK)return STN_DATA_CONTENT;
    if(memcmp(wallet.identifier,transfer->source.identifier,STN_ADDRESS_ID_SIZE)!=0)
        return STN_DATA_CONTENT;
    if(stn_transfer_encode(transfer,canonical)!=STN_DATA_OK)return STN_DATA_CONTENT;

    memcpy(result,STN_TRANSFER_AUTHORIZATION_DOMAIN,
           STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE-1u);
    result[STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE-1u]=0u;
    result[STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE]=STN_TRANSFER_AUTHORIZATION_VERSION;
    memcpy(result+STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE+1u,
           controller,STN_IDENTITY_PUBLIC_KEY_SIZE);
    memcpy(result+STN_TRANSFER_AUTHORIZATION_DOMAIN_SIZE+1u+
           STN_IDENTITY_PUBLIC_KEY_SIZE,canonical,sizeof(canonical));
    memcpy(statement,result,sizeof(result));
    return STN_DATA_OK;
}

stn_data_status stn_transfer_authorization_verify(
    const stn_transfer *transfer,
    const uint8_t controller[STN_IDENTITY_PUBLIC_KEY_SIZE],
    const uint8_t signature[STN_IDENTITY_SIGNATURE_SIZE])
{
    uint8_t statement[STN_TRANSFER_AUTHORIZATION_STATEMENT_SIZE];
    stn_data_status status;
    if(signature==NULL)return STN_DATA_ARGUMENT;
    status=stn_transfer_authorization_statement(transfer,controller,statement);
    if(status!=STN_DATA_OK)return status;
    return stn_identity_verify(controller,statement,sizeof(statement),
        signature,STN_IDENTITY_SIGNATURE_SIZE)==STN_IDENTITY_VALID
        ? STN_DATA_OK : STN_DATA_CONTENT;
}
