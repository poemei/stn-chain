/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_internal_miner.h"
#include <string.h>

stn_data_status stn_internal_miner_search(
    const uint8_t template_header[STN_SHARE_TEMPLATE_HEADER_SIZE],
    const uint8_t work_id[STN_SHARE_WORK_ID_SIZE],
    const stn_address *miner,
    uint64_t *next_nonce,
    uint64_t nonce_budget,
    unsigned duty_permille,
    const stn_hash_provider *provider,
    stn_share_evidence *evidence,
    stn_internal_miner_result *result)
{
    static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
    static const uint8_t block_domain[]="STN-CHAIN:BLOCK:ID:1";
    stn_block_header base,trial;
    uint8_t expected_work[32],share_target[32],encoded[STN_SHARE_TEMPLATE_HEADER_SIZE],hash[32];
    uint64_t nonce,i;
    size_t written;
    stn_data_status status;

    if(template_header==NULL || work_id==NULL || miner==NULL || next_nonce==NULL ||
       provider==NULL || provider->hash==NULL || evidence==NULL || result==NULL)
        return STN_DATA_ARGUMENT;
    if(miner->type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
    if(duty_permille==0u || duty_permille>STN_INTERNAL_MINER_MAX_DUTY_PERMILLE)
        return STN_DATA_CONTENT;

    status=stn_block_header_decode(template_header,STN_SHARE_TEMPLATE_HEADER_SIZE,&base);
    if(status!=STN_DATA_OK)return status;
    if(base.version!=STN_POW_BLOCK_VERSION)return STN_DATA_VERSION;
    if(base.reserved_work_nonce!=0u)return STN_DATA_CONTENT;
    status=stn_target_validate(base.reserved_target,32u);
    if(status!=STN_DATA_OK)return status;
    status=stn_economy_share_target(base.reserved_target,share_target);
    if(status!=STN_DATA_OK)return status;

    status=provider->hash(provider->user,work_domain,sizeof(work_domain),
        template_header,STN_SHARE_TEMPLATE_HEADER_SIZE,expected_work);
    if(status!=STN_DATA_OK)
        return status==STN_DATA_UNRESOLVED ? status : STN_DATA_PROVIDER_ERROR;
    if(memcmp(expected_work,work_id,32u)!=0)return STN_DATA_CONTENT;

    nonce=*next_nonce;
    *result=STN_INTERNAL_MINER_IDLE;
    if(nonce_budget==0u)return STN_DATA_OK;

    for(i=0u;i<nonce_budget;++i){
        trial=base;
        trial.reserved_work_nonce=nonce;
        written=0u;
        status=stn_block_header_encode(&trial,encoded,sizeof(encoded),&written);
        if(status!=STN_DATA_OK)return status;
        if(written!=sizeof(encoded))return STN_DATA_CONTENT;
        status=provider->hash(provider->user,block_domain,sizeof(block_domain),
            encoded,sizeof(encoded),hash);
        if(status!=STN_DATA_OK)
            return status==STN_DATA_UNRESOLVED ? status : STN_DATA_PROVIDER_ERROR;

        if(stn_pow_compare(hash,base.reserved_target)==STN_DATA_OK ||
           stn_pow_compare(hash,share_target)==STN_DATA_OK){
            stn_share_evidence found={0};
            memcpy(found.work_id,work_id,32u);
            found.miner=*miner;
            found.nonce=nonce;
            memcpy(found.template_header,template_header,sizeof(found.template_header));
            memcpy(found.body_commitment,base.transaction_commitment,
                sizeof(found.body_commitment));
            *evidence=found;
            *result=stn_pow_compare(hash,base.reserved_target)==STN_DATA_OK ?
                STN_INTERNAL_MINER_BLOCK : STN_INTERNAL_MINER_SHARE;
            *next_nonce=nonce==UINT64_MAX ? 0u : nonce+1u;
            return STN_DATA_OK;
        }
        if(nonce==UINT64_MAX){
            *next_nonce=0u;
            *result=STN_INTERNAL_MINER_EXHAUSTED;
            return STN_DATA_OK;
        }
        ++nonce;
    }
    *next_nonce=nonce;
    *result=STN_INTERNAL_MINER_IDLE;
    return STN_DATA_OK;
}


stn_data_status stn_internal_miner_worker_step(
    stn_internal_miner_worker *worker,
    stn_internal_miner_result *result)
{
    uint8_t template_response[STN_MINING_PREFIX+STN_BLOCK_MAX_SIZE];
    uint8_t share_id[STN_SHARE_ID_SIZE];
    stn_share_evidence evidence={0};
    stn_internal_miner_result found=STN_INTERNAL_MINER_IDLE;
    size_t response_length=0u,block_length;
    stn_rpc_code rpc;
    stn_data_status status;

    if(worker==NULL || result==NULL || worker->service==NULL ||
       worker->service->chain==NULL)
        return STN_DATA_ARGUMENT;
    if(worker->miner.type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
    if(worker->duty_permille==0u ||
       worker->duty_permille>STN_INTERNAL_MINER_MAX_DUTY_PERMILLE ||
       worker->nonce_budget==0u)
        return STN_DATA_CONTENT;

    *result=STN_INTERNAL_MINER_IDLE;
    rpc=stn_mining_template_local(worker->service,template_response,
        sizeof(template_response),&response_length);
    if(rpc!=STN_RPC_OK)return rpc==STN_RPC_UNAVAILABLE || rpc==STN_RPC_STALE ?
        STN_DATA_UNRESOLVED : STN_DATA_CONTENT;
    if(response_length<STN_MINING_PREFIX+STN_BLOCK_HEADER_SIZE)
        return STN_DATA_CONTENT;
    block_length=stn_wire_read(template_response+64u,4u);
    if(block_length<STN_BLOCK_HEADER_SIZE || block_length>STN_BLOCK_MAX_SIZE ||
       response_length!=STN_MINING_PREFIX+block_length)
        return STN_DATA_CONTENT;

    status=stn_internal_miner_search(template_response+STN_MINING_PREFIX,
        template_response+32u,&worker->miner,&worker->next_nonce,
        worker->nonce_budget,worker->duty_permille,
        &worker->service->chain->hash_provider,&evidence,&found);
    if(status!=STN_DATA_OK)return status;

    if(found==STN_INTERNAL_MINER_SHARE){
        rpc=stn_mining_submit_share_local(worker->service,&evidence,share_id);
        if(rpc==STN_RPC_STALE){worker->next_nonce=0u;return STN_DATA_UNRESOLVED;}
        if(rpc!=STN_RPC_OK)return STN_DATA_CONTENT;
    }else if(found==STN_INTERNAL_MINER_BLOCK){
        stn_block_header solved;
        size_t written=0u;
        uint8_t *block=template_response+STN_MINING_PREFIX;
        if(stn_block_header_decode(block,STN_BLOCK_HEADER_SIZE,&solved)!=STN_DATA_OK)
            return STN_DATA_CONTENT;
        solved.reserved_work_nonce=evidence.nonce;
        if(stn_block_header_encode(&solved,block,STN_BLOCK_HEADER_SIZE,&written)!=STN_DATA_OK ||
           written!=STN_BLOCK_HEADER_SIZE)
            return STN_DATA_CONTENT;
        rpc=stn_mining_submit_work_local(worker->service,&worker->miner,
            template_response,evidence.work_id,block,block_length);
        if(rpc==STN_RPC_STALE){worker->next_nonce=0u;return STN_DATA_UNRESOLVED;}
        if(rpc!=STN_RPC_OK)return STN_DATA_CONTENT;
        worker->next_nonce=0u;
    }else if(found==STN_INTERNAL_MINER_EXHAUSTED){
        worker->next_nonce=0u;
    }

    *result=found;
    return STN_DATA_OK;
}
