/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share_replay.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int test_share_replay(void)
{
    uint8_t memory[2u*STN_SHARE_REPLAY_KEY_SIZE];
    uint8_t key[STN_SHARE_REPLAY_KEY_SIZE];
    stn_share_replay_state state={0};
    stn_share_evidence a={0},b={0},c={0};
    size_t i;

    a.miner.type=STN_ADDRESS_IDENTITY;
    for(i=0;i<32u;++i){
        a.work_id[i]=(uint8_t)(i+1u);
        a.miner.identifier[i]=(uint8_t)(0xa0u+i);
    }
    a.nonce=0x0102030405060708ULL;
    b=a;b.nonce++;
    c=a;c.work_id[0]^=1u;

    CHECK(stn_share_replay_key(&a,key)==STN_SHARE_REPLAY_FRESH);
    CHECK(memcmp(key,a.work_id,32)==0);
    CHECK(memcmp(key+32,a.miner.identifier,32)==0);
    CHECK(key[64]==1 && key[65]==2 && key[66]==3 && key[67]==4);
    CHECK(key[68]==5 && key[69]==6 && key[70]==7 && key[71]==8);

    stn_share_replay_initialize(&state,memory,2);
    CHECK(state.consumed_count==0 && state.consumed_capacity==2);
    CHECK(stn_share_replay_check(&state,&a)==STN_SHARE_REPLAY_FRESH);
    CHECK(stn_share_replay_consume(&state,&a)==STN_SHARE_REPLAY_FRESH);
    CHECK(state.consumed_count==1);
    CHECK(stn_share_replay_check(&state,&a)==STN_SHARE_REPLAY_DUPLICATE);
    CHECK(stn_share_replay_consume(&state,&a)==STN_SHARE_REPLAY_DUPLICATE);
    CHECK(state.consumed_count==1);
    CHECK(stn_share_replay_consume(&state,&b)==STN_SHARE_REPLAY_FRESH);
    CHECK(state.consumed_count==2);
    CHECK(stn_share_replay_check(&state,&c)==STN_SHARE_REPLAY_FRESH);
    CHECK(stn_share_replay_consume(&state,&c)==STN_SHARE_REPLAY_CAPACITY);
    CHECK(state.consumed_count==2);

    a.miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_share_replay_key(&a,key)==STN_SHARE_REPLAY_ARGUMENT);
    CHECK(stn_share_replay_check(&state,&a)==STN_SHARE_REPLAY_ARGUMENT);
    CHECK(stn_share_replay_consume(&state,&a)==STN_SHARE_REPLAY_ARGUMENT);
    CHECK(stn_share_replay_key(NULL,key)==STN_SHARE_REPLAY_ARGUMENT);
    CHECK(stn_share_replay_key(&b,NULL)==STN_SHARE_REPLAY_ARGUMENT);
    CHECK(stn_share_replay_check(NULL,&b)==STN_SHARE_REPLAY_ARGUMENT);

    stn_share_replay_initialize(&state,NULL,1);
    CHECK(stn_share_replay_check(&state,&b)==STN_SHARE_REPLAY_ARGUMENT);
    stn_share_replay_initialize(&state,NULL,0);
    CHECK(stn_share_replay_check(&state,&b)==STN_SHARE_REPLAY_FRESH);
    CHECK(stn_share_replay_consume(&state,&b)==STN_SHARE_REPLAY_CAPACITY);

    printf("Share replay state: %u checks, %u failures.\n",checks,failures);
    return failures?1:0;
}

#ifdef STN_SHARE_REPLAY_TEST_MAIN
int main(void){return test_share_replay();}
#endif
