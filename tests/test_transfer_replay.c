/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_replay.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

static void make_transfer(stn_transfer *transfer,uint8_t source,uint8_t destination,uint64_t units)
{
    memset(transfer,0,sizeof(*transfer));
    transfer->source.type=STN_ADDRESS_WALLET;
    transfer->destination.type=STN_ADDRESS_WALLET;
    memset(transfer->source.identifier,source,STN_ADDRESS_ID_SIZE);
    memset(transfer->destination.identifier,destination,STN_ADDRESS_ID_SIZE);
    transfer->units=units;
}

int test_transfer_replay(void)
{
    uint8_t memory[2u*STN_TRANSFER_REPLAY_KEY_SIZE];
    uint8_t key[STN_TRANSFER_REPLAY_KEY_SIZE],canonical[STN_TRANSFER_CANONICAL_SIZE];
    stn_transfer_replay_state state={0};
    stn_transfer a,b,c;

    make_transfer(&a,0x11,0x22,25u);
    b=a;b.units=26u;
    c=a;c.destination.identifier[0]=0x33u;

    CHECK(stn_transfer_replay_key(&a,key)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(stn_transfer_encode(&a,canonical)==STN_DATA_OK);
    CHECK(memcmp(key,canonical,sizeof(key))==0);

    stn_transfer_replay_initialize(&state,memory,2u);
    CHECK(state.consumed_count==0u && state.consumed_capacity==2u);
    CHECK(stn_transfer_replay_check(&state,&a)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(stn_transfer_replay_consume(&state,&a)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(state.consumed_count==1u);
    CHECK(stn_transfer_replay_check(&state,&a)==STN_TRANSFER_REPLAY_DUPLICATE);
    CHECK(stn_transfer_replay_consume(&state,&a)==STN_TRANSFER_REPLAY_DUPLICATE);
    CHECK(state.consumed_count==1u);
    CHECK(stn_transfer_replay_check(&state,&b)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(stn_transfer_replay_consume(&state,&b)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(state.consumed_count==2u);
    CHECK(stn_transfer_replay_check(&state,&c)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(stn_transfer_replay_consume(&state,&c)==STN_TRANSFER_REPLAY_CAPACITY);
    CHECK(state.consumed_count==2u);

    a.units=0u;
    CHECK(stn_transfer_replay_key(&a,key)==STN_TRANSFER_REPLAY_ARGUMENT);
    CHECK(stn_transfer_replay_check(&state,&a)==STN_TRANSFER_REPLAY_ARGUMENT);
    CHECK(stn_transfer_replay_consume(&state,&a)==STN_TRANSFER_REPLAY_ARGUMENT);
    CHECK(stn_transfer_replay_key(NULL,key)==STN_TRANSFER_REPLAY_ARGUMENT);
    CHECK(stn_transfer_replay_key(&b,NULL)==STN_TRANSFER_REPLAY_ARGUMENT);
    CHECK(stn_transfer_replay_check(NULL,&b)==STN_TRANSFER_REPLAY_ARGUMENT);

    stn_transfer_replay_initialize(&state,NULL,1u);
    CHECK(stn_transfer_replay_check(&state,&b)==STN_TRANSFER_REPLAY_ARGUMENT);
    stn_transfer_replay_initialize(&state,NULL,0u);
    CHECK(stn_transfer_replay_check(&state,&b)==STN_TRANSFER_REPLAY_FRESH);
    CHECK(stn_transfer_replay_consume(&state,&b)==STN_TRANSFER_REPLAY_CAPACITY);

    printf("Transfer replay state: %u checks, %u failures.\n",checks,failures);
    return failures?1:0;
}

#ifdef STN_TRANSFER_REPLAY_TEST_MAIN
int main(void){return test_transfer_replay();}
#endif
