/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_consensus.h"

#include <string.h>

static stn_contract_status draft_view(
    const uint8_t *bytes,size_t length,stn_contract *draft,stn_address *address)
{
    stn_contract_status status;
    if(bytes==NULL || draft==NULL)return STN_CONTRACT_ARGUMENT;
    status=stn_contract_decode(bytes,length,draft);
    if(status!=STN_CONTRACT_OK)return status;
    if(draft->state!=STN_CONTRACT_STATE_DRAFT || draft->sequence!=0u)
        return STN_CONTRACT_STATE_ERROR;
    if(address!=NULL){
        status=stn_contract_address(bytes,length,address);
        if(status!=STN_CONTRACT_OK)return status;
    }
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_vote_eligible(
    const uint8_t *canonical_draft,size_t canonical_draft_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE])
{
    stn_contract draft;
    stn_contract_participant p;
    stn_contract_status status;
    uint8_t canonical_actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint16_t i;
    if(actor==NULL)return STN_CONTRACT_ARGUMENT;
    status=draft_view(canonical_draft,canonical_draft_length,&draft,NULL);
    if(status!=STN_CONTRACT_OK)return status;
    if(stn_identity_derive(actor,canonical_actor)!=STN_IDENTITY_VALID)
        return STN_CONTRACT_AUTHORITY_ERROR;
    for(i=0;i<draft.participant_count;++i){
        status=stn_contract_participant_at(&draft,i,&p);
        if(status!=STN_CONTRACT_OK)return status;
        if(p.role==STN_CONTRACT_ROLE_APPROVER &&
           memcmp(p.identity,canonical_actor,STN_IDENTITY_PUBLIC_KEY_SIZE)==0)
            return STN_CONTRACT_OK;
    }
    return STN_CONTRACT_AUTHORITY_ERROR;
}

stn_contract_status stn_contract_vote_state_initialize(
    stn_contract_vote_state *state,const uint8_t *canonical_draft,
    size_t canonical_draft_length,uint8_t *accepted,size_t accepted_capacity)
{
    stn_contract draft;
    stn_contract_participant p,q;
    stn_address address;
    stn_contract_vote_state built;
    stn_contract_status status;
    size_t eligible=0;
    uint16_t i,j;
    if(state==NULL || (accepted_capacity!=0u && accepted==NULL))
        return STN_CONTRACT_ARGUMENT;
    status=draft_view(canonical_draft,canonical_draft_length,&draft,&address);
    if(status!=STN_CONTRACT_OK)return status;
    for(i=0;i<draft.participant_count;++i){
        status=stn_contract_participant_at(&draft,i,&p);
        if(status!=STN_CONTRACT_OK)return status;
        if(p.role!=STN_CONTRACT_ROLE_APPROVER)continue;
        for(j=0;j<i;++j){
            status=stn_contract_participant_at(&draft,j,&q);
            if(status!=STN_CONTRACT_OK)return status;
            if(q.role==STN_CONTRACT_ROLE_APPROVER &&
               memcmp(q.identity,p.identity,STN_IDENTITY_PUBLIC_KEY_SIZE)==0)
                return STN_CONTRACT_DUPLICATE_APPROVAL;
        }
        ++eligible;
    }
    if(eligible==0u)return STN_CONTRACT_AUTHORITY_ERROR;
    memset(&built,0,sizeof(built));
    memcpy(built.contract_id,address.identifier,STN_ADDRESS_ID_SIZE);
    built.accepted=accepted;
    built.accepted_capacity=accepted_capacity;
    built.eligible_count=eligible;
    built.required_count=(eligible/2u)+1u;
    *state=built;
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_vote_accept(
    stn_contract_vote_state *state,const uint8_t *canonical_draft,
    size_t canonical_draft_length,
    const uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE],int *reached)
{
    stn_contract draft;
    stn_address address;
    stn_contract_status status;
    uint8_t canonical_actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    uint8_t key[STN_CONTRACT_VOTE_KEY_SIZE];
    size_t i;
    if(state==NULL || actor==NULL || reached==NULL)return STN_CONTRACT_ARGUMENT;
    if(state->accepted_count>state->accepted_capacity ||
       (state->accepted_capacity!=0u && state->accepted==NULL) ||
       state->eligible_count==0u || state->required_count!=(state->eligible_count/2u)+1u)
        return STN_CONTRACT_ARGUMENT;
    status=draft_view(canonical_draft,canonical_draft_length,&draft,&address);
    if(status!=STN_CONTRACT_OK)return status;
    if(memcmp(address.identifier,state->contract_id,STN_ADDRESS_ID_SIZE)!=0)
        return STN_CONTRACT_ADDRESS_ERROR;
    status=stn_contract_vote_eligible(canonical_draft,canonical_draft_length,actor);
    if(status!=STN_CONTRACT_OK)return status;
    if(stn_identity_derive(actor,canonical_actor)!=STN_IDENTITY_VALID)
        return STN_CONTRACT_AUTHORITY_ERROR;
    memcpy(key,state->contract_id,STN_ADDRESS_ID_SIZE);
    memcpy(key+STN_ADDRESS_ID_SIZE,canonical_actor,STN_IDENTITY_PUBLIC_KEY_SIZE);
    for(i=0;i<state->accepted_count;++i)
        if(memcmp(state->accepted+i*STN_CONTRACT_VOTE_KEY_SIZE,key,
                  STN_CONTRACT_VOTE_KEY_SIZE)==0)
            return STN_CONTRACT_DUPLICATE_APPROVAL;
    if(state->accepted_count==state->accepted_capacity)return STN_CONTRACT_CAPACITY;
    memcpy(state->accepted+state->accepted_count*STN_CONTRACT_VOTE_KEY_SIZE,
           key,STN_CONTRACT_VOTE_KEY_SIZE);
    ++state->accepted_count;
    *reached=state->accepted_count>=state->required_count ? 1 : 0;
    return STN_CONTRACT_OK;
}
