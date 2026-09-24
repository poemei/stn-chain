/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_acceptance.h"
#include "stn_wallet.h"

#include <stdio.h>
#include <string.h>

int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],uint8_t[64]);

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

static int sign_transfer(const stn_transfer *transfer,const uint8_t pk[32],
    const uint8_t sk[32],uint8_t sig[64])
{
    uint8_t statement[STN_TRANSFER_AUTHORIZATION_STATEMENT_SIZE];
    if(stn_transfer_authorization_statement(transfer,pk,statement)!=STN_DATA_OK)return -1;
    return stn_ed25519_sign(statement,sizeof(statement),pk,sk,sig);
}

int test_transfer_acceptance(void)
{
    static const uint8_t sk[32]={
        0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,
        0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    static const uint8_t pk[32]={
        0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
        0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
    stn_economic_balance balances[3],snapshot[3];
    uint8_t consumed[2u*STN_TRANSFER_REPLAY_KEY_SIZE],sig[64],bad_sig[64];
    stn_economic_state economic={0};
    stn_transfer_replay_state replay={0};
    stn_issuance_record issue={0};
    stn_address identity={0},source={0};
    stn_transfer transfer={0},changed;
    uint64_t total;

    identity.type=STN_ADDRESS_IDENTITY;memcpy(identity.identifier,pk,32u);
    CHECK(stn_wallet_derive(&identity,&source)==STN_DATA_OK);
    memset(balances,0,sizeof(balances));memset(consumed,0,sizeof(consumed));
    CHECK(stn_economic_state_initialize(&economic,balances,3u)==STN_DATA_OK);
    issue.reason=STN_ISSUANCE_REASON_BLOCK;issue.units=STN_ISSUANCE_BLOCK_UNITS;
    issue.destination.mining_identity=identity;issue.destination.wallet=source;
    CHECK(stn_economic_state_apply(&economic,&issue)==STN_DATA_OK);
    total=economic.total_supply;
    stn_transfer_replay_initialize(&replay,consumed,2u);

    transfer.source=source;transfer.destination.type=STN_ADDRESS_WALLET;
    memset(transfer.destination.identifier,0x22,32u);transfer.units=25u;
    CHECK(sign_transfer(&transfer,pk,sk,sig)==0);
    CHECK(stn_transfer_accept(&economic,&replay,&transfer,pk,sig)==STN_DATA_OK);
    CHECK(economic.balance_count==2u && economic.balances[0].units==75u);
    CHECK(economic.balances[1].units==25u && economic.total_supply==total);
    CHECK(replay.consumed_count==1u);

    memcpy(snapshot,balances,sizeof(balances));
    CHECK(stn_transfer_accept(&economic,&replay,&transfer,pk,sig)==STN_DATA_CONTENT);
    CHECK(memcmp(snapshot,balances,sizeof(balances))==0 && replay.consumed_count==1u);

    memcpy(bad_sig,sig,sizeof(sig));bad_sig[0]^=1u;
    changed=transfer;changed.units=10u;
    memcpy(snapshot,balances,sizeof(balances));
    CHECK(stn_transfer_accept(&economic,&replay,&changed,pk,bad_sig)==STN_DATA_CONTENT);
    CHECK(memcmp(snapshot,balances,sizeof(balances))==0 && replay.consumed_count==1u);

    changed=transfer;changed.units=200u;
    CHECK(sign_transfer(&changed,pk,sk,sig)==0);
    memcpy(snapshot,balances,sizeof(balances));
    CHECK(stn_transfer_accept(&economic,&replay,&changed,pk,sig)==STN_DATA_CONTENT);
    CHECK(memcmp(snapshot,balances,sizeof(balances))==0 && replay.consumed_count==1u);

    changed=transfer;changed.units=10u;memset(changed.destination.identifier,0x33,32u);
    CHECK(sign_transfer(&changed,pk,sk,sig)==0);
    CHECK(stn_transfer_accept(&economic,&replay,&changed,pk,sig)==STN_DATA_OK);
    CHECK(replay.consumed_count==2u && economic.total_supply==total);

    CHECK(stn_transfer_accept(NULL,&replay,&transfer,pk,sig)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_accept(&economic,NULL,&transfer,pk,sig)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_accept(&economic,&replay,NULL,pk,sig)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_accept(&economic,&replay,&transfer,NULL,sig)==STN_DATA_ARGUMENT);
    CHECK(stn_transfer_accept(&economic,&replay,&transfer,pk,NULL)==STN_DATA_ARGUMENT);

    printf("Transfer acceptance: %u checks, %u failures.\n",checks,failures);
    return failures?1:0;
}
#ifdef STN_TRANSFER_ACCEPTANCE_TEST_MAIN
int main(void){return test_transfer_acceptance();}
#endif
