/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_issuance_binding.h"
#include "stn_sha256.h"
#include "stn_chain.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"issuance binding line %d: %s\n",__LINE__,#e);}}while(0)

static void make_valid_share(stn_share_evidence *share,const stn_address *miner)
{
    static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
    static const uint8_t block_domain[]="STN-CHAIN:BLOCK:ID:1";
    stn_hash_provider provider={stn_sha256,NULL};
    stn_block_header header={0};
    uint8_t share_target[32],candidate[STN_SHARE_TEMPLATE_HEADER_SIZE],hash[32];
    size_t written=0u;
    uint64_t nonce;

    memset(share,0,sizeof(*share));
    share->miner=*miner;
    header.version=STN_POW_BLOCK_VERSION;
    header.height=1u;
    header.timestamp=1u;
    header.previous_hash[31]=1u;
    header.reserved_target[0]=0x0fu;
    header.reserved_target[31]=0xffu;
    CHECK(stn_block_body_commitment(NULL,0u,0u,&provider,
        header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_header_encode(&header,share->template_header,
        sizeof(share->template_header),&written)==STN_DATA_OK);
    CHECK(written==STN_SHARE_TEMPLATE_HEADER_SIZE);
    memcpy(share->body_commitment,header.transaction_commitment,32u);
    CHECK(stn_sha256(NULL,work_domain,sizeof(work_domain),share->template_header,
        sizeof(share->template_header),share->work_id)==STN_DATA_OK);
    CHECK(stn_economy_share_target(header.reserved_target,share_target)==STN_DATA_OK);

    for(nonce=0u;;++nonce){
        stn_block_header trial=header;
        trial.reserved_work_nonce=nonce;
        CHECK(stn_block_header_encode(&trial,candidate,sizeof(candidate),
            &written)==STN_DATA_OK);
        CHECK(stn_sha256(NULL,block_domain,sizeof(block_domain),candidate,
            sizeof(candidate),hash)==STN_DATA_OK);
        if(stn_pow_compare(hash,share_target)==STN_DATA_OK)break;
        CHECK(nonce!=UINT64_MAX);
    }
    share->nonce=nonce;
    CHECK(stn_share_verify_evidence(share,&provider,hash)==STN_DATA_OK);
}

static void vectors(void)
{
    stn_compensation_destination map,store[1];
    stn_compensation_state state={0};
    stn_share_evidence share={0};
    stn_block_compensation_evidence block={0};
    stn_issuance_record issuance={0},changed;
    uint8_t share_id[32],block_id[32];

    memset(&map,0,sizeof(map));
    map.mining_identity.type=STN_ADDRESS_IDENTITY;
    map.wallet.type=STN_ADDRESS_WALLET;
    memset(map.mining_identity.identifier,0x11,32u);
    memset(map.wallet.identifier,0x22,32u);

    CHECK(stn_compensation_state_initialize(&state,store,1u)==STN_DATA_OK);
    CHECK(stn_compensation_state_apply(&state,&map)==STN_DATA_OK);
    make_valid_share(&share,&map.mining_identity);
    CHECK(stn_share_id(&share,share_id)==STN_DATA_OK);

    issuance.reason=STN_ISSUANCE_REASON_SHARE;
    issuance.units=STN_ISSUANCE_SHARE_UNITS;
    memcpy(issuance.evidence_id,share_id,sizeof(share_id));
    issuance.destination=map;

    CHECK(stn_issuance_bind_share(&issuance,&share,&state)==STN_DATA_OK);

    changed=issuance;changed.evidence_id[0]^=1u;
    CHECK(stn_issuance_bind_share(&changed,&share,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.destination.mining_identity.identifier[0]^=1u;
    CHECK(stn_issuance_bind_share(&changed,&share,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.destination.wallet.identifier[0]^=1u;
    CHECK(stn_issuance_bind_share(&changed,&share,&state)==STN_DATA_CONTENT);
    CHECK(stn_issuance_bind_share(NULL,&share,&state)==STN_DATA_ARGUMENT);
    CHECK(stn_issuance_bind_share(&issuance,NULL,&state)==STN_DATA_ARGUMENT);
    CHECK(stn_issuance_bind_share(&issuance,&share,NULL)==STN_DATA_ARGUMENT);
    changed=issuance;changed.reason=STN_ISSUANCE_REASON_BLOCK;
    CHECK(stn_issuance_bind_share(&changed,&share,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.units=2u;
    CHECK(stn_issuance_bind_share(&changed,&share,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.destination.wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_issuance_bind_share(&changed,&share,&state)==STN_DATA_TYPE);

    memset(&block,0,sizeof(block));
    block.version=STN_BLOCK_COMPENSATION_VERSION;
    memset(block.block_id,0x33,sizeof(block.block_id));
    block.miner=map.mining_identity;
    CHECK(stn_block_compensation_id(&block,block_id)==STN_DATA_OK);
    memset(&issuance,0,sizeof(issuance));
    issuance.reason=STN_ISSUANCE_REASON_BLOCK;
    issuance.units=STN_ISSUANCE_BLOCK_UNITS;
    memcpy(issuance.evidence_id,block_id,sizeof(block_id));
    issuance.destination=map;
    CHECK(stn_issuance_bind_block(&issuance,&block,&state)==STN_DATA_OK);
    CHECK(issuance.units==10000u);

    changed=issuance;changed.evidence_id[0]^=1u;
    CHECK(stn_issuance_bind_block(&changed,&block,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.reason=STN_ISSUANCE_REASON_SHARE;
    CHECK(stn_issuance_bind_block(&changed,&block,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.units=STN_ISSUANCE_SHARE_UNITS;
    CHECK(stn_issuance_bind_block(&changed,&block,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.destination.mining_identity.identifier[0]^=1u;
    CHECK(stn_issuance_bind_block(&changed,&block,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.destination.wallet.identifier[0]^=1u;
    CHECK(stn_issuance_bind_block(&changed,&block,&state)==STN_DATA_CONTENT);
    changed=issuance;changed.destination.wallet.type=STN_ADDRESS_IDENTITY;
    CHECK(stn_issuance_bind_block(&changed,&block,&state)==STN_DATA_TYPE);
    CHECK(stn_issuance_bind_block(NULL,&block,&state)==STN_DATA_ARGUMENT);
    CHECK(stn_issuance_bind_block(&issuance,NULL,&state)==STN_DATA_ARGUMENT);
    CHECK(stn_issuance_bind_block(&issuance,&block,NULL)==STN_DATA_ARGUMENT);
}

int test_issuance_binding(void);
int test_issuance_binding(void)
{
    vectors();
    printf("Issuance binding: %u checks, %u failures.\n",checks,failures);
    return failures==0?0:1;
}
#ifdef STN_ISSUANCE_BINDING_TEST_MAIN
int main(void){return test_issuance_binding();}
#endif
