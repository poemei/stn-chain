/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_reward.h"
#include <stdio.h>
#include <string.h>

#ifdef STN_BLOCK_REWARD_TEST_MAIN
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int main(void)
{
    stn_compensation_destination map[1];
    stn_compensation_state state;
    stn_block_compensation_evidence evidence;
    stn_issuance_record issuance;
    stn_address miner,wallet;
    uint8_t block_id[32],expected_id[32];
    size_t i;

    memset(&miner,0,sizeof(miner));memset(&wallet,0,sizeof(wallet));
    miner.type=STN_ADDRESS_IDENTITY;wallet.type=STN_ADDRESS_WALLET;
    for(i=0;i<32u;++i){miner.identifier[i]=(uint8_t)(i+1u);wallet.identifier[i]=(uint8_t)(0xa0u+i);block_id[i]=(uint8_t)(0x40u+i);}

    CHECK(stn_compensation_state_initialize(&state,map,1u)==STN_DATA_OK);
    map[0].mining_identity=miner;map[0].wallet=wallet;state.count=1u;
    CHECK(stn_block_reward_build(block_id,&miner,&state,&evidence,&issuance)==STN_DATA_OK);
    CHECK(evidence.version==STN_BLOCK_COMPENSATION_VERSION);
    CHECK(memcmp(evidence.block_id,block_id,32u)==0);
    CHECK(evidence.miner.type==STN_ADDRESS_IDENTITY);
    CHECK(memcmp(evidence.miner.identifier,miner.identifier,32u)==0);
    CHECK(issuance.reason==STN_ISSUANCE_REASON_BLOCK);
    CHECK(issuance.units==STN_ISSUANCE_BLOCK_UNITS);
    CHECK(issuance.units==10000u);
    CHECK(issuance.destination.mining_identity.type==STN_ADDRESS_IDENTITY);
    CHECK(issuance.destination.wallet.type==STN_ADDRESS_WALLET);
    CHECK(memcmp(issuance.destination.wallet.identifier,wallet.identifier,32u)==0);
    CHECK(stn_block_compensation_id(&evidence,expected_id)==STN_DATA_OK);
    CHECK(memcmp(issuance.evidence_id,expected_id,32u)==0);

    memset(block_id,0,sizeof(block_id));
    CHECK(stn_block_reward_build(block_id,&miner,&state,&evidence,&issuance)==STN_DATA_CONTENT);
    block_id[0]=1u;miner.type=STN_ADDRESS_WALLET;
    CHECK(stn_block_reward_build(block_id,&miner,&state,&evidence,&issuance)==STN_DATA_TYPE);
    miner.type=STN_ADDRESS_IDENTITY;miner.identifier[0]^=0xffu;
    CHECK(stn_block_reward_build(block_id,&miner,&state,&evidence,&issuance)==STN_DATA_UNRESOLVED);
    CHECK(stn_block_reward_build(NULL,&miner,&state,&evidence,&issuance)==STN_DATA_ARGUMENT);

    printf("Block reward builder: %u checks, %u failures.\n",checks,failures);
    return failures==0u ? 0 : 1;
}
#endif
