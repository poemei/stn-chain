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

stn_data_status stn_block_compensation_replay_copy(
    const stn_block_compensation_replay *source,
    stn_block_compensation_replay *out)
{
    stn_block_compensation_replay copy;
    size_t i;
    if(source==NULL || out==NULL)return STN_DATA_ARGUMENT;
    if(source->count>STN_BLOCK_COMPENSATION_REPLAY_CAPACITY)return STN_DATA_CONTENT;
    for(i=0u;i<source->count;++i){
        if(zero_id(source->ids[i]))return STN_DATA_CONTENT;
    }
    copy=*source;
    *out=copy;
    return STN_DATA_OK;
}

stn_data_status stn_block_compensation_replay_consume(
    stn_block_compensation_replay *state,
    const uint8_t evidence_id[STN_BLOCK_COMPENSATION_ID_SIZE])
{
    size_t i;
    if(state==NULL || evidence_id==NULL)return STN_DATA_ARGUMENT;
    if(state->count>STN_BLOCK_COMPENSATION_REPLAY_CAPACITY)return STN_DATA_CONTENT;
    if(zero_id(evidence_id))return STN_DATA_CONTENT;
    for(i=0u;i<state->count;++i){
        if(zero_id(state->ids[i]))return STN_DATA_CONTENT;
        if(memcmp(state->ids[i],evidence_id,STN_BLOCK_COMPENSATION_ID_SIZE)==0)
            return STN_DATA_DUPLICATE;
    }
    if(state->count>=STN_BLOCK_COMPENSATION_REPLAY_CAPACITY)return STN_DATA_CAPACITY;
    memcpy(state->ids[state->count],evidence_id,STN_BLOCK_COMPENSATION_ID_SIZE);
    ++state->count;
    return STN_DATA_OK;
}

stn_data_status stn_block_compensation_replay_consume_evidence(
    stn_block_compensation_replay *state,
    const stn_block_compensation_evidence *evidence)
{
    uint8_t evidence_id[STN_BLOCK_COMPENSATION_ID_SIZE];
    stn_data_status status;
    if(state==NULL || evidence==NULL)return STN_DATA_ARGUMENT;
    status=stn_block_compensation_id(evidence,evidence_id);
    if(status!=STN_DATA_OK)return status;
    return stn_block_compensation_replay_consume(state,evidence_id);
}
