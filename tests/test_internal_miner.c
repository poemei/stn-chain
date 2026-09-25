/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_internal_miner.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"internal miner line %d: %s\n",__LINE__,#e);}}while(0)

static void make_template(uint8_t header_bytes[168],uint8_t work_id[32])
{
    static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
    stn_block_header h={0};
    size_t written=0u;
    h.version=STN_POW_BLOCK_VERSION;
    h.height=1u;
    h.timestamp=1u;
    h.previous_hash[31]=1u;
    /* Easy deterministic test target; production target is supplied by Chain. */
    h.reserved_target[0]=0x0fu;
    h.reserved_target[31]=0xffu;
    CHECK(stn_block_body_commitment(NULL,0u,0u,
        &(stn_hash_provider){stn_sha256,NULL},h.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_header_encode(&h,header_bytes,168u,&written)==STN_DATA_OK);
    CHECK(written==168u);
    CHECK(stn_sha256(NULL,work_domain,sizeof(work_domain),header_bytes,168u,work_id)==STN_DATA_OK);
}

static void vectors(void)
{
    stn_hash_provider provider={stn_sha256,NULL};
    stn_address miner={0},wrong={0};
    stn_share_evidence evidence={0},before;
    stn_internal_miner_result result=STN_INTERNAL_MINER_EXHAUSTED;
    uint8_t header[168],work_id[32],bad_work[32];
    uint64_t nonce=0u,before_nonce;
    unsigned attempts;

    make_template(header,work_id);
    miner.type=STN_ADDRESS_IDENTITY;
    memset(miner.identifier,0x11,32u);
    wrong=miner;wrong.type=STN_ADDRESS_WALLET;

    before=evidence;
    before_nonce=nonce;
    CHECK(stn_internal_miner_search(header,work_id,&miner,&nonce,0u,20u,
        &provider,&evidence,&result)==STN_DATA_OK);
    CHECK(nonce==before_nonce && result==STN_INTERNAL_MINER_IDLE);
    CHECK(memcmp(&evidence,&before,sizeof(evidence))==0);

    CHECK(stn_internal_miner_search(header,work_id,&wrong,&nonce,1u,20u,
        &provider,&evidence,&result)==STN_DATA_TYPE);
    CHECK(stn_internal_miner_search(header,work_id,&miner,&nonce,1u,0u,
        &provider,&evidence,&result)==STN_DATA_CONTENT);
    CHECK(stn_internal_miner_search(header,work_id,&miner,&nonce,1u,21u,
        &provider,&evidence,&result)==STN_DATA_CONTENT);

    memcpy(bad_work,work_id,32u);bad_work[0]^=1u;
    CHECK(stn_internal_miner_search(header,bad_work,&miner,&nonce,1u,20u,
        &provider,&evidence,&result)==STN_DATA_CONTENT);

    nonce=0u;
    for(attempts=0u;attempts<1000000u;++attempts){
        CHECK(stn_internal_miner_search(header,work_id,&miner,&nonce,64u,20u,
            &provider,&evidence,&result)==STN_DATA_OK);
        if(result==STN_INTERNAL_MINER_SHARE || result==STN_INTERNAL_MINER_BLOCK)break;
    }
    CHECK(attempts<1000000u);
    CHECK(evidence.miner.type==STN_ADDRESS_IDENTITY);
    CHECK(memcmp(evidence.miner.identifier,miner.identifier,32u)==0);
    CHECK(memcmp(evidence.work_id,work_id,32u)==0);
    CHECK(stn_share_verify_evidence(&evidence,&provider,bad_work)==STN_DATA_OK);

    {
        stn_block_header solved;
        CHECK(stn_block_header_decode(evidence.template_header,168u,&solved)==STN_DATA_OK);
        CHECK(memcmp(evidence.body_commitment,solved.transaction_commitment,32u)==0);
    }

    CHECK(stn_internal_miner_search(NULL,work_id,&miner,&nonce,1u,20u,
        &provider,&evidence,&result)==STN_DATA_ARGUMENT);
}


static void worker_validation(void)
{
    stn_internal_miner_worker worker={0};
    stn_internal_miner_result result=STN_INTERNAL_MINER_EXHAUSTED;
    stn_mining_service service={0};
    stn_chain_context chain={0};

    CHECK(stn_internal_miner_worker_step(NULL,&result)==STN_DATA_ARGUMENT);
    CHECK(stn_internal_miner_worker_step(&worker,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_internal_miner_worker_step(&worker,&result)==STN_DATA_ARGUMENT);

    worker.service=&service;
    service.chain=&chain;
    worker.miner.type=STN_ADDRESS_WALLET;
    worker.nonce_budget=STN_INTERNAL_MINER_DEFAULT_NONCE_BUDGET;
    worker.duty_permille=STN_INTERNAL_MINER_DEFAULT_DUTY_PERMILLE;
    CHECK(stn_internal_miner_worker_step(&worker,&result)==STN_DATA_TYPE);

    worker.miner.type=STN_ADDRESS_IDENTITY;
    worker.nonce_budget=0u;
    CHECK(stn_internal_miner_worker_step(&worker,&result)==STN_DATA_CONTENT);
    worker.nonce_budget=STN_INTERNAL_MINER_DEFAULT_NONCE_BUDGET;
    worker.duty_permille=STN_INTERNAL_MINER_MAX_DUTY_PERMILLE+1u;
    CHECK(stn_internal_miner_worker_step(&worker,&result)==STN_DATA_CONTENT);
}

int test_internal_miner(void)
{
    vectors();
    worker_validation();
    printf("Internal miner primitive: %u checks, %u failures.\n",checks,failures);
    return failures==0?0:1;
}
#ifdef STN_INTERNAL_MINER_TEST_MAIN
int main(void){return test_internal_miner();}
#endif
