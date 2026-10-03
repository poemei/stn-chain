/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_query.h"
#include "stn_wire_internal.h"

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

/* Caller holds a validated immutable snapshot throughout this read. */
stn_rpc_code stn_contract_query_handle(const stn_contract_snapshot *contracts,
    const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *written)
{
    if(written!=NULL)*written=0;
    if(q==NULL || p==NULL || written==NULL)return STN_RPC_PROVIDER;
    if(q->method==STN_RPC_CONTRACT_STATE){stn_address contract;const stn_contract_state_store *store;const stn_contract_state_entry *entry;size_t at=0;stn_contract_status status;if(q->payload==NULL||q->length!=70||stn_address_decode((const char *)q->payload,q->length,&contract)!=STN_DATA_OK||contract.type!=STN_ADDRESS_CONTRACT)return STN_RPC_INVALID;if(contracts==NULL)return STN_RPC_UNAVAILABLE;store=stn_contract_snapshot_const_state(contracts);if(store==NULL)return STN_RPC_UNAVAILABLE;status=stn_contract_state_find(store,contract.identifier,&at);if(status==STN_CONTRACT_ADDRESS_ERROR)return STN_RPC_NOT_FOUND;if(status!=STN_CONTRACT_OK||at>=store->entry_count)return STN_RPC_PROVIDER;entry=&store->entries[at];if(cap<26)return STN_RPC_CAPACITY;stn_wire_write(p,2,entry->current.state);stn_wire_write(p+2,2,entry->current.type);stn_wire_write(p+4,8,entry->current.sequence);stn_wire_write(p+12,8,entry->current.created_at);stn_wire_write(p+20,2,entry->current.participant_count);stn_wire_write(p+22,4,entry->current.terms_length);*written=26;return STN_RPC_OK;}
    if(q->method==STN_RPC_CONTRACT_LIST){stn_address identity;const stn_contract_state_store *store;stn_contract_query_result results[STN_CONTRACT_QUERY_MAX_RESULTS];size_t count=0,i;stn_contract_status status;if(q->payload==NULL||q->length!=69||stn_address_decode((const char *)q->payload,q->length,&identity)!=STN_DATA_OK||identity.type!=STN_ADDRESS_IDENTITY)return STN_RPC_INVALID;if(contracts==NULL)return STN_RPC_UNAVAILABLE;store=stn_contract_snapshot_const_state(contracts);if(store==NULL)return STN_RPC_UNAVAILABLE;status=stn_contract_query_identity(store,identity.identifier,results,STN_CONTRACT_QUERY_MAX_RESULTS,&count);if(status!=STN_CONTRACT_OK)return status==STN_CONTRACT_CAPACITY?STN_RPC_CAPACITY:STN_RPC_PROVIDER;if(cap<2u+count*STN_RPC_CONTRACT_LIST_ENTRY_SIZE)return STN_RPC_CAPACITY;stn_wire_write(p,2,count);for(i=0;i<count;++i){stn_address contract;char text[STN_ADDRESS_TEXT_CAPACITY];size_t encoded=0;uint8_t *e=p+2u+i*STN_RPC_CONTRACT_LIST_ENTRY_SIZE;memset(&contract,0,sizeof(contract));contract.type=STN_ADDRESS_CONTRACT;memcpy(contract.identifier,results[i].contract_id,STN_ADDRESS_ID_SIZE);if(stn_address_encode(&contract,text,sizeof(text),&encoded)!=STN_DATA_OK||encoded!=70u)return STN_RPC_PROVIDER;memcpy(e,text,70);stn_wire_write(e+70,2,results[i].state);stn_wire_write(e+72,2,results[i].type);stn_wire_write(e+74,8,results[i].sequence);stn_wire_write(e+82,8,results[i].created_at);}*written=2u+count*STN_RPC_CONTRACT_LIST_ENTRY_SIZE;return STN_RPC_OK;}
    return STN_RPC_METHOD;
}
