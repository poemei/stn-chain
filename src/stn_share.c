/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share.h"
#include "stn_sha256.h"
#include "stn_chain.h"
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

stn_data_status stn_share_decode(
    const uint8_t canonical[STN_SHARE_CANONICAL_SIZE],
    size_t length,
    stn_share_evidence *out)
{
    stn_share_evidence share={0};
    size_t i;

    if(canonical==NULL || out==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(length!=STN_SHARE_CANONICAL_SIZE){
        return STN_DATA_LENGTH;
    }
    if(canonical[0]!=(uint8_t)STN_SHARE_VERSION){
        return STN_DATA_VERSION;
    }

    memcpy(share.work_id,canonical+1,STN_SHARE_WORK_ID_SIZE);
    share.miner.type=STN_ADDRESS_IDENTITY;
    memcpy(share.miner.identifier,canonical+33,STN_ADDRESS_ID_SIZE);

    for(i=0;i<STN_SHARE_NONCE_SIZE;++i){
        share.nonce=(share.nonce<<8)|canonical[65u+i];
    }

    *out=share;
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


stn_data_status stn_share_verify(
    const stn_share_evidence *share,
    const uint8_t *canonical_template,
    size_t template_length,
    const stn_hash_provider *provider,
    uint8_t digest[32])
{
    static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
    uint8_t work_id[32],share_target[32],hash[32];
    uint8_t block[STN_BLOCK_MAX_SIZE];
    stn_block decoded;
    stn_data_status status;
    size_t written=0;

    if(share==NULL || canonical_template==NULL || provider==NULL ||
       provider->hash==NULL || digest==NULL){
        return STN_DATA_ARGUMENT;
    }
    if(share->miner.type!=STN_ADDRESS_IDENTITY){
        return STN_DATA_TYPE;
    }
    if(template_length<STN_BLOCK_HEADER_SIZE ||
       template_length>STN_BLOCK_MAX_SIZE){
        return STN_DATA_LENGTH;
    }

    status=stn_block_decode(canonical_template,template_length,&decoded);
    if(status!=STN_DATA_OK){return status;}
    if(decoded.header.version!=STN_POW_BLOCK_VERSION){
        return STN_DATA_VERSION;
    }

    status=provider->hash(
        provider->user,
        work_domain,
        sizeof(work_domain),
        canonical_template,
        template_length,
        work_id);
    if(status!=STN_DATA_OK){
        return status==STN_DATA_UNRESOLVED ? status : STN_DATA_PROVIDER_ERROR;
    }
    if(memcmp(work_id,share->work_id,32)!=0){
        return STN_DATA_CONTENT;
    }

    status=stn_economy_share_target(
        decoded.header.reserved_target,
        share_target);
    if(status!=STN_DATA_OK){return status;}

    memcpy(block,canonical_template,template_length);
    decoded.header.reserved_work_nonce=share->nonce;
    decoded.body=block+STN_BLOCK_HEADER_SIZE;
    status=stn_block_header_encode(
        &decoded.header,
        block,
        STN_BLOCK_HEADER_SIZE,
        &written);
    if(status!=STN_DATA_OK){return status;}
    if(written!=STN_BLOCK_HEADER_SIZE){return STN_DATA_CONTENT;}

    status=stn_chain_block_id(block,template_length,provider,hash);
    if(status!=STN_DATA_OK){return status;}
    status=stn_pow_compare(hash,share_target);
    if(status==STN_DATA_OK){
        memcpy(digest,hash,32);
    }
    return status;
}
