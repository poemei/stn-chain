/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_economic_state.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"economic state line %d: %s\n",__LINE__,#e);} } while(0)

static void issuance(stn_issuance_record *r,uint8_t reason,uint8_t wallet_byte)
{
    memset(r,0,sizeof(*r));
    r->reason=reason;
    r->units=reason==STN_ISSUANCE_REASON_SHARE ? STN_ISSUANCE_SHARE_UNITS : STN_ISSUANCE_BLOCK_UNITS;
    r->destination.mining_identity.type=STN_ADDRESS_IDENTITY;
    r->destination.wallet.type=STN_ADDRESS_WALLET;
    memset(r->destination.mining_identity.identifier,0x11,32u);
    memset(r->destination.wallet.identifier,wallet_byte,32u);
}

static void vectors(void)
{
    stn_economic_balance storage[3],snapshot[3];
    stn_economic_state state={0};
    stn_issuance_record r;
    stn_address wallet={0};
    uint64_t units=999u,total;
    size_t count;

    memset(storage,0,sizeof(storage));
    CHECK(stn_economic_state_initialize(&state,storage,3u)==STN_DATA_OK);
    CHECK(state.balance_count==0u && state.total_supply==0u);

    issuance(&r,STN_ISSUANCE_REASON_SHARE,0x22);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OK);
    CHECK(state.balance_count==1u && state.total_supply==1u);
    issuance(&r,STN_ISSUANCE_REASON_BLOCK,0x22);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OK);
    CHECK(state.balance_count==1u && state.total_supply==101u);
    CHECK(state.balances[0].units==101u);

    issuance(&r,STN_ISSUANCE_REASON_SHARE,0x11);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OK);
    issuance(&r,STN_ISSUANCE_REASON_BLOCK,0x33);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OK);
    CHECK(state.balance_count==3u && state.total_supply==202u);
    CHECK(state.balances[0].wallet_id[0]==0x11u);
    CHECK(state.balances[1].wallet_id[0]==0x22u);
    CHECK(state.balances[2].wallet_id[0]==0x33u);

    wallet.type=STN_ADDRESS_WALLET;memset(wallet.identifier,0x22,32u);
    CHECK(stn_economic_state_balance(&state,&wallet,&units)==STN_DATA_OK && units==101u);
    memset(wallet.identifier,0x44,32u);units=999u;
    CHECK(stn_economic_state_balance(&state,&wallet,&units)==STN_DATA_OK && units==0u);

    memcpy(snapshot,storage,sizeof(storage));total=state.total_supply;count=state.balance_count;
    issuance(&r,STN_ISSUANCE_REASON_SHARE,0x44);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_CAPACITY);
    CHECK(state.total_supply==total && state.balance_count==count);
    CHECK(memcmp(storage,snapshot,sizeof(storage))==0);

    state.total_supply=UINT64_MAX;
    issuance(&r,STN_ISSUANCE_REASON_SHARE,0x22);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OVERFLOW);
    CHECK(state.total_supply==UINT64_MAX && state.balances[1].units==101u);
    state.total_supply=total;
    state.balances[1].units=UINT64_MAX;
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OVERFLOW);
    CHECK(state.total_supply==total && state.balances[1].units==UINT64_MAX);
    state.balances[1].units=101u;

    r.units=2u;
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_CONTENT);
    r.units=1u;r.destination.wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_TYPE);
    CHECK(stn_economic_state_apply(NULL,&r)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_state_apply(&state,NULL)==STN_DATA_ARGUMENT);

    wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_economic_state_balance(&state,&wallet,&units)==STN_DATA_TYPE);
    CHECK(stn_economic_state_balance(NULL,&wallet,&units)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_state_balance(&state,NULL,&units)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_state_balance(&state,&wallet,NULL)==STN_DATA_ARGUMENT);

    CHECK(stn_economic_state_initialize(NULL,storage,3u)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_state_initialize(&state,NULL,1u)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_state_initialize(&state,NULL,0u)==STN_DATA_OK);
    issuance(&r,STN_ISSUANCE_REASON_SHARE,0x55);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_CAPACITY);
}


static void transfer_vectors(void)
{
    stn_economic_balance storage[4],snapshot[4];
    stn_economic_state state={0};
    stn_issuance_record r;
    stn_transfer transfer={0};
    uint64_t total;
    size_t count;

    memset(storage,0,sizeof(storage));
    CHECK(stn_economic_state_initialize(&state,storage,4u)==STN_DATA_OK);
    issuance(&r,STN_ISSUANCE_REASON_BLOCK,0x22);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OK);
    issuance(&r,STN_ISSUANCE_REASON_BLOCK,0x33);
    CHECK(stn_economic_state_apply(&state,&r)==STN_DATA_OK);
    total=state.total_supply;

    transfer.source.type=STN_ADDRESS_WALLET;
    transfer.destination.type=STN_ADDRESS_WALLET;
    memset(transfer.source.identifier,0x22,STN_ADDRESS_ID_SIZE);
    memset(transfer.destination.identifier,0x33,STN_ADDRESS_ID_SIZE);
    transfer.units=25u;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_OK);
    CHECK(state.balances[0].units==75u && state.balances[1].units==125u);
    CHECK(state.total_supply==total);

    memset(transfer.destination.identifier,0x11,STN_ADDRESS_ID_SIZE);
    transfer.units=10u;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_OK);
    CHECK(state.balance_count==3u);
    CHECK(state.balances[0].wallet_id[0]==0x11u && state.balances[0].units==10u);
    CHECK(state.balances[1].wallet_id[0]==0x22u && state.balances[1].units==65u);
    CHECK(state.balances[2].wallet_id[0]==0x33u && state.balances[2].units==125u);
    CHECK(state.total_supply==total);

    memcpy(snapshot,storage,sizeof(storage));count=state.balance_count;
    memset(transfer.source.identifier,0x44,STN_ADDRESS_ID_SIZE);
    transfer.units=1u;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_CONTENT);
    CHECK(state.balance_count==count && state.total_supply==total);
    CHECK(memcmp(storage,snapshot,sizeof(storage))==0);

    memset(transfer.source.identifier,0x22,STN_ADDRESS_ID_SIZE);
    transfer.units=66u;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_CONTENT);
    CHECK(memcmp(storage,snapshot,sizeof(storage))==0);

    transfer.units=0u;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_CONTENT);
    transfer.units=1u;transfer.destination=transfer.source;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_CONTENT);
    transfer.destination.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_TYPE);
    CHECK(stn_economic_state_apply_transfer(NULL,&transfer)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_state_apply_transfer(&state,NULL)==STN_DATA_ARGUMENT);

    transfer.destination.type=STN_ADDRESS_WALLET;
    memset(transfer.destination.identifier,0x55,STN_ADDRESS_ID_SIZE);
    state.balance_capacity=state.balance_count;
    memcpy(snapshot,storage,sizeof(storage));
    CHECK(stn_economic_state_apply_transfer(&state,&transfer)==STN_DATA_CAPACITY);
    CHECK(memcmp(storage,snapshot,sizeof(storage))==0 && state.total_supply==total);
}

int test_economic_state(void);
int test_economic_state(void)
{
    vectors();
    transfer_vectors();
    printf("Economic accepted state: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}

#ifdef STN_ECONOMIC_STATE_TEST_MAIN
int main(void){return test_economic_state();}
#endif
