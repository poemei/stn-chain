/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_replay.h"
#include <stdio.h>
#include <string.h>

#ifdef STN_BLOCK_COMPENSATION_REPLAY_TEST_MAIN
static unsigned checks=0u,failures=0u;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int main(void)
{
    stn_block_compensation_replay state;
    stn_block_compensation_evidence evidence;
    uint8_t a[STN_BLOCK_COMPENSATION_ID_SIZE]={0};
    uint8_t b[STN_BLOCK_COMPENSATION_ID_SIZE]={0};
    size_t i;

    stn_block_compensation_replay_initialize(&state);
    CHECK(state.count==0u);
    CHECK(stn_block_compensation_replay_consume(NULL,a)==STN_DATA_ARGUMENT);
    CHECK(stn_block_compensation_replay_consume(&state,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_block_compensation_replay_consume(&state,a)==STN_DATA_CONTENT);

    a[0]=1u;b[0]=2u;
    CHECK(stn_block_compensation_replay_consume(&state,a)==STN_DATA_OK);
    CHECK(state.count==1u);
    CHECK(stn_block_compensation_replay_consume(&state,a)==STN_DATA_DUPLICATE);
    CHECK(state.count==1u);
    CHECK(stn_block_compensation_replay_consume(&state,b)==STN_DATA_OK);
    CHECK(state.count==2u);

    stn_block_compensation_replay_initialize(&state);
    memset(&evidence,0,sizeof(evidence));
    evidence.version=STN_BLOCK_COMPENSATION_VERSION;
    evidence.block_id[0]=1u;
    evidence.miner.type=STN_ADDRESS_IDENTITY;
    evidence.miner.identifier[0]=2u;
    CHECK(stn_block_compensation_replay_consume_evidence(NULL,&evidence)==STN_DATA_ARGUMENT);
    CHECK(stn_block_compensation_replay_consume_evidence(&state,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_block_compensation_replay_consume_evidence(&state,&evidence)==STN_DATA_OK);
    CHECK(state.count==1u);
    CHECK(stn_block_compensation_replay_consume_evidence(&state,&evidence)==STN_DATA_DUPLICATE);
    CHECK(state.count==1u);
    evidence.miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_block_compensation_replay_consume_evidence(&state,&evidence)==STN_DATA_TYPE);
    CHECK(state.count==1u);

    stn_block_compensation_replay_initialize(&state);
    for(i=0u;i<STN_BLOCK_COMPENSATION_REPLAY_CAPACITY;++i){
        uint8_t id[STN_BLOCK_COMPENSATION_ID_SIZE]={0};
        id[0]=(uint8_t)(i+1u);
        id[1]=(uint8_t)((i>>8)+1u);
        CHECK(stn_block_compensation_replay_consume(&state,id)==STN_DATA_OK);
    }
    CHECK(state.count==STN_BLOCK_COMPENSATION_REPLAY_CAPACITY);
    memset(a,0,sizeof(a));a[31]=1u;
    CHECK(stn_block_compensation_replay_consume(&state,a)==STN_DATA_CAPACITY);

    printf("Block compensation replay: %u checks, %u failures.\n",checks,failures);
    return failures==0u ? 0 : 1;
}
#endif
