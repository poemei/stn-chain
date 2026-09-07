/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pow.h"
#include "stn_chain.h"
#include <string.h>

stn_data_status stn_target_validate(const uint8_t *target,size_t length)
{
    size_t i; unsigned value=0;
    if(target==NULL) { return STN_DATA_ARGUMENT; }
    if(length!=32) { return STN_DATA_LENGTH; }
    for(i=0;i<32;++i) { value|=target[i]; }
    return value!=0 && target[0]<0x80 ? STN_DATA_OK : STN_DATA_TARGET;
}

stn_data_status stn_pow_compare(const uint8_t hash[32],const uint8_t target[32])
{
    stn_data_status s;
    if(hash==NULL) { return STN_DATA_ARGUMENT; }
    s=stn_target_validate(target,32);
    if(s!=STN_DATA_OK) { return s; }
    return memcmp(hash,target,32)<=0 ? STN_DATA_OK : STN_DATA_WORK;
}

stn_data_status stn_work_add(const stn_work *a,const stn_work *b,stn_work *out)
{
    stn_work sum={{0}}; size_t i=32; unsigned carry=0;
    if(a==NULL || b==NULL || out==NULL) { return STN_DATA_ARGUMENT; }
    while(i!=0) { unsigned v; --i; v=(unsigned)a->bytes[i]+b->bytes[i]+carry; sum.bytes[i]=(uint8_t)(v&255u); carry=v>>8; }
    if(carry!=0) { return STN_DATA_OVERFLOW; }
    *out=sum; return STN_DATA_OK;
}

stn_data_status stn_target_work(const uint8_t target[32],stn_work *out)
{
    uint8_t denominator[33]={0},remainder[33]={0};
    stn_work q={{0}}; int bit; size_t i; unsigned carry=1;
    stn_data_status s;
    if(out==NULL) { return STN_DATA_ARGUMENT; }
    s=stn_target_validate(target,32); if(s!=STN_DATA_OK) { return s; }
    memcpy(denominator+1,target,32);
    for(i=33;i!=0;) { unsigned v; --i; v=denominator[i]+carry; denominator[i]=(uint8_t)(v&255u); carry=v>>8; }
    /* Bounded binary long division of the 257-bit numerator 2^256. */
    for(bit=256;bit>=0;--bit) {
        carry=bit==256 ? 1u : 0u;
        for(i=33;i!=0;) { unsigned v; --i; v=(unsigned)remainder[i]*2u+carry; remainder[i]=(uint8_t)(v&255u); carry=v>>8; }
        if(memcmp(remainder,denominator,33)>=0) {
            unsigned borrow=0;
            for(i=33;i!=0;) { unsigned sub,v; --i; sub=denominator[i]+borrow; v=remainder[i]; remainder[i]=(uint8_t)((v-sub)&255u); borrow=v<sub; }
            if(bit==256) { return STN_DATA_OVERFLOW; }
            q.bytes[31-(unsigned)bit/8]|=(uint8_t)(1u<<((unsigned)bit%8));
        }
    }
    *out=q; return STN_DATA_OK;
}

stn_data_status stn_work_at_height(const uint8_t target[32],uint64_t height,stn_work *out)
{
    stn_work unit,power,sum={{0}}; stn_data_status s;
    if(out==NULL) { return STN_DATA_ARGUMENT; }
    s=stn_target_work(target,&unit); if(s!=STN_DATA_OK) { return s; }
    power=unit;
    while(height!=0) {
        if((height&1u)!=0) { s=stn_work_add(&sum,&power,&sum); if(s!=STN_DATA_OK) { return s; } }
        height>>=1;
        if(height!=0) { s=stn_work_add(&power,&power,&power); if(s!=STN_DATA_OK) { return s; } }
    }
    s=stn_work_add(&sum,&unit,&sum); if(s==STN_DATA_OK) { *out=sum; }
    return s;
}

stn_data_status stn_pow_verify(const uint8_t *bytes,size_t length,
    const stn_hash_provider *provider,uint8_t digest[32])
{
    stn_block b; uint8_t hash[32]; stn_data_status s;
    if(digest==NULL) { return STN_DATA_ARGUMENT; }
    s=stn_block_decode(bytes,length,&b); if(s!=STN_DATA_OK) { return s; }
    if(b.header.version!=STN_POW_BLOCK_VERSION) { return STN_DATA_VERSION; }
    s=stn_target_validate(b.header.reserved_target,32); if(s!=STN_DATA_OK) { return s; }
    s=stn_chain_block_id(bytes,length,provider,hash); if(s!=STN_DATA_OK) { return s; }
    s=stn_pow_compare(hash,b.header.reserved_target);
    if(s==STN_DATA_OK) { memcpy(digest,hash,32); }
    return s;
}
