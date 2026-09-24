/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_economic_state.h"
#include <string.h>
#include <stdint.h>

stn_data_status stn_economic_state_initialize(
    stn_economic_state *state,
    stn_economic_balance *storage,
    size_t capacity)
{
    stn_economic_state result={0};
    if(state==NULL || (capacity!=0u && storage==NULL))return STN_DATA_ARGUMENT;
    result.balances=storage;
    result.balance_capacity=capacity;
    *state=result;
    return STN_DATA_OK;
}

static stn_data_status record_valid(const stn_issuance_record *r)
{
    if(r==NULL)return STN_DATA_ARGUMENT;
    if(r->destination.mining_identity.type!=STN_ADDRESS_IDENTITY ||
       r->destination.wallet.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if((r->reason==STN_ISSUANCE_REASON_SHARE && r->units==STN_ISSUANCE_SHARE_UNITS) ||
       (r->reason==STN_ISSUANCE_REASON_BLOCK && r->units==STN_ISSUANCE_BLOCK_UNITS))
        return STN_DATA_OK;
    return STN_DATA_CONTENT;
}

stn_data_status stn_economic_state_apply(
    stn_economic_state *state,
    const stn_issuance_record *record)
{
    size_t at=0u;
    int cmp=0;
    stn_data_status status;

    if(state==NULL)return STN_DATA_ARGUMENT;
    status=record_valid(record);
    if(status!=STN_DATA_OK)return status;
    if(state->balance_count>state->balance_capacity ||
       (state->balance_capacity!=0u && state->balances==NULL))return STN_DATA_ARGUMENT;
    if(UINT64_MAX-state->total_supply<record->units)return STN_DATA_OVERFLOW;

    while(at<state->balance_count){
        cmp=memcmp(state->balances[at].wallet_id,
                   record->destination.wallet.identifier,STN_ADDRESS_ID_SIZE);
        if(cmp>=0)break;
        ++at;
    }
    if(at<state->balance_count && cmp==0){
        if(UINT64_MAX-state->balances[at].units<record->units)return STN_DATA_OVERFLOW;
        state->balances[at].units+=record->units;
        state->total_supply+=record->units;
        return STN_DATA_OK;
    }
    if(state->balance_count==state->balance_capacity)return STN_DATA_CAPACITY;

    if(at<state->balance_count){
        memmove(state->balances+at+1u,state->balances+at,
            (state->balance_count-at)*sizeof(*state->balances));
    }
    memcpy(state->balances[at].wallet_id,
           record->destination.wallet.identifier,STN_ADDRESS_ID_SIZE);
    state->balances[at].units=record->units;
    ++state->balance_count;
    state->total_supply+=record->units;
    return STN_DATA_OK;
}


stn_data_status stn_economic_state_apply_transfer(
    stn_economic_state *state,
    const stn_transfer *transfer)
{
    size_t source_at=0u,destination_at=0u,i;
    int source_found=0,destination_found=0;
    uint64_t destination_units=0u;

    if(state==NULL || transfer==NULL)return STN_DATA_ARGUMENT;
    if(transfer->source.type!=STN_ADDRESS_WALLET ||
       transfer->destination.type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if(transfer->units==0u ||
       memcmp(transfer->source.identifier,transfer->destination.identifier,
              STN_ADDRESS_ID_SIZE)==0)return STN_DATA_CONTENT;
    if(state->balance_count>state->balance_capacity ||
       (state->balance_capacity!=0u && state->balances==NULL))return STN_DATA_ARGUMENT;

    for(i=0u;i<state->balance_count;++i){
        int source_cmp=memcmp(state->balances[i].wallet_id,
            transfer->source.identifier,STN_ADDRESS_ID_SIZE);
        int destination_cmp=memcmp(state->balances[i].wallet_id,
            transfer->destination.identifier,STN_ADDRESS_ID_SIZE);
        if(source_cmp==0){source_found=1;source_at=i;}
        if(destination_cmp==0){destination_found=1;destination_at=i;}
    }

    if(!source_found || state->balances[source_at].units<transfer->units)
        return STN_DATA_CONTENT;
    if(destination_found){
        destination_units=state->balances[destination_at].units;
        if(UINT64_MAX-destination_units<transfer->units)return STN_DATA_OVERFLOW;
        state->balances[source_at].units-=transfer->units;
        state->balances[destination_at].units+=transfer->units;
        return STN_DATA_OK;
    }

    if(state->balance_count==state->balance_capacity)return STN_DATA_CAPACITY;

    destination_at=0u;
    while(destination_at<state->balance_count &&
          memcmp(state->balances[destination_at].wallet_id,
                 transfer->destination.identifier,STN_ADDRESS_ID_SIZE)<0)
        ++destination_at;

    state->balances[source_at].units-=transfer->units;
    if(destination_at<state->balance_count){
        memmove(state->balances+destination_at+1u,state->balances+destination_at,
            (state->balance_count-destination_at)*sizeof(*state->balances));
    }
    memcpy(state->balances[destination_at].wallet_id,
           transfer->destination.identifier,STN_ADDRESS_ID_SIZE);
    state->balances[destination_at].units=transfer->units;
    ++state->balance_count;
    return STN_DATA_OK;
}

stn_data_status stn_economic_state_balance(
    const stn_economic_state *state,
    const stn_address *wallet,
    uint64_t *units)
{
    size_t i;
    if(state==NULL || wallet==NULL || units==NULL)return STN_DATA_ARGUMENT;
    if(wallet->type!=STN_ADDRESS_WALLET)return STN_DATA_TYPE;
    if(state->balance_count>state->balance_capacity ||
       (state->balance_capacity!=0u && state->balances==NULL))return STN_DATA_ARGUMENT;
    for(i=0u;i<state->balance_count;++i){
        int cmp=memcmp(state->balances[i].wallet_id,wallet->identifier,STN_ADDRESS_ID_SIZE);
        if(cmp==0){*units=state->balances[i].units;return STN_DATA_OK;}
        if(cmp>0)break;
    }
    *units=0u;
    return STN_DATA_OK;
}
