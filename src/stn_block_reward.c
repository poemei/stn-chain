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
