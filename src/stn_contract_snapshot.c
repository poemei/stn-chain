/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_snapshot.h"

#include <stdlib.h>
#include <string.h>

struct stn_contract_snapshot {
    size_t references;
    stn_contract_state_store state;
    stn_contract_state_entry entries[STN_CONTRACT_STATE_MAX_CONTRACTS];
    uint8_t votes[STN_CONTRACT_STATE_MAX_VOTES * STN_CONTRACT_VOTE_KEY_SIZE];
};

static void bind(stn_contract_snapshot *snapshot)
{
    size_t i;
    snapshot->state.entries=snapshot->entries;
    snapshot->state.entry_capacity=STN_CONTRACT_STATE_MAX_CONTRACTS;
    snapshot->state.votes=snapshot->votes;
    snapshot->state.vote_capacity=STN_CONTRACT_STATE_MAX_VOTES;
    for(i=0;i<snapshot->state.entry_count;++i){
        /*
         * Contract DRAFT bytes are immutable accepted-history evidence and are
         * borrowed by the snapshot. Mutable current state retains the same
         * participant/terms representation established by registration.
         */
        if(snapshot->entries[i].current.participants==NULL &&
           snapshot->entries[i].canonical_draft!=NULL){
            stn_contract decoded;
            if(stn_contract_decode(snapshot->entries[i].canonical_draft,
                snapshot->entries[i].canonical_draft_length,&decoded)==STN_CONTRACT_OK){
                snapshot->entries[i].current.participant_bytes=decoded.participant_bytes;
                snapshot->entries[i].current.terms=decoded.terms;
            }
        }
    }
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
    bind(snapshot);
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
    if(--snapshot->references==0u)free(snapshot);
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
