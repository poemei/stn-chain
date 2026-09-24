/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_compensation_state.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"compensation state line %d: %s\n",__LINE__,#e);}}while(0)
static void mapping(stn_compensation_destination *d,uint8_t identity,uint8_t wallet)
{memset(d,0,sizeof(*d));d->mining_identity.type=STN_ADDRESS_IDENTITY;d->wallet.type=STN_ADDRESS_WALLET;memset(d->mining_identity.identifier,identity,32u);memset(d->wallet.identifier,wallet,32u);}
static void vectors(void)
{
 stn_compensation_destination store[2],d,snapshot[2];stn_compensation_state s={0};stn_address out={0},id={0};
 CHECK(stn_compensation_state_initialize(&s,store,2u)==STN_DATA_OK);
 mapping(&d,0x22,0xaa);CHECK(stn_compensation_state_apply(&s,&d)==STN_DATA_OK);
 mapping(&d,0x11,0xbb);CHECK(stn_compensation_state_apply(&s,&d)==STN_DATA_OK);
 CHECK(s.count==2u && s.entries[0].mining_identity.identifier[0]==0x11u && s.entries[1].mining_identity.identifier[0]==0x22u);
 id.type=STN_ADDRESS_IDENTITY;memset(id.identifier,0x22,32u);
 CHECK(stn_compensation_state_lookup(&s,&id,&out)==STN_DATA_OK && out.type==STN_ADDRESS_WALLET && out.identifier[0]==0xaau);
 mapping(&d,0x22,0xaa);CHECK(stn_compensation_state_apply(&s,&d)==STN_DATA_OK && s.count==2u);
 memcpy(snapshot,store,sizeof(store));mapping(&d,0x22,0xcc);
 CHECK(stn_compensation_state_apply(&s,&d)==STN_DATA_DUPLICATE);
 CHECK(memcmp(snapshot,store,sizeof(store))==0 && s.count==2u);
 mapping(&d,0x33,0xdd);CHECK(stn_compensation_state_apply(&s,&d)==STN_DATA_CAPACITY);
 id.identifier[0]=0x44;memset(id.identifier,0x44,32u);CHECK(stn_compensation_state_lookup(&s,&id,&out)==STN_DATA_UNRESOLVED);
 d.wallet.type=STN_ADDRESS_IDENTITY;CHECK(stn_compensation_state_apply(&s,&d)==STN_DATA_TYPE);
 CHECK(stn_compensation_state_apply(NULL,&d)==STN_DATA_ARGUMENT);
 CHECK(stn_compensation_state_apply(&s,NULL)==STN_DATA_ARGUMENT);
 CHECK(stn_compensation_state_initialize(NULL,store,2u)==STN_DATA_ARGUMENT);
 CHECK(stn_compensation_state_initialize(&s,NULL,1u)==STN_DATA_ARGUMENT);
 CHECK(stn_compensation_state_lookup(NULL,&id,&out)==STN_DATA_ARGUMENT);
 id.type=STN_ADDRESS_WALLET;CHECK(stn_compensation_state_lookup(&s,&id,&out)==STN_DATA_TYPE);
}
int test_compensation_state(void);
int test_compensation_state(void){vectors();printf("Compensation accepted state: %u checks, %u failures.\n",checks,failures);return failures==0?0:1;}
#ifdef STN_COMPENSATION_STATE_TEST_MAIN
int main(void){return test_compensation_state();}
#endif
