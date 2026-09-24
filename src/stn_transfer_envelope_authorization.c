/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope_authorization.h"
#include "stn_wallet.h"
#include <string.h>

stn_data_status stn_transfer_envelope_authorization_statement(
 const stn_transfer_envelope *e,uint8_t out[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE])
{
 stn_address identity={0},wallet={0};uint8_t transfer[STN_TRANSFER_CANONICAL_SIZE];
 uint8_t result[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE];size_t p=0,i;uint8_t nz=0;
 if(e==NULL || out==NULL)return STN_DATA_ARGUMENT;
 for(i=0;i<STN_TRANSFER_NONCE_SIZE;++i)nz|=e->nonce[i];if(nz==0)return STN_DATA_CONTENT;
 if(stn_identity_derive(e->controller,identity.identifier)!=STN_IDENTITY_VALID)return STN_DATA_CONTENT;
 identity.type=STN_ADDRESS_IDENTITY;
 if(stn_wallet_derive(&identity,&wallet)!=STN_DATA_OK ||
    memcmp(wallet.identifier,e->transfer.source.identifier,STN_ADDRESS_ID_SIZE)!=0)return STN_DATA_CONTENT;
 if(stn_transfer_encode(&e->transfer,transfer)!=STN_DATA_OK)return STN_DATA_CONTENT;
 memcpy(result+p,STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN,
  STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE-1u);p+=STN_TRANSFER_ENVELOPE_AUTHORIZATION_DOMAIN_SIZE-1u;
 result[p++]=0u;result[p++]=STN_TRANSFER_ENVELOPE_AUTHORIZATION_VERSION;
 memcpy(result+p,e->controller,32u);p+=32u;memcpy(result+p,e->nonce,32u);p+=32u;
 memcpy(result+p,transfer,sizeof(transfer));memcpy(out,result,sizeof(result));return STN_DATA_OK;
}
stn_data_status stn_transfer_envelope_authorization_verify(const stn_transfer_envelope *e)
{
 uint8_t statement[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE];stn_data_status s;
 if(e==NULL)return STN_DATA_ARGUMENT;
 s=stn_transfer_envelope_authorization_statement(e,statement);if(s!=STN_DATA_OK)return s;
 return stn_identity_verify(e->controller,statement,sizeof(statement),e->signature,
  STN_IDENTITY_SIGNATURE_SIZE)==STN_IDENTITY_VALID?STN_DATA_OK:STN_DATA_CONTENT;
}
