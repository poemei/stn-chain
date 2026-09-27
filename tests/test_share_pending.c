/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share_pending.h"
#include "stn_share.h"
#include "stn_transaction.h"
#include <assert.h>
#include <string.h>

static stn_data_status fake_hash(void *user,const uint8_t *domain,size_t domain_length,
    const uint8_t *bytes,size_t length,uint8_t out[32])
{
    size_t i;(void)user;(void)domain;(void)domain_length;
    memset(out,0,32u);
    for(i=0u;i<length;++i)out[i%32u]^=bytes[i];
    out[31]^=(uint8_t)length;return STN_DATA_OK;
}

static size_t encode_share(stn_share_evidence *share,uint8_t *out,size_t capacity)
{
    stn_transaction tx={0};uint8_t record[STN_SHARE_CANONICAL_SIZE];size_t written=0u;
    assert(stn_share_encode(share,record)==STN_DATA_OK);
    tx.version=1u;tx.type=STN_TX_SHARE_EVIDENCE;tx.record_bytes=record;
    tx.record_length=sizeof(record);
    assert(stn_transaction_encode(&tx,out,capacity,&written)==STN_DATA_OK);
    return written;
}

int main(void)
{
    stn_pending pool;stn_hash_provider hash={0};stn_share_evidence share={0};
    stn_validation_report report;uint8_t id[32],dummy_id[32];size_t i,share_len;
    uint8_t share_tx[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];

    hash.hash=fake_hash;share.miner.type=STN_ADDRESS_IDENTITY;
    share.miner.identifier[0]=1u;share.work_id[0]=2u;share.nonce=7u;
    share_len=encode_share(&share,share_tx,sizeof(share_tx));

    stn_pending_init(&pool);
    for(i=0u;i<STN_PENDING_MAX_ENTRIES-1u;++i){
        uint8_t tx[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];size_t n;
        share.nonce=100u+i;n=encode_share(&share,tx,sizeof(tx));
        assert(stn_pending_insert(&pool,tx,n,&hash,dummy_id)==STN_PENDING_ACCEPTED);
    }
    assert(pool.count==STN_PENDING_MAX_ENTRIES-1u);

    /* One free slot is insufficient for evidence + deterministic issuance.
     * Refusal happens before mutation, so no orphan share can consume it. */
    assert(stn_share_pending_admit(&pool,share_tx,share_len,NULL,NULL,&hash,
        &report,id)==STN_PENDING_CAPACITY);
    assert(pool.count==STN_PENDING_MAX_ENTRIES-1u);

    stn_pending_clear(&pool);return 0;
}
