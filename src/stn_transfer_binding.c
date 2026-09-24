/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_binding.h"

stn_data_status stn_transfer_binding_apply(
    stn_economic_state *economic,
    stn_transfer_replay_state *replay,
    const stn_transfer *transfer)
{
    stn_transfer_replay_result replay_status;
    stn_data_status economic_status;

    if(economic==NULL || replay==NULL || transfer==NULL)return STN_DATA_ARGUMENT;

    replay_status=stn_transfer_replay_check(replay,transfer);
    if(replay_status==STN_TRANSFER_REPLAY_DUPLICATE)return STN_DATA_CONTENT;
    if(replay_status==STN_TRANSFER_REPLAY_ARGUMENT)return STN_DATA_ARGUMENT;
    if(replay_status!=STN_TRANSFER_REPLAY_FRESH)return STN_DATA_CONTENT;
    if(replay->consumed_count==replay->consumed_capacity)return STN_DATA_CAPACITY;

    economic_status=stn_economic_state_apply_transfer(economic,transfer);
    if(economic_status!=STN_DATA_OK)return economic_status;

    replay_status=stn_transfer_replay_consume(replay,transfer);
    if(replay_status!=STN_TRANSFER_REPLAY_FRESH)return STN_DATA_ARGUMENT;
    return STN_DATA_OK;
}
