/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_query.h"

#include <string.h>

static int terminal_state(uint16_t state)
{
    return state==STN_CONTRACT_STATE_EXECUTED ||
        state==STN_CONTRACT_STATE_REJECTED ||
        state==STN_CONTRACT_STATE_REVOKED ||
        state==STN_CONTRACT_STATE_CLOSED;
}

static int before(const stn_contract_query_result *a,
    const stn_contract_query_result *b)
{
    int at=terminal_state(a->state),bt=terminal_state(b->state);
    if(at!=bt)return at<bt;
    if(a->created_at!=b->created_at)return a->created_at>b->created_at;
    return memcmp(a->contract_id,b->contract_id,STN_ADDRESS_ID_SIZE)<0;
}

static int contains_identity(const stn_contract_state_entry *entry,
    const uint8_t identity[STN_ADDRESS_ID_SIZE])
{
    uint16_t i;
    stn_contract_participant participant;
    for(i=0;i<entry->current.participant_count;++i){
        if(stn_contract_participant_at(&entry->current,i,&participant)!=STN_CONTRACT_OK)
            return -1;
        if(memcmp(participant.identity,identity,STN_ADDRESS_ID_SIZE)==0)return 1;
    }
    return 0;
}

stn_contract_status stn_contract_query_identity(
    const stn_contract_state_store *store,
    const uint8_t identity[STN_ADDRESS_ID_SIZE],
    stn_contract_query_result *results,size_t capacity,size_t *count)
{
    size_t i,j,n=0,limit;
    if(store==NULL || identity==NULL || results==NULL || count==NULL)
        return STN_CONTRACT_ARGUMENT;
    if(store->entry_count>store->entry_capacity ||
       (store->entry_capacity!=0u && store->entries==NULL))
        return STN_CONTRACT_ARGUMENT;
    limit=capacity<STN_CONTRACT_QUERY_MAX_RESULTS?capacity:STN_CONTRACT_QUERY_MAX_RESULTS;
    if(limit==0u)return STN_CONTRACT_CAPACITY;
    for(i=0;i<store->entry_count;++i){
        stn_contract_query_result item;
        int member=contains_identity(&store->entries[i],identity);
        if(member<0)return STN_CONTRACT_ARGUMENT;
        if(!member)continue;
        memcpy(item.contract_id,store->entries[i].contract_id,STN_ADDRESS_ID_SIZE);
        item.state=store->entries[i].current.state;
        item.type=store->entries[i].current.type;
        item.sequence=store->entries[i].current.sequence;
        item.created_at=store->entries[i].current.created_at;
        if(n<limit){results[n++]=item;}
        else if(before(&item,&results[n-1])){results[n-1]=item;}
        else continue;
        for(j=n-1;j>0 && before(&results[j],&results[j-1]);--j){
            stn_contract_query_result swap=results[j-1];
            results[j-1]=results[j];results[j]=swap;
        }
    }
    *count=n;return STN_CONTRACT_OK;
}
