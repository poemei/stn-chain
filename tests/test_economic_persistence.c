/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_economic_persistence.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"economic persistence line %d: %s\n",__LINE__,#e);}}while(0)

static void vectors(void)
{
    stn_economic_balance balances[2]={0},loaded[2]={0};stn_economic_state state={0},out={0},saved={0};
    uint8_t bytes[STN_ECONOMIC_PERSISTENCE_HEADER_SIZE+2u*STN_ECONOMIC_PERSISTENCE_BALANCE_SIZE],copy[sizeof(bytes)];size_t size=0u,written=99u;
    memset(balances[0].wallet_id,0x11,32u);balances[0].units=25u;memset(balances[1].wallet_id,0x22,32u);balances[1].units=75u;
    CHECK(stn_economic_state_initialize(&state,balances,2u)==STN_DATA_OK);state.balance_count=2u;state.total_supply=100u;
    CHECK(stn_economic_persistence_size(&state,&size)==STN_DATA_OK && size==sizeof(bytes));
    CHECK(stn_economic_persistence_encode(&state,bytes,sizeof(bytes),&written)==STN_DATA_OK && written==sizeof(bytes));
    CHECK(bytes[0]==STN_ECONOMIC_PERSISTENCE_VERSION);CHECK(stn_economic_persistence_decode(bytes,sizeof(bytes),&out,loaded,2u)==STN_DATA_OK);
    CHECK(out.total_supply==100u && out.balance_count==2u);CHECK(memcmp(out.balances,balances,sizeof(balances))==0);
    saved=out;memcpy(copy,bytes,sizeof(bytes));copy[0]=2u;CHECK(stn_economic_persistence_decode(copy,sizeof(copy),&out,loaded,2u)==STN_DATA_VERSION && out.total_supply==saved.total_supply);
    CHECK(stn_economic_persistence_decode(bytes,sizeof(bytes)-1u,&out,loaded,2u)==STN_DATA_LENGTH);
    CHECK(stn_economic_persistence_decode(bytes,sizeof(bytes),&out,loaded,1u)==STN_DATA_CAPACITY);
    memcpy(copy,bytes,sizeof(bytes));memset(copy+STN_ECONOMIC_PERSISTENCE_HEADER_SIZE+40u,0x11,32u);CHECK(stn_economic_persistence_decode(copy,sizeof(copy),&out,loaded,2u)==STN_DATA_CONTENT);
    memcpy(copy,bytes,sizeof(bytes));copy[8]^=1u;CHECK(stn_economic_persistence_decode(copy,sizeof(copy),&out,loaded,2u)==STN_DATA_CONTENT);
    written=99u;CHECK(stn_economic_persistence_encode(&state,bytes,sizeof(bytes)-1u,&written)==STN_DATA_CAPACITY && written==0u);
    state.total_supply=99u;CHECK(stn_economic_persistence_encode(&state,bytes,sizeof(bytes),&written)==STN_DATA_CONTENT);
    CHECK(stn_economic_persistence_size(NULL,&size)==STN_DATA_ARGUMENT);CHECK(stn_economic_persistence_size(&state,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_economic_persistence_decode(NULL,sizeof(bytes),&out,loaded,2u)==STN_DATA_ARGUMENT);
}
int test_economic_persistence(void){vectors();printf("Economic persistence: %u checks, %u failures.\n",checks,failures);return failures==0?0:1;}
#ifdef STN_ECONOMIC_PERSISTENCE_TEST_MAIN
int main(void){return test_economic_persistence();}
#endif
