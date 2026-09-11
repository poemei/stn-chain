/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_identity.h"
#include <string.h>

int stn_ed25519_verify(const uint8_t *,size_t,const uint8_t[32],const uint8_t[64]);

static int canonical_scalar(const uint8_t *s)
{
    static const uint8_t order[32]={0xed,0xd3,0xf5,0x5c,0x1a,0x63,0x12,0x58,0xd6,0x9c,0xf7,0xa2,0xde,0xf9,0xde,0x14,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x10};
    size_t i;
    for(i=32;i!=0;--i){if(s[i-1]<order[i-1]){return 1;}if(s[i-1]>order[i-1]){return 0;}}
    return 0;
}
static int canonical_point(const uint8_t *p)
{
    static const uint8_t prime[32]={0xed,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f};
    uint8_t y[32];size_t i;
    memcpy(y,p,32);y[31]&=0x7f;
    for(i=32;i!=0;--i){if(y[i-1]<prime[i-1]){return 1;}if(y[i-1]>prime[i-1]){return 0;}}
    return 0;
}
static int nonzero(const uint8_t *p,size_t n){size_t i;uint8_t v=0;for(i=0;i<n;++i){v|=p[i];}return v!=0;}
static int small_order(const uint8_t *p)
{
    static const uint8_t identity[32]={1};
    return memcmp(p,identity,32)==0;
}

stn_identity_result stn_identity_derive(const uint8_t public_key[32],uint8_t identity[32])
{
    if(public_key==NULL || identity==NULL || !canonical_point(public_key) || !nonzero(public_key,32) || small_order(public_key)){return STN_IDENTITY_MALFORMED;}
    memcpy(identity,public_key,32);return STN_IDENTITY_VALID;
}
stn_identity_result stn_identity_statement(const uint8_t *bytes,size_t length,uint8_t *out,size_t cap,size_t *written)
{
    if(written!=NULL){*written=0;}
    if((bytes==NULL && length!=0) || out==NULL || written==NULL || length>SIZE_MAX-STN_IDENTITY_DOMAIN_SIZE-1u){return STN_IDENTITY_MALFORMED;}
    if(cap<STN_IDENTITY_DOMAIN_SIZE+length){return STN_IDENTITY_MALFORMED;}
    memcpy(out,STN_IDENTITY_DOMAIN,STN_IDENTITY_DOMAIN_SIZE-1u);out[STN_IDENTITY_DOMAIN_SIZE-1u]=0;
    if(length!=0){memcpy(out+STN_IDENTITY_DOMAIN_SIZE,bytes,length);}*written=STN_IDENTITY_DOMAIN_SIZE+length;
    return STN_IDENTITY_VALID;
}
stn_identity_result stn_identity_verify(const uint8_t public_key[32],const uint8_t *statement,size_t length,const uint8_t *signature,size_t signature_length)
{
    if(public_key==NULL || statement==NULL || signature==NULL || signature_length!=64 || !canonical_point(public_key) || !nonzero(public_key,32) || small_order(public_key)){return STN_IDENTITY_MALFORMED;}
    if(!canonical_point(signature) || !canonical_scalar(signature+32) || small_order(signature)){return STN_IDENTITY_MALFORMED;}
    return stn_ed25519_verify(statement,length,public_key,signature)==0 ? STN_IDENTITY_VALID : STN_IDENTITY_INVALID;
}
