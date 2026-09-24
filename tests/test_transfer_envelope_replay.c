/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transfer_envelope_replay.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int test_transfer_envelope_replay(void)
{
 stn_transfer_envelope a={0},b={0};uint8_t key[64],key2[64],store[2][64];
 stn_transfer_envelope_replay_state s={0},bad={0};
 memset(a.controller,0x11,32);a.nonce[31]=1;a.transfer.source.type=STN_ADDRESS_WALLET;
 a.transfer.destination.type=STN_ADDRESS_WALLET;memset(a.transfer.source.identifier,0x22,32);
 memset(a.transfer.destination.identifier,0x33,32);a.transfer.units=25;b=a;
 CHECK(stn_transfer_envelope_replay_key(&a,key)==STN_TRANSFER_ENVELOPE_REPLAY_FRESH);
 CHECK(memcmp(key,a.controller,32)==0 && memcmp(key+32,a.nonce,32)==0);
 stn_transfer_envelope_replay_initialize(&s,(uint8_t*)store,2);
 CHECK(stn_transfer_envelope_replay_check(&s,&a)==STN_TRANSFER_ENVELOPE_REPLAY_FRESH);
 CHECK(stn_transfer_envelope_replay_consume(&s,&a)==STN_TRANSFER_ENVELOPE_REPLAY_FRESH);
 CHECK(s.consumed_count==1u);
 CHECK(stn_transfer_envelope_replay_check(&s,&a)==STN_TRANSFER_ENVELOPE_REPLAY_DUPLICATE);
 CHECK(stn_transfer_envelope_replay_consume(&s,&a)==STN_TRANSFER_ENVELOPE_REPLAY_DUPLICATE);
 b.nonce[31]=2;
 CHECK(stn_transfer_envelope_replay_key(&b,key2)==STN_TRANSFER_ENVELOPE_REPLAY_FRESH);
 CHECK(memcmp(key,key2,64)!=0);
 CHECK(stn_transfer_envelope_replay_check(&s,&b)==STN_TRANSFER_ENVELOPE_REPLAY_FRESH);
 CHECK(stn_transfer_envelope_replay_consume(&s,&b)==STN_TRANSFER_ENVELOPE_REPLAY_FRESH);
 CHECK(s.consumed_count==2u);
 b=a;b.nonce[31]=3;b.transfer.units=25;
 CHECK(stn_transfer_envelope_replay_consume(&s,&b)==STN_TRANSFER_ENVELOPE_REPLAY_CAPACITY);
 CHECK(s.consumed_count==2u);
 b=a;b.transfer.units=99;
 CHECK(stn_transfer_envelope_replay_check(&s,&b)==STN_TRANSFER_ENVELOPE_REPLAY_DUPLICATE);
 memset(b.nonce,0,32);
 CHECK(stn_transfer_envelope_replay_key(&b,key2)==STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT);
 CHECK(stn_transfer_envelope_replay_key(NULL,key2)==STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT);
 CHECK(stn_transfer_envelope_replay_key(&a,NULL)==STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT);
 CHECK(stn_transfer_envelope_replay_check(NULL,&a)==STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT);
 bad.consumed=NULL;bad.consumed_capacity=1;
 CHECK(stn_transfer_envelope_replay_check(&bad,&a)==STN_TRANSFER_ENVELOPE_REPLAY_ARGUMENT);
 printf("Transfer envelope replay: %u checks, %u failures.\n",checks,failures);return failures?1:0;
}
#ifdef STN_TRANSFER_ENVELOPE_REPLAY_TEST_MAIN
int main(void){return test_transfer_envelope_replay();}
#endif
