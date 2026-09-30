/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_response_acceptance.h"
#include "stn_contract_response.h"

#include <string.h>

stn_data_status stn_contract_response_accept(
    stn_contract_snapshot *snapshot,
    const stn_transaction *transaction)
{
    const stn_contract_state_store *state;
    stn_contract_response response;
    stn_contract_status status;
    size_t i;

    if(snapshot==NULL || transaction==NULL)return STN_DATA_ARGUMENT;
    if(transaction->type!=STN_TX_CONTRACT_RESPONSE)return STN_DATA_TYPE;
    status=stn_contract_response_decode(transaction->record_bytes,
        transaction->record_length,&response);
    if(status!=STN_CONTRACT_OK)return STN_DATA_CONTENT;
    if(stn_contract_response_signature_verify(&response)!=STN_CONTRACT_OK)
        return STN_DATA_CONTENT;

    state=stn_contract_snapshot_const_state(snapshot);
    if(state==NULL)return STN_DATA_ARGUMENT;
    for(i=0u;i<state->entry_count;++i){
        const stn_contract_state_entry *entry=&state->entries[i];
        stn_address contract_address;
        if(stn_contract_address(entry->canonical_draft,
            entry->canonical_draft_length,&contract_address)!=STN_CONTRACT_OK)
            return STN_DATA_CONTENT;
        if(memcmp(contract_address.identifier,response.contract_id,
            STN_ADDRESS_ID_SIZE)!=0)continue;
        if(stn_contract_response_participant_validate(&response,
            entry->canonical_draft,entry->canonical_draft_length)!=STN_CONTRACT_OK)
            return STN_DATA_CONTENT;
        status=stn_contract_snapshot_response_register(snapshot,
            transaction->record_bytes,transaction->record_length);
        if(status==STN_CONTRACT_OK)return STN_DATA_OK;
        if(status==STN_CONTRACT_CAPACITY)return STN_DATA_CAPACITY;
        if(status==STN_CONTRACT_DUPLICATE_APPROVAL)return STN_DATA_DUPLICATE;
        return STN_DATA_CONTENT;
    }
    return STN_DATA_CONTENT;
}
