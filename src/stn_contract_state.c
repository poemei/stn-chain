/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_state.h"

#include <string.h>

void stn_contract_state_initialize(
    stn_contract_state_store *state,stn_contract_state_entry *entries,
    size_t entry_capacity,uint8_t *votes,size_t vote_capacity)
{
    if(state==NULL)return;
    memset(state,0,sizeof(*state));
    state->entries=entries;state->entry_capacity=entry_capacity;
    state->votes=votes;state->vote_capacity=vote_capacity;
}

stn_contract_status stn_contract_state_find(
    const stn_contract_state_store *state,
    const uint8_t contract_id[STN_ADDRESS_ID_SIZE],size_t *index)
{
    size_t i;
    if(state==NULL || contract_id==NULL || index==NULL)return STN_CONTRACT_ARGUMENT;
    if(state->entry_count>state->entry_capacity ||
       (state->entry_capacity!=0u && state->entries==NULL))
        return STN_CONTRACT_ARGUMENT;
    for(i=0;i<state->entry_count;++i){
        if(memcmp(state->entries[i].contract_id,contract_id,
                  STN_ADDRESS_ID_SIZE)==0){
            *index=i;return STN_CONTRACT_OK;
        }
    }
    return STN_CONTRACT_ADDRESS_ERROR;
}

stn_contract_status stn_contract_state_register(
    stn_contract_state_store *state,const uint8_t *canonical_draft,
    size_t canonical_draft_length,size_t *index)
{
    stn_contract_state_entry entry;
    stn_contract_vote_state votes;
    stn_contract_status status;
    stn_contract draft;
    stn_address address;
    size_t existing;
    if(state==NULL || canonical_draft==NULL || index==NULL)
        return STN_CONTRACT_ARGUMENT;
    if(state->entry_count>state->entry_capacity ||
       state->vote_count>state->vote_capacity ||
       (state->entry_capacity!=0u && state->entries==NULL) ||
       (state->vote_capacity!=0u && state->votes==NULL))
        return STN_CONTRACT_ARGUMENT;
    status=stn_contract_decode(canonical_draft,canonical_draft_length,&draft);
    if(status!=STN_CONTRACT_OK)return status;
    if(draft.state!=STN_CONTRACT_STATE_DRAFT || draft.sequence!=0u)
        return STN_CONTRACT_STATE_ERROR;
    status=stn_contract_address(canonical_draft,canonical_draft_length,&address);
    if(status!=STN_CONTRACT_OK)return status;
    if(stn_contract_state_find(state,address.identifier,&existing)==STN_CONTRACT_OK)
        return STN_CONTRACT_ADDRESS_ERROR;
    if(state->entry_count==state->entry_capacity)return STN_CONTRACT_CAPACITY;
    if(state->vote_count>state->vote_capacity-draft.participant_count)
        return STN_CONTRACT_CAPACITY;
    memset(&entry,0,sizeof(entry));
    status=stn_contract_vote_state_initialize(&votes,canonical_draft,
        canonical_draft_length,
        state->votes+state->vote_count*STN_CONTRACT_VOTE_KEY_SIZE,
        draft.participant_count);
    if(status!=STN_CONTRACT_OK)return status;
    memcpy(entry.contract_id,address.identifier,STN_ADDRESS_ID_SIZE);
    entry.canonical_draft=canonical_draft;
    entry.canonical_draft_length=canonical_draft_length;
    entry.current=draft;
    entry.vote_offset=state->vote_count;
    entry.eligible_count=votes.eligible_count;
    entry.required_count=votes.required_count;
    state->entries[state->entry_count]=entry;
    *index=state->entry_count++;
    state->vote_count+=draft.participant_count;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_state_apply_vote(
    stn_contract_state_store *state,size_t index,
    const uint8_t *canonical_current,size_t canonical_current_length,
    uint64_t expected_sequence,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE])
{
    stn_contract_state_entry *entry;
    stn_contract_vote_state votes;
    stn_contract current,next;
    stn_contract_status status;
    if(state==NULL || canonical_current==NULL || actor==NULL ||
       index>=state->entry_count)return STN_CONTRACT_ARGUMENT;
    entry=&state->entries[index];
    status=stn_contract_lineage_validate(entry->canonical_draft,
        entry->canonical_draft_length,canonical_current,
        canonical_current_length);
    if(status!=STN_CONTRACT_OK)return status;
    status=stn_contract_decode(canonical_current,canonical_current_length,&current);
    if(status!=STN_CONTRACT_OK)return status;
    if(current.sequence!=entry->current.sequence ||
       current.state!=entry->current.state)
        return STN_CONTRACT_STATE_ERROR;
    votes.accepted=state->votes+entry->vote_offset*STN_CONTRACT_VOTE_KEY_SIZE;
    votes.accepted_count=entry->vote_count;
    votes.accepted_capacity=entry->eligible_count;
    votes.eligible_count=entry->eligible_count;
    votes.required_count=entry->required_count;
    memcpy(votes.contract_id,entry->contract_id,STN_ADDRESS_ID_SIZE);
    status=stn_contract_vote_apply(&current,expected_sequence,
        entry->canonical_draft,entry->canonical_draft_length,actor,&votes,&next);
    if(status!=STN_CONTRACT_OK)return status;
    /*
     * next may borrow participants/terms from canonical_current. The store must
     * retain its own stable decoded DRAFT spans so later encoding and lineage
     * checks never depend on a caller's transient current buffer.
     */
    /*
     * The registered DRAFT is the store's canonical backing representation.
     * stn_contract_encode() consumes native participants, while decoded
     * Contract views expose packed participant_bytes. Re-decode the DRAFT and
     * materialize its bounded participant set into store-owned entry storage
     * before publishing mutable state.
     */
    next.participants=entry->current.participants;
    next.participant_bytes=entry->current.participant_bytes;
    next.terms=entry->current.terms;
    entry->current=next;
    entry->vote_count=votes.accepted_count;
    return STN_CONTRACT_OK;
}
