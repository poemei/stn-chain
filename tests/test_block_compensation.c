/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int stn_block_compensation_tests(void)
{
    stn_block_compensation_evidence e={0},decoded={0},unchanged;
    uint8_t canonical[STN_BLOCK_COMPENSATION_CANONICAL_SIZE]={0};
    uint8_t id1[32]={0},id2[32]={0};
    size_t i;
    e.miner.type=STN_ADDRESS_IDENTITY;
    for(i=0u;i<32u;++i){e.block_id[i]=(uint8_t)(i+1u);e.miner.identifier[i]=(uint8_t)(0x80u+i);}

    CHECK(stn_block_compensation_encode(&e,canonical)==STN_DATA_OK);
    CHECK(canonical[0]==STN_BLOCK_COMPENSATION_VERSION);
    CHECK(memcmp(canonical+1u,e.block_id,32u)==0);
    CHECK(memcmp(canonical+33u,e.miner.identifier,32u)==0);
    CHECK(stn_block_compensation_decode(canonical,sizeof(canonical),&decoded)==STN_DATA_OK);
    CHECK(decoded.miner.type==STN_ADDRESS_IDENTITY && memcmp(decoded.block_id,e.block_id,32u)==0 && memcmp(decoded.miner.identifier,e.miner.identifier,32u)==0);
    CHECK(stn_block_compensation_id(&e,id1)==STN_DATA_OK && stn_block_compensation_id(&e,id2)==STN_DATA_OK && memcmp(id1,id2,32u)==0);

    unchanged=decoded;canonical[0]=2u;
    CHECK(stn_block_compensation_decode(canonical,sizeof(canonical),&decoded)==STN_DATA_VERSION && memcmp(&decoded,&unchanged,sizeof(decoded))==0);
    canonical[0]=STN_BLOCK_COMPENSATION_VERSION;
    CHECK(stn_block_compensation_decode(canonical,sizeof(canonical)-1u,&decoded)==STN_DATA_LENGTH);

    e.miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_block_compensation_encode(&e,canonical)==STN_DATA_TYPE);
    e.miner.type=STN_ADDRESS_IDENTITY;memset(e.block_id,0,32u);
    CHECK(stn_block_compensation_encode(&e,canonical)==STN_DATA_CONTENT);
    for(i=0u;i<32u;++i)e.block_id[i]=(uint8_t)(i+1u);
    memset(e.miner.identifier,0,32u);
    CHECK(stn_block_compensation_encode(&e,canonical)==STN_DATA_CONTENT);

    printf("Block compensation evidence: %u checks, %u failures.\n",checks,failures);
    return failures==0u ? 0 : 1;
}

#ifdef STN_BLOCK_COMPENSATION_TEST_MAIN
int main(void){return stn_block_compensation_tests();}
#endif
