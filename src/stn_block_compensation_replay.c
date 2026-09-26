/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_replay.h"
#include <string.h>

static int zero_id(const uint8_t id[STN_BLOCK_COMPENSATION_ID_SIZE])
{
    size_t i;
    uint8_t any=0u;
    for(i=0u;i<STN_BLOCK_COMPENSATION_ID_SIZE;++i)any|=id[i];
    return any==0u;
}

void stn_block_compensation_replay_initialize(stn_block_compensation_replay *state)
{
    if(state==NULL)return;
    memset(state,0,sizeof(*state));
}

stn_data_status stn_block_compensation_replay_consume(
    stn_block_compensation_replay *state,
    const uint8_t evidence_id[STN_BLOCK_COMPENSATION_ID_SIZE])
{
    size_t i;
    if(state==NULL || evidence_id==NULL)return STN_DATA_ARGUMENT;
    if(zero_id(evidence_id))return STN_DATA_CONTENT;
    for(i=0u;i<state->count;++i){
        if(memcmp(state->ids[i],evidence_id,STN_BLOCK_COMPENSATION_ID_SIZE)==0)
            return STN_DATA_DUPLICATE;
    }
    if(state->count>=STN_BLOCK_COMPENSATION_REPLAY_CAPACITY)return STN_DATA_CAPACITY;
    memcpy(state->ids[state->count],evidence_id,STN_BLOCK_COMPENSATION_ID_SIZE);
    ++state->count;
    return STN_DATA_OK;
}
