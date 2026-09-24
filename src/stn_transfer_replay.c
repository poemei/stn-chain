/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_replay.h"

#include <string.h>

static int state_valid(const stn_transfer_replay_state *state)
{
    return state!=NULL &&
        state->consumed_count<=state->consumed_capacity &&
        (state->consumed_capacity==0u || state->consumed!=NULL);
}

stn_transfer_replay_result stn_transfer_replay_key(
    const stn_transfer *transfer,
    uint8_t key[STN_TRANSFER_REPLAY_KEY_SIZE])
{
    if(transfer==NULL || key==NULL)return STN_TRANSFER_REPLAY_ARGUMENT;
    if(stn_transfer_encode(transfer,key)!=STN_DATA_OK)
        return STN_TRANSFER_REPLAY_ARGUMENT;
    return STN_TRANSFER_REPLAY_FRESH;
}

void stn_transfer_replay_initialize(
    stn_transfer_replay_state *state,
    uint8_t *consumed,
    size_t consumed_capacity)
{
    if(state!=NULL){
        state->consumed=consumed;
        state->consumed_count=0u;
        state->consumed_capacity=consumed_capacity;
    }
}

stn_transfer_replay_result stn_transfer_replay_check(
    const stn_transfer_replay_state *state,
    const stn_transfer *transfer)
{
    uint8_t key[STN_TRANSFER_REPLAY_KEY_SIZE];
    size_t i;
    if(!state_valid(state) ||
       stn_transfer_replay_key(transfer,key)!=STN_TRANSFER_REPLAY_FRESH)
        return STN_TRANSFER_REPLAY_ARGUMENT;
    for(i=0u;i<state->consumed_count;++i){
        if(memcmp(state->consumed+i*STN_TRANSFER_REPLAY_KEY_SIZE,
                  key,STN_TRANSFER_REPLAY_KEY_SIZE)==0)
            return STN_TRANSFER_REPLAY_DUPLICATE;
    }
    return STN_TRANSFER_REPLAY_FRESH;
}

stn_transfer_replay_result stn_transfer_replay_consume(
    stn_transfer_replay_state *state,
    const stn_transfer *transfer)
{
    uint8_t key[STN_TRANSFER_REPLAY_KEY_SIZE];
    stn_transfer_replay_result result;
    if(!state_valid(state) ||
       stn_transfer_replay_key(transfer,key)!=STN_TRANSFER_REPLAY_FRESH)
        return STN_TRANSFER_REPLAY_ARGUMENT;
    result=stn_transfer_replay_check(state,transfer);
    if(result!=STN_TRANSFER_REPLAY_FRESH)return result;
    if(state->consumed_count==state->consumed_capacity)
        return STN_TRANSFER_REPLAY_CAPACITY;
    memcpy(state->consumed+state->consumed_count*STN_TRANSFER_REPLAY_KEY_SIZE,
           key,STN_TRANSFER_REPLAY_KEY_SIZE);
    ++state->consumed_count;
    return STN_TRANSFER_REPLAY_FRESH;
}
