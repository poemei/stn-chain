/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pending_cleanup.h"
#include "stn_wire_internal.h"
#include <string.h>

stn_data_status stn_pending_committed_inclusions(
    const stn_pending *pool,
    const stn_storage_view *candidate,
    const stn_hash_provider *hash,
    uint8_t remove[STN_PENDING_MAX_ENTRIES])
{
    size_t i,j,k,offset;

    if(pool==NULL || candidate==NULL || hash==NULL || hash->hash==NULL ||
       remove==NULL || (candidate->count!=0u && candidate->blocks==NULL)){
        return STN_DATA_ARGUMENT;
    }

    memset(remove,0,STN_PENDING_MAX_ENTRIES);

    for(i=0u;i<candidate->count;++i){
        stn_block block;
        if(stn_block_decode(candidate->blocks[i].bytes,
                candidate->blocks[i].length,&block)!=STN_DATA_OK){
            return STN_DATA_CONTENT;
        }

        offset=0u;
        for(j=0u;j<block.header.transaction_count;++j){
            size_t n;
            uint8_t id[32];

            if(offset+4u>block.header.body_length){return STN_DATA_CONTENT;}
            n=(size_t)stn_wire_read(block.body+offset,4u);
            offset+=4u;
            if(n>block.header.body_length-offset){return STN_DATA_CONTENT;}
            if(stn_transaction_validate_structure(block.body+offset,n)!=STN_DATA_OK){
                return STN_DATA_CONTENT;
            }
            if(stn_transaction_id(block.body+offset,n,hash,id)!=STN_DATA_OK){
                return STN_DATA_PROVIDER_ERROR;
            }

            for(k=0u;k<pool->count;++k){
                if(memcmp(pool->entries[k].id,id,32u)==0){remove[k]=1u;break;}
            }
            offset+=n;
        }

        if(offset!=block.header.body_length){return STN_DATA_CONTENT;}
    }

    return STN_DATA_OK;
}
