/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_reward.h"
#include <string.h>

stn_data_status stn_block_reward_build(
    const uint8_t block_id[STN_BLOCK_COMPENSATION_BLOCK_ID_SIZE],
    const stn_address *miner,
    const stn_compensation_state *compensation,
    stn_block_compensation_evidence *evidence,
    stn_issuance_record *issuance)
{
    stn_block_compensation_evidence e;
    stn_issuance_record i;
    stn_address wallet;
    stn_data_status status;
    size_t z;
    unsigned nonzero=0;

    if(block_id==NULL || miner==NULL || compensation==NULL ||
       evidence==NULL || issuance==NULL)return STN_DATA_ARGUMENT;
    if(miner->type!=STN_ADDRESS_IDENTITY)return STN_DATA_TYPE;
    for(z=0;z<STN_BLOCK_COMPENSATION_BLOCK_ID_SIZE;++z)nonzero|=block_id[z];
    if(nonzero==0)return STN_DATA_CONTENT;

    status=stn_compensation_state_lookup(compensation,miner,&wallet);
    if(status!=STN_DATA_OK)return status;
    if(wallet.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;

    memset(&e,0,sizeof(e));
    e.version=STN_BLOCK_COMPENSATION_VERSION;
    memcpy(e.block_id,block_id,sizeof(e.block_id));
    e.miner=*miner;

    memset(&i,0,sizeof(i));
    i.reason=STN_ISSUANCE_REASON_BLOCK;
    i.units=STN_ISSUANCE_BLOCK_UNITS;
    status=stn_block_compensation_id(&e,i.evidence_id);
    if(status!=STN_DATA_OK)return status;
    i.destination.mining_identity=*miner;
    i.destination.wallet=wallet;

    *evidence=e;
    *issuance=i;
    return STN_DATA_OK;
}

stn_data_status stn_block_reward_encode_transactions(
    const stn_block_compensation_evidence *evidence,
    const stn_issuance_record *issuance,
    uint8_t *evidence_tx,size_t evidence_capacity,size_t *evidence_written,
    uint8_t *issuance_tx,size_t issuance_capacity,size_t *issuance_written)
{
    uint8_t evidence_bytes[STN_BLOCK_COMPENSATION_CANONICAL_SIZE];
    uint8_t issuance_bytes[STN_ISSUANCE_CANONICAL_SIZE];
    stn_transaction tx;
    stn_data_status status;
    size_t written;

    if(evidence_written!=NULL)*evidence_written=0u;
    if(issuance_written!=NULL)*issuance_written=0u;
    if(evidence==NULL || issuance==NULL || evidence_tx==NULL || issuance_tx==NULL ||
       evidence_written==NULL || issuance_written==NULL)return STN_DATA_ARGUMENT;

    status=stn_block_compensation_encode(evidence,evidence_bytes,sizeof(evidence_bytes),&written);
    if(status!=STN_DATA_OK)return status;
    if(written!=sizeof(evidence_bytes))return STN_DATA_CONTENT;
    memset(&tx,0,sizeof(tx));
    tx.version=1u;tx.type=STN_TX_BLOCK_COMPENSATION_EVIDENCE;
    tx.record_bytes=evidence_bytes;tx.record_length=(uint32_t)sizeof(evidence_bytes);
    status=stn_transaction_encode(&tx,evidence_tx,evidence_capacity,evidence_written);
    if(status!=STN_DATA_OK)return status;

    status=stn_issuance_encode(issuance,issuance_bytes,sizeof(issuance_bytes),&written);
    if(status!=STN_DATA_OK){*evidence_written=0u;return status;}
    if(written!=sizeof(issuance_bytes)){*evidence_written=0u;return STN_DATA_CONTENT;}
    tx.type=STN_TX_ISSUANCE;tx.record_bytes=issuance_bytes;tx.record_length=(uint32_t)sizeof(issuance_bytes);
    status=stn_transaction_encode(&tx,issuance_tx,issuance_capacity,issuance_written);
    if(status!=STN_DATA_OK){*evidence_written=0u;*issuance_written=0u;return status;}
    return STN_DATA_OK;
}
