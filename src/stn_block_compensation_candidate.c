/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_candidate.h"
#include "stn_issuance.h"

stn_data_status stn_block_compensation_candidate_apply(
    const stn_transaction *evidence_tx,
    const stn_transaction *issuance_tx,
    stn_block_compensation_replay *replay,
    const stn_compensation_state *compensation,
    stn_economic_state *economy,
    stn_block_compensation_evidence *accepted_evidence)
{
    stn_block_compensation_evidence evidence;
    stn_issuance_record issuance;
    stn_data_status status;

    if(evidence_tx==NULL || issuance_tx==NULL || replay==NULL ||
       compensation==NULL || economy==NULL)return STN_DATA_ARGUMENT;
    if(evidence_tx->version!=1u ||
       evidence_tx->type!=STN_TX_BLOCK_COMPENSATION_EVIDENCE ||
       evidence_tx->record_bytes==NULL ||
       evidence_tx->record_length!=STN_TX_BLOCK_COMPENSATION_SIZE)return STN_DATA_TYPE;
    if(issuance_tx->version!=1u || issuance_tx->type!=STN_TX_ISSUANCE ||
       issuance_tx->record_bytes==NULL ||
       issuance_tx->record_length!=STN_TX_ISSUANCE_SIZE)return STN_DATA_TYPE;

    status=stn_block_compensation_decode(evidence_tx->record_bytes,
        evidence_tx->record_length,&evidence);
    if(status!=STN_DATA_OK)return status;
    status=stn_issuance_decode(issuance_tx->record_bytes,
        issuance_tx->record_length,&issuance);
    if(status!=STN_DATA_OK)return status;
    if(issuance.reason!=STN_ISSUANCE_REASON_BLOCK)return STN_DATA_TYPE;

    status=stn_block_compensation_accept(replay,&evidence,&issuance,
        compensation,economy);
    if(status!=STN_DATA_OK)return status;
    if(accepted_evidence!=NULL)*accepted_evidence=evidence;
    return STN_DATA_OK;
}
