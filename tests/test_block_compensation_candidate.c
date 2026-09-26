/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_candidate.h"
#include <stdio.h>
#include <string.h>

#ifdef STN_BLOCK_COMPENSATION_CANDIDATE_TEST_MAIN
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int main(void)
{
    stn_block_compensation_replay replay;
    stn_compensation_destination destinations[1];
    stn_compensation_state compensation;
    stn_economic_balance balances[1];
    stn_economic_state economy;
    stn_block_compensation_evidence evidence,accepted;
    stn_issuance_record issuance;
    stn_transaction evidence_tx,issuance_tx;
    stn_address miner,wallet;
    uint8_t block_id[32],evidence_bytes[STN_BLOCK_COMPENSATION_CANONICAL_SIZE];
    uint8_t issuance_bytes[STN_ISSUANCE_CANONICAL_SIZE];
    uint64_t units=0u;
    size_t i;

    memset(&miner,0,sizeof(miner));memset(&wallet,0,sizeof(wallet));
    miner.type=STN_ADDRESS_IDENTITY;wallet.type=STN_ADDRESS_WALLET;
    for(i=0u;i<32u;++i){miner.identifier[i]=(uint8_t)(i+1u);wallet.identifier[i]=(uint8_t)(0x80u+i);block_id[i]=(uint8_t)(0x40u+i);}
    CHECK(stn_compensation_state_initialize(&compensation,destinations,1u)==STN_DATA_OK);
    destinations[0].mining_identity=miner;destinations[0].wallet=wallet;compensation.count=1u;
    CHECK(stn_economic_state_initialize(&economy,balances,1u)==STN_DATA_OK);
    stn_block_compensation_replay_initialize(&replay);

    memset(&evidence,0,sizeof(evidence));evidence.version=STN_BLOCK_COMPENSATION_VERSION;memcpy(evidence.block_id,block_id,32u);evidence.miner=miner;
    memset(&issuance,0,sizeof(issuance));issuance.reason=STN_ISSUANCE_REASON_BLOCK;issuance.units=STN_ISSUANCE_BLOCK_UNITS;issuance.destination=destinations[0];
    CHECK(stn_block_compensation_id(&evidence,issuance.evidence_id)==STN_DATA_OK);
    CHECK(stn_block_compensation_encode(&evidence,evidence_bytes)==STN_DATA_OK);
    CHECK(stn_issuance_encode(&issuance,issuance_bytes)==STN_DATA_OK);

    evidence_tx.version=1u;evidence_tx.type=STN_TX_BLOCK_COMPENSATION_EVIDENCE;evidence_tx.record_bytes=evidence_bytes;evidence_tx.record_length=sizeof(evidence_bytes);
    issuance_tx.version=1u;issuance_tx.type=STN_TX_ISSUANCE;issuance_tx.record_bytes=issuance_bytes;issuance_tx.record_length=sizeof(issuance_bytes);

    CHECK(stn_block_compensation_candidate_apply(&evidence_tx,&issuance_tx,&replay,&compensation,&economy,&accepted)==STN_DATA_OK);
    CHECK(memcmp(&accepted,&evidence,sizeof(evidence))==0);
    CHECK(stn_economic_state_balance(&economy,&wallet,&units)==STN_DATA_OK);
    CHECK(units==STN_ISSUANCE_BLOCK_UNITS);
    CHECK(replay.count==1u);
    CHECK(stn_block_compensation_candidate_apply(&evidence_tx,&issuance_tx,&replay,&compensation,&economy,NULL)==STN_DATA_DUPLICATE);
    CHECK(economy.total_supply==STN_ISSUANCE_BLOCK_UNITS);

    issuance_tx.type=STN_TX_TRANSFER;
    CHECK(stn_block_compensation_candidate_apply(&evidence_tx,&issuance_tx,&replay,&compensation,&economy,NULL)==STN_DATA_TYPE);
    CHECK(economy.total_supply==STN_ISSUANCE_BLOCK_UNITS);
    issuance_tx.type=STN_TX_ISSUANCE;
    evidence_tx.type=STN_TX_SHARE_EVIDENCE;
    CHECK(stn_block_compensation_candidate_apply(&evidence_tx,&issuance_tx,&replay,&compensation,&economy,NULL)==STN_DATA_TYPE);
    CHECK(economy.total_supply==STN_ISSUANCE_BLOCK_UNITS);

    printf("Block compensation candidate: %u checks, %u failures.\n",checks,failures);
    return failures==0u ? 0 : 1;
}
#endif
