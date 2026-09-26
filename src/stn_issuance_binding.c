/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_issuance_binding.h"
#include <string.h>

stn_data_status stn_issuance_bind_share(
    const stn_issuance_record *issuance,
    const stn_share_evidence *share,
    const stn_compensation_state *compensation)
{
    uint8_t share_id[STN_SHARE_ID_SIZE];
    stn_address wallet;
    stn_data_status status;

    if(issuance==NULL || share==NULL || compensation==NULL)return STN_DATA_ARGUMENT;
    if(issuance->reason!=STN_ISSUANCE_REASON_SHARE ||
       issuance->units!=STN_ISSUANCE_SHARE_UNITS)return STN_DATA_CONTENT;
    if(share->miner.type!=STN_ADDRESS_IDENTITY ||
       issuance->destination.mining_identity.type!=STN_ADDRESS_IDENTITY ||
       issuance->destination.wallet.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if(memcmp(issuance->destination.mining_identity.identifier,
              share->miner.identifier,STN_ADDRESS_ID_SIZE)!=0)return STN_DATA_CONTENT;

    status=stn_share_id(share,share_id);
    if(status!=STN_DATA_OK)return status;
    if(memcmp(issuance->evidence_id,share_id,STN_SHARE_ID_SIZE)!=0)return STN_DATA_CONTENT;

    status=stn_compensation_state_lookup(compensation,&share->miner,&wallet);
    if(status!=STN_DATA_OK)return status;
    if(memcmp(wallet.identifier,issuance->destination.wallet.identifier,
              STN_ADDRESS_ID_SIZE)!=0)return STN_DATA_CONTENT;
    return STN_DATA_OK;
}

stn_data_status stn_issuance_bind_block(
    const stn_issuance_record *issuance,
    const stn_block_compensation_evidence *evidence,
    const stn_compensation_state *compensation)
{
    uint8_t evidence_id[STN_BLOCK_COMPENSATION_ID_SIZE];
    stn_address wallet;
    stn_data_status status;

    if(issuance==NULL || evidence==NULL || compensation==NULL)return STN_DATA_ARGUMENT;
    if(issuance->reason!=STN_ISSUANCE_REASON_BLOCK ||
       issuance->units!=STN_ISSUANCE_BLOCK_UNITS)return STN_DATA_CONTENT;
    if(evidence->miner.type!=STN_ADDRESS_IDENTITY ||
       issuance->destination.mining_identity.type!=STN_ADDRESS_IDENTITY ||
       issuance->destination.wallet.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if(memcmp(issuance->destination.mining_identity.identifier,
              evidence->miner.identifier,STN_ADDRESS_ID_SIZE)!=0)return STN_DATA_CONTENT;

    status=stn_block_compensation_id(evidence,evidence_id);
    if(status!=STN_DATA_OK)return status;
    if(memcmp(issuance->evidence_id,evidence_id,sizeof(evidence_id))!=0)return STN_DATA_CONTENT;

    status=stn_compensation_state_lookup(compensation,&evidence->miner,&wallet);
    if(status!=STN_DATA_OK)return status;
    if(memcmp(wallet.identifier,issuance->destination.wallet.identifier,
              STN_ADDRESS_ID_SIZE)!=0)return STN_DATA_CONTENT;
    return STN_DATA_OK;
}
