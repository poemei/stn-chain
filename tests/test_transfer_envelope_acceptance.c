/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope_acceptance.h"
#include "stn_wallet.h"
#include <stdio.h>
#include <string.h>
int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],uint8_t[64]);
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
static int sign_e(stn_transfer_envelope *e,const uint8_t pk[32],const uint8_t sk[32]){
 uint8_t s[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE];
 if(stn_transfer_envelope_authorization_statement(e,s)!=STN_DATA_OK)return -1;
 return stn_ed25519_sign(s,sizeof(s),pk,sk,e->signature);
}
int test_transfer_envelope_acceptance(void){
 static const uint8_t sk[32]={0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
 static const uint8_t pk[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
 stn_economic_balance balances[3],snapshot[3];uint8_t consumed[2*STN_TRANSFER_ENVELOPE_REPLAY_KEY_SIZE];
 stn_economic_state economic={0};stn_transfer_envelope_replay_state replay={0};
 stn_transfer_envelope e={0},changed;stn_address id={0};stn_issuance_record issue={0};uint64_t total;
 id.type=STN_ADDRESS_IDENTITY;memcpy(id.identifier,pk,32);memcpy(e.controller,pk,32);e.nonce[31]=1;
 CHECK(stn_wallet_derive(&id,&e.transfer.source)==STN_DATA_OK);
 e.transfer.destination.type=STN_ADDRESS_WALLET;memset(e.transfer.destination.identifier,0x22,32);e.transfer.units=25;
 memset(balances,0,sizeof(balances));memset(consumed,0,sizeof(consumed));
 CHECK(stn_economic_state_initialize(&economic,balances,3)==STN_DATA_OK);
 issue.reason=STN_ISSUANCE_REASON_BLOCK;issue.units=STN_ISSUANCE_BLOCK_UNITS;issue.destination.mining_identity=id;issue.destination.wallet=e.transfer.source;
 CHECK(stn_economic_state_apply(&economic,&issue)==STN_DATA_OK);total=economic.total_supply;
 stn_transfer_envelope_replay_initialize(&replay,consumed,2);
 CHECK(sign_e(&e,pk,sk)==0);
 CHECK(stn_transfer_envelope_accept(&economic,&replay,&e)==STN_DATA_OK);
 CHECK(economic.balance_count==2 && economic.balances[0].units==75 && economic.balances[1].units==25);
 CHECK(economic.total_supply==total && replay.consumed_count==1);
 memcpy(snapshot,balances,sizeof(balances));
 CHECK(stn_transfer_envelope_accept(&economic,&replay,&e)==STN_DATA_DUPLICATE);
 CHECK(memcmp(snapshot,balances,sizeof(balances))==0 && replay.consumed_count==1);
 changed=e;changed.nonce[31]=2;CHECK(sign_e(&changed,pk,sk)==0);
 CHECK(stn_transfer_envelope_accept(&economic,&replay,&changed)==STN_DATA_OK);
 CHECK(economic.balances[0].units==50 && economic.balances[1].units==50 && replay.consumed_count==2);
 changed=e;changed.nonce[31]=3;changed.transfer.units=1;CHECK(sign_e(&changed,pk,sk)==0);
 memcpy(snapshot,balances,sizeof(balances));
 CHECK(stn_transfer_envelope_accept(&economic,&replay,&changed)==STN_DATA_CAPACITY);
 CHECK(memcmp(snapshot,balances,sizeof(balances))==0 && replay.consumed_count==2);
 changed=e;changed.nonce[31]=4;changed.transfer.units=200;CHECK(sign_e(&changed,pk,sk)==0);
 CHECK(stn_transfer_envelope_accept(&economic,&replay,&changed)==STN_DATA_CAPACITY);
 changed=e;changed.signature[0]^=1;memcpy(snapshot,balances,sizeof(balances));
 CHECK(stn_transfer_envelope_accept(&economic,&replay,&changed)==STN_DATA_CONTENT);
 CHECK(memcmp(snapshot,balances,sizeof(balances))==0 && replay.consumed_count==2);
 CHECK(stn_transfer_envelope_accept(NULL,&replay,&e)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_accept(&economic,NULL,&e)==STN_DATA_ARGUMENT);
 CHECK(stn_transfer_envelope_accept(&economic,&replay,NULL)==STN_DATA_ARGUMENT);
 printf("Transfer envelope acceptance: %u checks, %u failures.\n",checks,failures);return failures?1:0;
}
#ifdef STN_TRANSFER_ENVELOPE_ACCEPTANCE_TEST_MAIN
int main(void){return test_transfer_envelope_acceptance();}
#endif
