/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share_replay.h"
#include <string.h>

static int state_valid(const stn_share_replay_state *state)
{
    return state!=NULL &&
        state->consumed_count<=state->consumed_capacity &&
        (state->consumed_capacity==0u || state->consumed!=NULL);
}

stn_share_replay_result stn_share_replay_key(
    const stn_share_evidence *share,
    uint8_t key[STN_SHARE_REPLAY_KEY_SIZE])
{
    size_t i;
    if(share==NULL || key==NULL || share->miner.type!=STN_ADDRESS_IDENTITY){
        return STN_SHARE_REPLAY_ARGUMENT;
    }
    memcpy(key,share->work_id,32);
    memcpy(key+32,share->miner.identifier,32);
    for(i=0;i<8u;++i){
        key[64u+i]=(uint8_t)(share->nonce>>(56u-8u*i));
    }
    return STN_SHARE_REPLAY_FRESH;
}

void stn_share_replay_initialize(
    stn_share_replay_state *state,
    uint8_t *consumed,
    size_t consumed_capacity)
{
    if(state!=NULL){
        state->consumed=consumed;
        state->consumed_count=0;
        state->consumed_capacity=consumed_capacity;
    }
}

stn_share_replay_result stn_share_replay_check(
    const stn_share_replay_state *state,
    const stn_share_evidence *share)
{
    uint8_t key[STN_SHARE_REPLAY_KEY_SIZE];
    size_t i;
    if(!state_valid(state) ||
       stn_share_replay_key(share,key)!=STN_SHARE_REPLAY_FRESH){
        return STN_SHARE_REPLAY_ARGUMENT;
    }
    for(i=0;i<state->consumed_count;++i){
        if(memcmp(state->consumed+i*STN_SHARE_REPLAY_KEY_SIZE,
                  key,STN_SHARE_REPLAY_KEY_SIZE)==0){
            return STN_SHARE_REPLAY_DUPLICATE;
        }
    }
    return STN_SHARE_REPLAY_FRESH;
}

stn_share_replay_result stn_share_replay_consume(
    stn_share_replay_state *state,
    const stn_share_evidence *share)
{
    uint8_t key[STN_SHARE_REPLAY_KEY_SIZE];
    stn_share_replay_result result;
    if(!state_valid(state) ||
       stn_share_replay_key(share,key)!=STN_SHARE_REPLAY_FRESH){
        return STN_SHARE_REPLAY_ARGUMENT;
    }
    result=stn_share_replay_check(state,share);
    if(result!=STN_SHARE_REPLAY_FRESH){
        return result;
    }
    if(state->consumed_count==state->consumed_capacity){
        return STN_SHARE_REPLAY_CAPACITY;
    }
    memcpy(state->consumed+state->consumed_count*STN_SHARE_REPLAY_KEY_SIZE,
           key,STN_SHARE_REPLAY_KEY_SIZE);
    ++state->consumed_count;
    return STN_SHARE_REPLAY_FRESH;
}
