/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pending.h"
#include "stn_block.h"
#include "stn_share.h"
#include "stn_compensation.h"
#include "stn_compensation_state.h"
#include "stn_issuance.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

#define CHECK(expr) do { \
    ++checks; \
    if(!(expr)){ \
        ++failures; \
        fprintf(stderr,"pending economy line %d: %s\n",__LINE__,#expr); \
    } \
} while(0)

static stn_data_status build_share(
    const stn_address *miner,
    stn_share_evidence *share)
{
    stn_block_header header={0};
    size_t written=0u;
    size_t i;

    if(miner==NULL || share==NULL)return STN_DATA_ARGUMENT;

    memset(share,0,sizeof(*share));
    header.version=STN_POW_BLOCK_VERSION;
    header.height=1u;
    header.timestamp=1u;
    memset(header.reserved_target,0xff,sizeof(header.reserved_target));
    for(i=0u;i<sizeof(header.network_id);++i)header.network_id[i]=(uint8_t)(i+1u);
    for(i=0u;i<sizeof(header.previous_hash);++i)header.previous_hash[i]=(uint8_t)(0x80u+i);

    if(stn_block_header_encode(&header,share->template_header,
        sizeof(share->template_header),&written)!=STN_DATA_OK ||
       written!=STN_SHARE_TEMPLATE_HEADER_SIZE)return STN_DATA_CONTENT;

    for(i=0u;i<sizeof(share->work_id);++i)share->work_id[i]=(uint8_t)(0x40u+i);
    share->miner=*miner;
    share->nonce=7u;
    memcpy(share->body_commitment,header.transaction_commitment,
        sizeof(share->body_commitment));
    return STN_DATA_OK;
}

static stn_data_status encode_transaction(
    uint16_t type,
    const uint8_t *record,
    size_t record_length,
    uint8_t *output,
    size_t capacity,
    size_t *written)
{
    stn_transaction tx;
    tx.version=1u;
    tx.type=type;
    tx.record_bytes=record;
    tx.record_length=(uint32_t)record_length;
    return stn_transaction_encode(&tx,output,capacity,written);
}

int main(void)
{
    stn_hash_provider hash={stn_sha256,NULL};
    stn_pending pool={0},empty={0},mapping_pool={0};
    stn_storage_view active={0},mapping_active={0};
    stn_validation_report report;
    stn_compensation_destination mapping={0};
    stn_compensation_destination compensation_storage[1];
    stn_compensation_destination empty_storage[1];
    stn_compensation_state compensation={0},empty_compensation={0};
    stn_share_evidence share={0};
    stn_issuance_record issuance={0};
    uint8_t share_record[STN_SHARE_CANONICAL_SIZE];
    uint8_t share_tx[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
    uint8_t issuance_record[STN_ISSUANCE_CANONICAL_SIZE];
    uint8_t issuance_tx[STN_TX_HEADER_SIZE+STN_ISSUANCE_CANONICAL_SIZE];
    uint8_t mapping_record[STN_COMPENSATION_DESTINATION_SIZE];
    uint8_t mapping_tx[STN_TX_HEADER_SIZE+STN_COMPENSATION_DESTINATION_SIZE];
    uint8_t id[32];
    size_t share_tx_length=0u,issuance_tx_length=0u,mapping_tx_length=0u;
    size_t i;

    mapping.mining_identity.type=STN_ADDRESS_IDENTITY;
    mapping.wallet.type=STN_ADDRESS_WALLET;
    for(i=0u;i<STN_ADDRESS_ID_SIZE;++i){
        mapping.mining_identity.identifier[i]=(uint8_t)(0x10u+i);
        mapping.wallet.identifier[i]=(uint8_t)(0xa0u+i);
    }

    CHECK(stn_compensation_state_initialize(&compensation,
        compensation_storage,1u)==STN_DATA_OK);
    CHECK(stn_compensation_state_apply(&compensation,&mapping)==STN_DATA_OK);
    active.state.compensation=&compensation;

    CHECK(build_share(&mapping.mining_identity,&share)==STN_DATA_OK);
    CHECK(stn_share_encode(&share,share_record)==STN_DATA_OK);
    CHECK(encode_transaction(STN_TX_SHARE_EVIDENCE,share_record,sizeof(share_record),
        share_tx,sizeof(share_tx),&share_tx_length)==STN_DATA_OK);
    CHECK(stn_pending_insert(&pool,share_tx,share_tx_length,&hash,id)==STN_PENDING_ACCEPTED);

    issuance.reason=STN_ISSUANCE_REASON_SHARE;
    issuance.units=STN_ISSUANCE_SHARE_UNITS;
    issuance.destination=mapping;
    CHECK(stn_share_id(&share,issuance.evidence_id)==STN_DATA_OK);
    CHECK(stn_issuance_encode(&issuance,issuance_record)==STN_DATA_OK);
    CHECK(encode_transaction(STN_TX_ISSUANCE,issuance_record,sizeof(issuance_record),
        issuance_tx,sizeof(issuance_tx),&issuance_tx_length)==STN_DATA_OK);

    CHECK(stn_pending_admit_transaction(&pool,issuance_tx,issuance_tx_length,
        NULL,&active,&hash,&report,id)==STN_PENDING_ACCEPTED);
    CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(pool.count==2u);
    CHECK(stn_pending_admit_transaction(&pool,issuance_tx,issuance_tx_length,
        NULL,&active,&hash,&report,id)==STN_PENDING_DUPLICATE);

    CHECK(stn_pending_admit_transaction(&empty,issuance_tx,issuance_tx_length,
        NULL,&active,&hash,&report,id)==STN_PENDING_UNAVAILABLE);
    CHECK(empty.count==0u);

    CHECK(stn_compensation_state_initialize(&empty_compensation,
        empty_storage,1u)==STN_DATA_OK);
    mapping_active.state.compensation=&empty_compensation;
    CHECK(stn_compensation_destination_encode(&mapping,mapping_record)==STN_DATA_OK);
    CHECK(encode_transaction(STN_TX_COMPENSATION_DESTINATION,
        mapping_record,sizeof(mapping_record),mapping_tx,sizeof(mapping_tx),
        &mapping_tx_length)==STN_DATA_OK);
    CHECK(stn_pending_admit_transaction(&mapping_pool,mapping_tx,mapping_tx_length,
        NULL,&mapping_active,&hash,&report,id)==STN_PENDING_ACCEPTED);
    CHECK(mapping_pool.count==1u);

    stn_pending_clear(&pool);
    stn_pending_clear(&empty);
    stn_pending_clear(&mapping_pool);

    printf("Pending economy: %u checks, %u failures.\n",checks,failures);
    return failures==0u ? 0 : 1;
}
