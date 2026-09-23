/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_lineage.h"

#include <string.h>

static stn_contract_status views(
    const uint8_t *draft_bytes,size_t draft_length,
    const uint8_t *current_bytes,size_t current_length,
    stn_contract *draft,stn_contract *current)
{
    stn_contract_status status;
    size_t participant_bytes;
    if(draft_bytes==NULL || current_bytes==NULL || draft==NULL || current==NULL)
        return STN_CONTRACT_ARGUMENT;
    status=stn_contract_decode(draft_bytes,draft_length,draft);
    if(status!=STN_CONTRACT_OK)return status;
    if(draft->state!=STN_CONTRACT_STATE_DRAFT || draft->sequence!=0u)
        return STN_CONTRACT_STATE_ERROR;
    status=stn_contract_decode(current_bytes,current_length,current);
    if(status!=STN_CONTRACT_OK)return status;
    if(draft->version!=current->version ||
       draft->type!=current->type ||
       draft->created_at!=current->created_at ||
       draft->participant_count!=current->participant_count ||
       draft->terms_length!=current->terms_length)
        return STN_CONTRACT_ADDRESS_ERROR;
    participant_bytes=(size_t)draft->participant_count*
        STN_CONTRACT_PARTICIPANT_SIZE;
    if(participant_bytes!=0u &&
       memcmp(draft->participant_bytes,current->participant_bytes,
              participant_bytes)!=0)
        return STN_CONTRACT_ADDRESS_ERROR;
    if(draft->terms_length!=0u &&
       memcmp(draft->terms,current->terms,draft->terms_length)!=0)
        return STN_CONTRACT_ADDRESS_ERROR;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_lineage_validate(
    const uint8_t *canonical_draft,size_t canonical_draft_length,
    const uint8_t *canonical_current,size_t canonical_current_length)
{
    stn_contract draft,current;
    return views(canonical_draft,canonical_draft_length,
        canonical_current,canonical_current_length,&draft,&current);
}

stn_contract_status stn_contract_lineage_id(
    const uint8_t *canonical_draft,size_t canonical_draft_length,
    const uint8_t *canonical_current,size_t canonical_current_length,
    uint8_t contract_id[STN_ADDRESS_ID_SIZE])
{
    stn_contract draft,current;
    stn_address address;
    stn_contract_status status;
    if(contract_id==NULL)return STN_CONTRACT_ARGUMENT;
    status=views(canonical_draft,canonical_draft_length,
        canonical_current,canonical_current_length,&draft,&current);
    if(status!=STN_CONTRACT_OK)return status;
    status=stn_contract_address(canonical_draft,canonical_draft_length,&address);
    if(status!=STN_CONTRACT_OK)return status;
    memcpy(contract_id,address.identifier,STN_ADDRESS_ID_SIZE);
    return STN_CONTRACT_OK;
}
