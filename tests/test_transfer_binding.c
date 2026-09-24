/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_binding.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

static void wallet(stn_address *address,uint8_t byte)
{
    memset(address,0,sizeof(*address));
    address->type=STN_ADDRESS_WALLET;
    memset(address->identifier,byte,STN_ADDRESS_ID_SIZE);
}

static void issuance(stn_issuance_record *record,uint8_t byte)
{
    memset(record,0,sizeof(*record));
    record->reason=STN_ISSUANCE_REASON_BLOCK;
    record->units=STN_ISSUANCE_BLOCK_UNITS;
    record->destination.mining_identity.type=STN_ADDRESS_IDENTITY;
    record->destination.wallet.type=STN_ADDRESS_WALLET;
    memset(record->destination.mining_identity.identifier,0x44,STN_ADDRESS_ID_SIZE);
    memset(record->destination.wallet.identifier,byte,STN_ADDRESS_ID_SIZE);
}

int test_transfer_binding(void)
{
    stn_economic_balance balances[3],snapshot[3];
    uint8_t consumed[2u*STN_TRANSFER_REPLAY_KEY_SIZE];
    stn_economic_state economic={0};
    stn_transfer_replay_state replay={0};
    stn_issuance_record issue;
    stn_transfer transfer={0},changed;
    uint64_t total;
    size_t count;

    memset(balances,0,sizeof(balances));
    memset(consumed,0,sizeof(consumed));
    CHECK(stn_economic_state_initialize(&economic,balances,3u)==STN_DATA_OK);
    issuance(&issue,0x11);
    CHECK(stn_economic_state_apply(&economic,&issue)==STN_DATA_OK);
    total=economic.total_supply;
    stn_transfer_replay_initialize(&replay,consumed,2u);

    wallet(&transfer.source,0x11);
    wallet(&transfer.destination,0x22);
    transfer.units=25u;
    CHECK(stn_transfer_binding_apply(&economic,&replay,&transfer)==STN_DATA_OK);
    CHECK(economic.balance_count==2u);
    CHECK(economic.balances[0].units==75u);
    CHECK(economic.balances[1].units==25u);
    CHECK(economic.total_supply==total);
    CHECK(replay.consumed_count==1u);

    memcpy(snapshot,balances,sizeof(balances));count=economic.balance_count;
    CHECK(stn_transfer_binding_apply(&economic,&replay,&transfer)==STN_DATA_CONTENT);
    CHECK(economic.balance_count==count && economic.total_supply==total);
    CHECK(memcmp(balances,snapshot,sizeof(balances))==0);
    CHECK(replay.consumed_count==1u);

    changed=transfer;changed.units=200u;
    memcpy(snapshot,balances,sizeof(balances));
    CHECK(stn_transfer_binding_apply(&economic,&replay,&changed)==STN_DATA_CONTENT);
    CHECK(memcmp(balances,snapshot,sizeof(balances))==0);
    CHECK(replay.consumed_count==1u);

    changed=transfer;changed.units=10u;wallet(&changed.destination,0x33);
    CHECK(stn_transfer_binding_apply(&economic,&replay,&changed)==STN_DATA_OK);
    CHECK(economic.balance_count==3u && replay.consumed_count==2u);
    CHECK(economic.total_supply==total);

    changed=transfer;changed.units=1u;wallet(&changed.destination,0x44);
    memcpy(snapshot,balances,sizeof(balances));count=economic.balance_count;
    CHECK(stn_transfer_binding_apply(&economic,&replay,&changed)==STN_DATA_CAPACITY);
    CHECK(economic.balance_count==count && economic.total_supply==total);
    CHECK(memcmp(balances,snapshot,sizeof(balances))==0);
    CHECK(replay.consumed_count==2u);

    CHECK(stn_transfer_binding_apply(NULL,&replay,&transfer)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_binding_apply(&economic,NULL,&transfer)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_binding_apply(&economic,&replay,NULL)==STN_DATA_ARGUMENT);

    printf("Transfer binding: %u checks, %u failures.\n",checks,failures);
    return failures?1:0;
}

#ifdef STN_TRANSFER_BINDING_TEST_MAIN
int main(void){return test_transfer_binding();}
#endif
