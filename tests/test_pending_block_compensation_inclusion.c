/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pending_cleanup.h"
#include "stn_block_compensation.h"
#include "stn_transaction.h"
#include "stn_sha256.h"
#include "stn_block.h"
#include "stn_wire_internal.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    stn_pending pool={0};
    stn_hash_provider hash={stn_sha256,NULL};
    stn_block_compensation_evidence evidence={0};
    stn_transaction tx={0};
    stn_block block={0};
    stn_storage_view view={0};
    stn_block_span span={0};
    uint8_t record[STN_BLOCK_COMPENSATION_CANONICAL_SIZE];
    uint8_t encoded[STN_TX_HEADER_SIZE+STN_BLOCK_COMPENSATION_CANONICAL_SIZE];
    uint8_t body[4u+sizeof(encoded)];
    uint8_t block_bytes[STN_BLOCK_HEADER_SIZE+sizeof(body)];
    uint8_t id[32],remove[STN_PENDING_MAX_ENTRIES]={0};
    size_t encoded_length=0u,block_length=0u;

    evidence.block_id[0]=1u;
    evidence.miner.type=STN_ADDRESS_IDENTITY;
    evidence.miner.identifier[0]=2u;
    if(stn_block_compensation_encode(&evidence,record)!=STN_DATA_OK)return 1;

    tx.version=1u;
    tx.type=STN_TX_BLOCK_COMPENSATION_EVIDENCE;
    tx.record_bytes=record;
    tx.record_length=sizeof(record);
    if(stn_transaction_encode(&tx,encoded,sizeof(encoded),&encoded_length)!=STN_DATA_OK)return 1;
    if(stn_pending_insert(&pool,encoded,encoded_length,&hash,id)!=STN_PENDING_ACCEPTED)return 1;

    stn_wire_write(body,4u,encoded_length);
    memcpy(body+4u,encoded,encoded_length);
    block.header.version=3u;
    block.header.network_id[0]=1u;
    block.header.reserved_target[0]=0x7fu;
    block.header.transaction_count=1u;
    block.header.body_length=(uint32_t)sizeof(body);
    block.body=body;
    if(stn_block_body_commitment(body,sizeof(body),1u,&hash,block.header.transaction_commitment)!=STN_DATA_OK)return 1;
    if(stn_block_encode(&block,block_bytes,sizeof(block_bytes),&block_length)!=STN_DATA_OK)return 1;

    span.bytes=block_bytes;span.length=block_length;
    view.blocks=&span;view.count=1u;
    if(stn_pending_committed_inclusions(&pool,&view,&hash,remove)!=STN_DATA_OK)return 1;
    if(remove[0]!=1u)return 1;
    stn_pending_prune(&pool,remove);
    if(pool.count!=0u)return 1;
    puts("Committed pending cleanup recognizes block-compensation evidence: PASS");
    return 0;
}
