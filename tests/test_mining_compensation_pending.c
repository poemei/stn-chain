/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_mining.h"
#include "stn_compensation.h"
#include "stn_transaction.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do {++checks;if(!(e)){++failures;fprintf(stderr,"mining compensation pending line %d: %s\n",__LINE__,#e);}} while(0)

static int destination_transaction(
    uint8_t identity_byte,
    uint8_t wallet_byte,
    uint8_t output[STN_TX_HEADER_SIZE+STN_COMPENSATION_DESTINATION_SIZE],
    size_t *written)
{
    stn_compensation_destination destination={0};
    stn_transaction tx={0};
    uint8_t canonical[STN_COMPENSATION_DESTINATION_SIZE];

    destination.mining_identity.type=STN_ADDRESS_IDENTITY;
    destination.wallet.type=STN_ADDRESS_WALLET;
    memset(destination.mining_identity.identifier,identity_byte,STN_ADDRESS_ID_SIZE);
    memset(destination.wallet.identifier,wallet_byte,STN_ADDRESS_ID_SIZE);

    if(stn_compensation_destination_encode(&destination,canonical)!=STN_DATA_OK)return 0;

    tx.version=1u;
    tx.type=STN_TX_COMPENSATION_DESTINATION;
    tx.record_bytes=canonical;
    tx.record_length=STN_COMPENSATION_DESTINATION_SIZE;

    return stn_transaction_encode(
        &tx,
        output,
        STN_TX_HEADER_SIZE+STN_COMPENSATION_DESTINATION_SIZE,
        written)==STN_DATA_OK;
}

int test_mining_compensation_pending(void)
{
    stn_pending pending={0};
    stn_address miner={0},wallet={0};
    uint8_t first[STN_TX_HEADER_SIZE+STN_COMPENSATION_DESTINATION_SIZE];
    uint8_t conflict[STN_TX_HEADER_SIZE+STN_COMPENSATION_DESTINATION_SIZE];
    size_t first_length=0u,conflict_length=0u;

    miner.type=STN_ADDRESS_IDENTITY;
    memset(miner.identifier,0x11,STN_ADDRESS_ID_SIZE);

    CHECK(stn_mining_pending_compensation_lookup(
        &pending,&miner,&wallet)==STN_DATA_UNRESOLVED);

    CHECK(destination_transaction(0x11,0x22,first,&first_length));
    pending.entries[0].transaction=first;
    pending.entries[0].length=first_length;
    pending.count=1u;
    pending.bytes=first_length;

    CHECK(stn_mining_pending_compensation_lookup(
        &pending,&miner,&wallet)==STN_DATA_OK);
    CHECK(wallet.type==STN_ADDRESS_WALLET);
    CHECK(wallet.identifier[0]==0x22 &&
        wallet.identifier[STN_ADDRESS_ID_SIZE-1u]==0x22);

    miner.identifier[0]=0x33;
    CHECK(stn_mining_pending_compensation_lookup(
        &pending,&miner,&wallet)==STN_DATA_UNRESOLVED);
    miner.identifier[0]=0x11;

    CHECK(destination_transaction(0x11,0x44,conflict,&conflict_length));
    pending.entries[1].transaction=conflict;
    pending.entries[1].length=conflict_length;
    pending.count=2u;
    pending.bytes=first_length+conflict_length;

    CHECK(stn_mining_pending_compensation_lookup(
        &pending,&miner,&wallet)==STN_DATA_DUPLICATE);

    miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_mining_pending_compensation_lookup(
        &pending,&miner,&wallet)==STN_DATA_TYPE);

    printf("Mining pending compensation: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}
