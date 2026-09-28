/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transaction_status.h"
#include "stn_transaction.h"
#include "stn_wire_internal.h"
#include <string.h>

stn_transaction_status_code stn_transaction_status_find(
    const stn_chain_context *context,
    const stn_block_span *blocks,
    size_t count,
    const uint8_t transaction_id[32],
    stn_transaction_status *out)
{
    stn_chain_state state={0};
    stn_chain_report report;
    stn_block block;
    stn_transaction tx;
    stn_transaction_status found={0};
    uint8_t id[32],block_id[32];
    size_t i,j,offset,body_length;

    if(context==NULL||blocks==NULL||count==0u||transaction_id==NULL||out==NULL)
        return STN_TRANSACTION_STATUS_ARGUMENT;

    report=stn_chain_reconstruct_history(context,blocks,count,&state);
    if(report.acceptance==STN_ACCEPTANCE_UNRESOLVED){
        stn_chain_state_release(&state);
        return STN_TRANSACTION_STATUS_UNAVAILABLE;
    }
    if(report.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT){
        stn_chain_state_release(&state);
        return STN_TRANSACTION_STATUS_PROVIDER;
    }
    stn_chain_state_release(&state);

    for(i=0u;i<count;++i){
        if(stn_block_decode(blocks[i].bytes,blocks[i].length,&block)!=STN_DATA_OK)
            return STN_TRANSACTION_STATUS_PROVIDER;
        body_length=(size_t)block.header.body_length;
        offset=0u;
        for(j=0u;j<block.header.transaction_count;++j){
            size_t n;
            if(offset>body_length||body_length-offset<4u)
                return STN_TRANSACTION_STATUS_PROVIDER;
            n=(size_t)stn_wire_read(block.body+offset,4u);
            offset+=4u;
            if(n>body_length-offset||stn_transaction_decode(block.body+offset,n,&tx)!=STN_DATA_OK)
                return STN_TRANSACTION_STATUS_PROVIDER;
            if(stn_transaction_id(block.body+offset,n,&context->hash_provider,id)!=STN_DATA_OK)
                return STN_TRANSACTION_STATUS_PROVIDER;
            if(memcmp(id,transaction_id,32u)==0){
                if(stn_chain_block_id(blocks[i].bytes,blocks[i].length,&context->hash_provider,block_id)!=STN_DATA_OK)
                    return STN_TRANSACTION_STATUS_PROVIDER;
                found.height=block.header.height;
                memcpy(found.block_id,block_id,32u);
                found.transaction_position=(uint32_t)j;
                *out=found;
                return STN_TRANSACTION_STATUS_ACCEPTED;
            }
            offset+=n;
        }
        if(offset!=body_length)return STN_TRANSACTION_STATUS_PROVIDER;
    }
    return STN_TRANSACTION_STATUS_NOT_FOUND;
}
