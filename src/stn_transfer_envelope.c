/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope.h"
#include <string.h>

static int nonzero(const uint8_t *p,size_t n){size_t i;uint8_t v=0;for(i=0;i<n;++i)v|=p[i];return v!=0;}

stn_data_status stn_transfer_envelope_encode(const stn_transfer_envelope *e,
    uint8_t out[STN_TRANSFER_ENVELOPE_CANONICAL_SIZE])
{
    uint8_t result[STN_TRANSFER_ENVELOPE_CANONICAL_SIZE],transfer[STN_TRANSFER_CANONICAL_SIZE];
    size_t p=0;
    if(e==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(!nonzero(e->controller,sizeof(e->controller)) || !nonzero(e->nonce,sizeof(e->nonce)))
        return STN_DATA_CONTENT;
    if(stn_transfer_encode(&e->transfer,transfer)!=STN_DATA_OK)return STN_DATA_CONTENT;
    result[p++]=STN_TRANSFER_ENVELOPE_VERSION;
    memcpy(result+p,e->controller,sizeof(e->controller));p+=sizeof(e->controller);
    memcpy(result+p,e->nonce,sizeof(e->nonce));p+=sizeof(e->nonce);
    memcpy(result+p,transfer,sizeof(transfer));p+=sizeof(transfer);
    memcpy(result+p,e->signature,sizeof(e->signature));
    memcpy(out,result,sizeof(result));return STN_DATA_OK;
}

stn_data_status stn_transfer_envelope_decode(const uint8_t *bytes,size_t length,
    stn_transfer_envelope *out)
{
    stn_transfer_envelope e={0};size_t p=0;stn_data_status status;
    if(bytes==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(length!=STN_TRANSFER_ENVELOPE_CANONICAL_SIZE)return STN_DATA_LENGTH;
    if(bytes[p++]!=STN_TRANSFER_ENVELOPE_VERSION)return STN_DATA_VERSION;
    memcpy(e.controller,bytes+p,sizeof(e.controller));p+=sizeof(e.controller);
    memcpy(e.nonce,bytes+p,sizeof(e.nonce));p+=sizeof(e.nonce);
    if(!nonzero(e.controller,sizeof(e.controller)) || !nonzero(e.nonce,sizeof(e.nonce)))
        return STN_DATA_CONTENT;
    status=stn_transfer_decode(bytes+p,STN_TRANSFER_CANONICAL_SIZE,&e.transfer);
    if(status!=STN_DATA_OK)return STN_DATA_CONTENT;
    p+=STN_TRANSFER_CANONICAL_SIZE;
    memcpy(e.signature,bytes+p,sizeof(e.signature));
    *out=e;return STN_DATA_OK;
}
