/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block_compensation_candidate.h"
#include "stn_block_reward.h"
#include "stn_compensation_state.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>

static int fail_at(int line){fprintf(stderr,"block compensation pending line %d\n",line);return 1;}
#define CHECK(x) do{if(!(x))return fail_at(__LINE__);}while(0)

static void genesis(uint8_t p[364])
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(p,0,364);memcpy(p,"STNB",4);p[5]=3;p[8]=1;p[163]=1;p[167]=196;p[171]=192;
    memcpy(p+172,"STNT",4);p[177]=1;p[179]=1;p[183]=180;memcpy(p+184,"STNR",4);
    p[189]=1;p[191]=1;p[192]=1;p[256]=3;memcpy(p+88,commitment,32);
    memset(p+120,255,32);p[120]=127;
}

int main(void)
{
    stn_pending pool={0};stn_storage_view active={0};stn_block_span span;
    stn_compensation_state compensation;stn_compensation_destination storage[2],destination={0};
    stn_block_compensation_replay replay;stn_block_compensation_evidence evidence;
    stn_issuance_record issuance;stn_hash_provider hash={stn_sha256,NULL};
    stn_validation_report report;uint8_t block[364],block_id[32],evidence_tx[STN_BLOCK_REWARD_EVIDENCE_TX_SIZE];
    uint8_t issuance_tx[STN_BLOCK_REWARD_ISSUANCE_TX_SIZE],id[32],body[2048];
    size_t evidence_length=0,issuance_length=0,written=0;uint32_t count=0;
    stn_transaction first,second;size_t offset=0;uint32_t n;

    genesis(block);span.bytes=block;span.length=sizeof(block);active.blocks=&span;active.count=1;
    CHECK(stn_chain_block_id(block,sizeof(block),&hash,block_id)==STN_DATA_OK);
    CHECK(stn_compensation_state_initialize(&compensation,storage,2u)==STN_DATA_OK);
    destination.mining_identity.type=STN_ADDRESS_IDENTITY;destination.wallet.type=STN_ADDRESS_WALLET;
    memset(destination.mining_identity.identifier,0x11,32u);memset(destination.wallet.identifier,0x22,32u);
    CHECK(stn_compensation_state_apply(&compensation,&destination)==STN_DATA_OK);
    active.state.compensation=&compensation;stn_block_compensation_replay_initialize(&replay);
    active.state.block_compensation=&replay;

    CHECK(stn_block_reward_build(block_id,&destination.mining_identity,&compensation,&evidence,&issuance)==STN_DATA_OK);
    CHECK(stn_block_reward_encode_transactions(&evidence,&issuance,evidence_tx,sizeof(evidence_tx),&evidence_length,issuance_tx,sizeof(issuance_tx),&issuance_length)==STN_DATA_OK);

    CHECK(stn_block_compensation_pending_admit(&pool,evidence_tx,evidence_length,NULL,&active,&hash,&report,id)==STN_PENDING_ACCEPTED);
    CHECK(stn_block_compensation_pending_admit(&pool,issuance_tx,issuance_length,NULL,&active,&hash,&report,id)==STN_PENDING_ACCEPTED);
    CHECK(pool.count==2u);
    CHECK(stn_block_compensation_pending_assemble(&pool,NULL,&active,body,sizeof(body),&written,&count)==STN_DATA_OK);
    CHECK(count==2u&&written>0u);

    n=((uint32_t)body[offset]<<24)|((uint32_t)body[offset+1]<<16)|((uint32_t)body[offset+2]<<8)|body[offset+3];offset+=4u;
    CHECK(stn_transaction_decode(body+offset,n,&first)==STN_DATA_OK);offset+=n;
    n=((uint32_t)body[offset]<<24)|((uint32_t)body[offset+1]<<16)|((uint32_t)body[offset+2]<<8)|body[offset+3];offset+=4u;
    CHECK(stn_transaction_decode(body+offset,n,&second)==STN_DATA_OK);
    CHECK(first.type==STN_TX_BLOCK_COMPENSATION_EVIDENCE&&second.type==STN_TX_ISSUANCE);
    CHECK(stn_block_compensation_candidate_apply(&first,&second,&replay,&compensation,
        &(stn_economic_state){0},NULL)!=STN_DATA_TYPE);

    stn_pending_clear(&pool);
    puts("Block compensation pending pairing tests passed.");
    return 0;
}
