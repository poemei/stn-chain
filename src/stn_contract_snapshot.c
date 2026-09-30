/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_snapshot.h"

#include <stdlib.h>
#include <string.h>

typedef struct stn_contract_snapshot_response_entry {
    uint8_t contract_id[STN_ADDRESS_ID_SIZE];
    uint8_t actor[STN_IDENTITY_PUBLIC_KEY_SIZE];
    size_t text_offset;
    uint32_t text_length;
} stn_contract_snapshot_response_entry;

struct stn_contract_snapshot {
    size_t references;
    stn_contract_state_store state;
    stn_contract_state_entry entries[STN_CONTRACT_STATE_MAX_CONTRACTS];
    uint8_t votes[STN_CONTRACT_STATE_MAX_VOTES * STN_CONTRACT_VOTE_KEY_SIZE];
    uint8_t *draft_bytes;
    size_t draft_bytes_length;
    stn_contract_snapshot_response_entry responses[STN_CONTRACT_SNAPSHOT_MAX_RESPONSES];
    size_t response_count;
    uint8_t *response_text_bytes;
    size_t response_text_bytes_length;
};

static int bind(stn_contract_snapshot *snapshot,const uint8_t *old_drafts)
{
    size_t i;
    snapshot->state.entries=snapshot->entries;
    snapshot->state.entry_capacity=STN_CONTRACT_STATE_MAX_CONTRACTS;
    snapshot->state.votes=snapshot->votes;
    snapshot->state.vote_capacity=STN_CONTRACT_STATE_MAX_VOTES;
    for(i=0;i<snapshot->state.entry_count;++i){
        stn_contract_state_entry *entry=&snapshot->entries[i];
        size_t offset;
        stn_contract decoded;
        if(entry->canonical_draft==NULL || old_drafts==NULL ||
           entry->canonical_draft<old_drafts ||
           (size_t)(entry->canonical_draft-old_drafts)>snapshot->draft_bytes_length)
            return 0;
        offset=(size_t)(entry->canonical_draft-old_drafts);
        if(entry->canonical_draft_length>snapshot->draft_bytes_length-offset)
            return 0;
        entry->canonical_draft=snapshot->draft_bytes+offset;
        if(stn_contract_decode(entry->canonical_draft,
            entry->canonical_draft_length,&decoded)!=STN_CONTRACT_OK)
            return 0;
        decoded.sequence=entry->current.sequence;
        decoded.state=entry->current.state;
        entry->current=decoded;
    }
    return 1;
}

stn_contract_snapshot *stn_contract_snapshot_create(void)
{
    stn_contract_snapshot *snapshot=calloc(1,sizeof(*snapshot));
    if(snapshot==NULL)return NULL;
    snapshot->references=1u;
    stn_contract_state_initialize(&snapshot->state,snapshot->entries,
        STN_CONTRACT_STATE_MAX_CONTRACTS,snapshot->votes,
        STN_CONTRACT_STATE_MAX_VOTES);
    return snapshot;
}

stn_contract_snapshot *stn_contract_snapshot_clone(
    const stn_contract_snapshot *source)
{
    stn_contract_snapshot *snapshot;
    if(source==NULL || source->references==0u)return NULL;
    snapshot=malloc(sizeof(*snapshot));
    if(snapshot==NULL)return NULL;
    memcpy(snapshot,source,sizeof(*snapshot));
    snapshot->references=1u;
    snapshot->draft_bytes=NULL;
    snapshot->response_text_bytes=NULL;
    if(source->draft_bytes_length!=0u){
        snapshot->draft_bytes=malloc(source->draft_bytes_length);
        if(snapshot->draft_bytes==NULL){free(snapshot);return NULL;}
        memcpy(snapshot->draft_bytes,source->draft_bytes,source->draft_bytes_length);
    }
    if(source->response_text_bytes_length!=0u){
        snapshot->response_text_bytes=malloc(source->response_text_bytes_length);
        if(snapshot->response_text_bytes==NULL){free(snapshot->draft_bytes);free(snapshot);return NULL;}
        memcpy(snapshot->response_text_bytes,source->response_text_bytes,
            source->response_text_bytes_length);
    }
    if(!bind(snapshot,source->draft_bytes)){
        free(snapshot->response_text_bytes);free(snapshot->draft_bytes);free(snapshot);return NULL;
    }
    return snapshot;
}

stn_contract_snapshot *stn_contract_snapshot_share(
    stn_contract_snapshot *snapshot)
{
    if(snapshot==NULL || snapshot->references==0u ||
       snapshot->references==SIZE_MAX)return NULL;
    ++snapshot->references;
    return snapshot;
}

void stn_contract_snapshot_release(stn_contract_snapshot *snapshot)
{
    if(snapshot==NULL)return;
    if(snapshot->references==0u)abort();
    if(--snapshot->references==0u){
        free(snapshot->response_text_bytes);
        free(snapshot->draft_bytes);
        free(snapshot);
    }
}

stn_contract_state_store *stn_contract_snapshot_state(
    stn_contract_snapshot *snapshot)
{
    return snapshot==NULL ? NULL : &snapshot->state;
}

const stn_contract_state_store *stn_contract_snapshot_const_state(
    const stn_contract_snapshot *snapshot)
{
    return snapshot==NULL ? NULL : &snapshot->state;
}

