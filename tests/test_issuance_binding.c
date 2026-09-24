/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_issuance_binding.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"issuance binding line %d: %s\n",__LINE__,#e);}}while(0)
static void vectors(void)
{
 stn_compensation_destination map,store[1];stn_compensation_state state={0};
 stn_share_evidence share={0};stn_issuance_record issuance={0};uint8_t id[32];
 map.mining_identity.type=STN_ADDRESS_IDENTITY;map.wallet.type=STN_ADDRESS_WALLET;
 memset(map.mining_identity.identifier,0x11,32u);memset(map.wallet.identifier,0x22,32u);
 CHECK(stn_compensation_state_initialize(&state,store,1u)==STN_DATA_OK);
 CHECK(stn_compensation_state_apply(&state,&map)==STN_DATA_OK);
 share.miner=map.mining_identity;
 share.template_header[0]=0; /* malformed evidence proves binding fails closed */
 issuance.reason=STN_ISSUANCE_REASON_SHARE;issuance.units=STN_ISSUANCE_SHARE_UNITS;issuance.destination=map;
 CHECK(stn_issuance_bind_share(&issuance,&share,&state)!=STN_DATA_OK);
 CHECK(stn_issuance_bind_share(NULL,&share,&state)==STN_DATA_ARGUMENT);
 CHECK(stn_issuance_bind_share(&issuance,NULL,&state)==STN_DATA_ARGUMENT);
 CHECK(stn_issuance_bind_share(&issuance,&share,NULL)==STN_DATA_ARGUMENT);
 issuance.reason=STN_ISSUANCE_REASON_BLOCK;
 CHECK(stn_issuance_bind_share(&issuance,&share,&state)==STN_DATA_CONTENT);
 issuance.reason=STN_ISSUANCE_REASON_SHARE;issuance.units=2u;
 CHECK(stn_issuance_bind_share(&issuance,&share,&state)==STN_DATA_CONTENT);
 issuance.units=1u;issuance.destination.wallet.type=STN_ADDRESS_IDENTITY;
 CHECK(stn_issuance_bind_share(&issuance,&share,&state)==STN_DATA_TYPE);
 (void)id;
}
int test_issuance_binding(void);
int test_issuance_binding(void){vectors();printf("Issuance binding: %u checks, %u failures.\n",checks,failures);return failures==0?0:1;}
#ifdef STN_ISSUANCE_BINDING_TEST_MAIN
int main(void){return test_issuance_binding();}
#endif
