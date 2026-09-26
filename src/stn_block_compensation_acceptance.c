/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_acceptance.h"

stn_data_status stn_block_compensation_accept(
    stn_block_compensation_replay *replay,
    const stn_block_compensation_evidence *evidence,
    const stn_issuance_record *issuance,
    const stn_compensation_state *compensation,
    stn_economic_state *economy)
{
    stn_block_compensation_replay next_replay;
    stn_economic_state next_economy;
    stn_economic_balance *balances;
    stn_data_status status;
    size_t i;

    if(replay==NULL || evidence==NULL || issuance==NULL ||
       compensation==NULL || economy==NULL)return STN_DATA_ARGUMENT;
    if(economy->balance_count>economy->balance_capacity ||
       (economy->balance_capacity!=0u && economy->balances==NULL))return STN_DATA_CONTENT;

    status=stn_issuance_bind_block(issuance,evidence,compensation);
    if(status!=STN_DATA_OK)return status;
    status=stn_block_compensation_replay_copy(replay,&next_replay);
    if(status!=STN_DATA_OK)return status;
    status=stn_block_compensation_replay_consume_evidence(&next_replay,evidence);
    if(status!=STN_DATA_OK)return status;

    balances=economy->balances;
    next_economy=*economy;
    if(economy->balance_capacity!=0u){
        stn_economic_balance scratch[economy->balance_capacity];
        for(i=0u;i<economy->balance_count;++i)scratch[i]=economy->balances[i];
        next_economy.balances=scratch;
        status=stn_economic_state_apply(&next_economy,issuance);
        if(status!=STN_DATA_OK)return status;
        for(i=0u;i<next_economy.balance_count;++i)balances[i]=scratch[i];
    }else{
        status=stn_economic_state_apply(&next_economy,issuance);
        if(status!=STN_DATA_OK)return status;
    }
    economy->balance_count=next_economy.balance_count;
    economy->total_supply=next_economy.total_supply;
    *replay=next_replay;
    return STN_DATA_OK;
}