stn_contract_status stn_contract_snapshot_register(
    stn_contract_snapshot *snapshot,const uint8_t *canonical_draft,
    size_t canonical_draft_length,size_t *index)
{
    uint8_t *grown,*old_drafts;
    size_t old_length;
    stn_contract_status status;
    if(snapshot==NULL || canonical_draft==NULL || index==NULL)
        return STN_CONTRACT_ARGUMENT;
    if(canonical_draft_length>STN_CONTRACT_MAX_SIZE ||
       snapshot->draft_bytes_length>SIZE_MAX-canonical_draft_length)
        return STN_CONTRACT_CAPACITY;
    old_drafts=snapshot->draft_bytes;
    old_length=snapshot->draft_bytes_length;
    grown=malloc(old_length+canonical_draft_length);
    if(grown==NULL)return STN_CONTRACT_CAPACITY;
    if(old_length!=0u)memcpy(grown,old_drafts,old_length);
    memcpy(grown+old_length,canonical_draft,canonical_draft_length);
    snapshot->draft_bytes=grown;
    snapshot->draft_bytes_length=old_length+canonical_draft_length;
    if(old_length!=0u && !bind(snapshot,old_drafts)){
        snapshot->draft_bytes=old_drafts;snapshot->draft_bytes_length=old_length;
        free(grown);return STN_CONTRACT_ARGUMENT;
    }
    status=stn_contract_state_register(&snapshot->state,grown+old_length,
        canonical_draft_length,index);
    if(status!=STN_CONTRACT_OK){
        snapshot->draft_bytes=old_drafts;snapshot->draft_bytes_length=old_length;
        if(old_length!=0u && !bind(snapshot,grown))abort();
        free(grown);return status;
    }
    free(old_drafts);
    return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_snapshot_response_register(
    stn_contract_snapshot *snapshot,const uint8_t *canonical_response,
    size_t canonical_response_length)
{
    stn_contract_response decoded;
    stn_contract_snapshot_response_entry *entry;
    uint8_t *grown;
    size_t i,old_length;
    stn_contract_status status;
    if(snapshot==NULL || canonical_response==NULL)return STN_CONTRACT_ARGUMENT;
    status=stn_contract_response_decode(canonical_response,canonical_response_length,&decoded);
    if(status!=STN_CONTRACT_OK)return status;
    if(snapshot->response_count>=STN_CONTRACT_SNAPSHOT_MAX_RESPONSES)
        return STN_CONTRACT_CAPACITY;
    for(i=0u;i<snapshot->response_count;++i){
        const stn_contract_snapshot_response_entry *existing=&snapshot->responses[i];
        if(memcmp(existing->contract_id,decoded.contract_id,STN_ADDRESS_ID_SIZE)==0 &&
           memcmp(existing->actor,decoded.actor,STN_IDENTITY_PUBLIC_KEY_SIZE)==0 &&
           existing->text_length==decoded.text_length &&
           memcmp(snapshot->response_text_bytes+existing->text_offset,
               decoded.text,decoded.text_length)==0)
            return STN_CONTRACT_DUPLICATE;
    }
    old_length=snapshot->response_text_bytes_length;
    if(old_length>SIZE_MAX-decoded.text_length)return STN_CONTRACT_CAPACITY;
    grown=malloc(old_length+decoded.text_length);
    if(grown==NULL)return STN_CONTRACT_CAPACITY;
    if(old_length!=0u)memcpy(grown,snapshot->response_text_bytes,old_length);
    memcpy(grown+old_length,decoded.text,decoded.text_length);
    free(snapshot->response_text_bytes);
    snapshot->response_text_bytes=grown;
    snapshot->response_text_bytes_length=old_length+decoded.text_length;
    entry=&snapshot->responses[snapshot->response_count++];
    memcpy(entry->contract_id,decoded.contract_id,STN_ADDRESS_ID_SIZE);
    memcpy(entry->actor,decoded.actor,STN_IDENTITY_PUBLIC_KEY_SIZE);
    entry->text_offset=old_length;
    entry->text_length=decoded.text_length;
    return STN_CONTRACT_OK;
}

size_t stn_contract_snapshot_response_count(
    const stn_contract_snapshot *snapshot,
    const uint8_t contract_id[STN_ADDRESS_ID_SIZE])
{
    size_t i,count=0u;
    if(snapshot==NULL || contract_id==NULL)return 0u;
    for(i=0u;i<snapshot->response_count;++i)
        if(memcmp(snapshot->responses[i].contract_id,contract_id,STN_ADDRESS_ID_SIZE)==0)
            ++count;
    return count;
}

stn_contract_status stn_contract_snapshot_response_at(
    const stn_contract_snapshot *snapshot,
    const uint8_t contract_id[STN_ADDRESS_ID_SIZE],size_t response_index,
    stn_contract_snapshot_response *response)
{
    size_t i,seen=0u;
    if(snapshot==NULL || contract_id==NULL || response==NULL)return STN_CONTRACT_ARGUMENT;
    for(i=0u;i<snapshot->response_count;++i){
        const stn_contract_snapshot_response_entry *entry=&snapshot->responses[i];
        if(memcmp(entry->contract_id,contract_id,STN_ADDRESS_ID_SIZE)!=0)continue;
        if(seen++!=response_index)continue;
        memcpy(response->contract_id,entry->contract_id,STN_ADDRESS_ID_SIZE);
        memcpy(response->actor,entry->actor,STN_IDENTITY_PUBLIC_KEY_SIZE);
        response->text=snapshot->response_text_bytes+entry->text_offset;
        response->text_length=entry->text_length;
        return STN_CONTRACT_OK;
    }
    return STN_CONTRACT_NOT_FOUND;
}
