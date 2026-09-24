/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_economic_persistence.h"
#include <string.h>
#include <stdint.h>

static void write64(uint8_t *p,uint64_t v){size_t i=8u;while(i!=0u){p[--i]=(uint8_t)(v&255u);v>>=8;}}
static uint64_t read64(const uint8_t *p){uint64_t v=0u;size_t i;for(i=0u;i<8u;++i)v=(v<<8)|p[i];return v;}

static stn_data_status valid(const stn_economic_state *state)
{
    size_t i;uint64_t sum=0u;
    if(state==NULL)return STN_DATA_ARGUMENT;
    if(state->balance_count>state->balance_capacity ||
       (state->balance_count!=0u && state->balances==NULL))return STN_DATA_CONTENT;
    for(i=0u;i<state->balance_count;++i){
        if(i!=0u && memcmp(state->balances[i-1u].wallet_id,state->balances[i].wallet_id,STN_ADDRESS_ID_SIZE)>=0)return STN_DATA_CONTENT;
        if(UINT64_MAX-sum<state->balances[i].units)return STN_DATA_OVERFLOW;
        sum+=state->balances[i].units;
    }
    return sum==state->total_supply?STN_DATA_OK:STN_DATA_CONTENT;
}

stn_data_status stn_economic_persistence_size(const stn_economic_state *state,size_t *size)
{
    stn_data_status status;if(size==NULL)return STN_DATA_ARGUMENT;status=valid(state);if(status!=STN_DATA_OK)return status;
    if(state->balance_count>(SIZE_MAX-STN_ECONOMIC_PERSISTENCE_HEADER_SIZE)/STN_ECONOMIC_PERSISTENCE_BALANCE_SIZE)return STN_DATA_OVERFLOW;
    *size=STN_ECONOMIC_PERSISTENCE_HEADER_SIZE+state->balance_count*STN_ECONOMIC_PERSISTENCE_BALANCE_SIZE;return STN_DATA_OK;
}

stn_data_status stn_economic_persistence_encode(const stn_economic_state *state,uint8_t *bytes,size_t capacity,size_t *written)
{
    size_t need,i,at=STN_ECONOMIC_PERSISTENCE_HEADER_SIZE;stn_data_status status;
    if(written==NULL)return STN_DATA_ARGUMENT;*written=0u;if(bytes==NULL)return STN_DATA_ARGUMENT;
    status=stn_economic_persistence_size(state,&need);if(status!=STN_DATA_OK)return status;if(capacity<need)return STN_DATA_CAPACITY;
    bytes[0]=STN_ECONOMIC_PERSISTENCE_VERSION;write64(bytes+1u,state->total_supply);write64(bytes+9u,(uint64_t)state->balance_count);
    for(i=0u;i<state->balance_count;++i){memcpy(bytes+at,state->balances[i].wallet_id,32u);write64(bytes+at+32u,state->balances[i].units);at+=40u;}
    *written=need;return STN_DATA_OK;
}

stn_data_status stn_economic_persistence_decode(const uint8_t *bytes,size_t length,stn_economic_state *state,stn_economic_balance *storage,size_t capacity)
{
    stn_economic_state decoded={0};size_t count,i,need,at=STN_ECONOMIC_PERSISTENCE_HEADER_SIZE;uint64_t encoded_count;
    if(bytes==NULL || state==NULL || (capacity!=0u && storage==NULL))return STN_DATA_ARGUMENT;
    if(length<STN_ECONOMIC_PERSISTENCE_HEADER_SIZE)return STN_DATA_LENGTH;if(bytes[0]!=STN_ECONOMIC_PERSISTENCE_VERSION)return STN_DATA_VERSION;
    encoded_count=read64(bytes+9u);if(encoded_count>SIZE_MAX)return STN_DATA_OVERFLOW;count=(size_t)encoded_count;
    if(count>(SIZE_MAX-STN_ECONOMIC_PERSISTENCE_HEADER_SIZE)/STN_ECONOMIC_PERSISTENCE_BALANCE_SIZE)return STN_DATA_OVERFLOW;
    need=STN_ECONOMIC_PERSISTENCE_HEADER_SIZE+count*STN_ECONOMIC_PERSISTENCE_BALANCE_SIZE;if(length!=need)return STN_DATA_LENGTH;if(count>capacity)return STN_DATA_CAPACITY;
    decoded.balances=storage;decoded.balance_capacity=capacity;decoded.balance_count=count;decoded.total_supply=read64(bytes+1u);
    for(i=0u;i<count;++i){memcpy(storage[i].wallet_id,bytes+at,32u);storage[i].units=read64(bytes+at+32u);at+=40u;}
    if(valid(&decoded)!=STN_DATA_OK)return STN_DATA_CONTENT;*state=decoded;return STN_DATA_OK;
}
