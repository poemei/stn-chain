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
    stn_block_compensation_evidence evidence,decoded_evidence;
    stn_issuance_record issuance,decoded_issuance;
    stn_transaction tx;
    stn_address miner,wallet;
    uint8_t block_id[32],expected_id[32];
    uint8_t evidence_tx[STN_TX_HEADER_SIZE+STN_TX_BLOCK_COMPENSATION_SIZE];
    uint8_t issuance_tx[STN_TX_HEADER_SIZE+STN_TX_ISSUANCE_SIZE];
    size_t evidence_written=0u,issuance_written=0u,i;

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

    CHECK(stn_block_reward_encode_transactions(&evidence,&issuance,
        evidence_tx,sizeof(evidence_tx),&evidence_written,
        issuance_tx,sizeof(issuance_tx),&issuance_written)==STN_DATA_OK);
    CHECK(evidence_written==sizeof(evidence_tx));
    CHECK(issuance_written==sizeof(issuance_tx));
    CHECK(stn_transaction_decode(evidence_tx,evidence_written,&tx)==STN_DATA_OK);
    CHECK(tx.type==STN_TX_BLOCK_COMPENSATION_EVIDENCE);
    CHECK(stn_block_compensation_decode(tx.record_bytes,tx.record_length,&decoded_evidence)==STN_DATA_OK);
    CHECK(memcmp(decoded_evidence.block_id,evidence.block_id,32u)==0);
    CHECK(memcmp(decoded_evidence.miner.identifier,evidence.miner.identifier,32u)==0);
    CHECK(stn_transaction_decode(issuance_tx,issuance_written,&tx)==STN_DATA_OK);
    CHECK(tx.type==STN_TX_ISSUANCE);
    CHECK(stn_issuance_decode(tx.record_bytes,tx.record_length,&decoded_issuance)==STN_DATA_OK);
    CHECK(stn_issuance_bind_block(&decoded_issuance,&decoded_evidence,&state)==STN_DATA_OK);
    CHECK(stn_block_reward_encode_transactions(&evidence,&issuance,
        evidence_tx,sizeof(evidence_tx)-1u,&evidence_written,
        issuance_tx,sizeof(issuance_tx),&issuance_written)==STN_DATA_CAPACITY);
    CHECK(evidence_written==0u && issuance_written==0u);

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
