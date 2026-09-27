/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_share_pending.h"
#include "stn_share.h"
#include "stn_issuance.h"
#include "stn_transaction.h"
#include <assert.h>
#include <string.h>

static stn_data_status fake_hash(void *user,const uint8_t *domain,size_t domain_length,
    const uint8_t *bytes,size_t length,uint8_t out[32])
{
    size_t i;(void)user;(void)domain;(void)domain_length;
    memset(out,0,32u);
    for(i=0u;i<length;++i){out[i%32u]^=bytes[i];}
    out[31]^=(uint8_t)length;
    return STN_DATA_OK;
}

static size_t encode_tx(uint16_t type,const uint8_t *record,size_t record_length,
    uint8_t *out,size_t capacity)
{
    stn_transaction tx={0};size_t written=0u;
    tx.version=1u;tx.type=type;tx.record_bytes=record;tx.record_length=record_length;
    assert(stn_transaction_encode(&tx,out,capacity,&written)==STN_DATA_OK);
    return written;
}

int main(void)
{
    stn_pending pool;stn_hash_provider hash={0};
    stn_share_evidence share={0};stn_issuance_record issuance={0};
    uint8_t share_record[STN_SHARE_CANONICAL_SIZE];
    uint8_t issuance_record[STN_ISSUANCE_CANONICAL_SIZE];
    uint8_t share_tx[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
    uint8_t issuance_tx[STN_TX_HEADER_SIZE+STN_ISSUANCE_CANONICAL_SIZE];
    uint8_t share_id[32],issuance_id[32],dummy_id[32];
    size_t share_len,issuance_len,i;

    hash.hash=fake_hash;
    share.miner.type=STN_ADDRESS_IDENTITY;
    share.miner.identifier[0]=1u;
    share.nonce=7u;
    share.work_id[0]=2u;
    assert(stn_share_encode(&share,share_record)==STN_DATA_OK);

    issuance.reason=STN_ISSUANCE_REASON_SHARE;
    issuance.units=STN_ISSUANCE_SHARE_UNITS;
    issuance.evidence_id[0]=3u;
    issuance.destination.mining_identity=share.miner;
    issuance.destination.wallet.type=STN_ADDRESS_WALLET;
    issuance.destination.wallet.identifier[0]=4u;
    assert(stn_issuance_encode(&issuance,issuance_record)==STN_DATA_OK);

    share_len=encode_tx(STN_TX_SHARE_EVIDENCE,share_record,sizeof(share_record),share_tx,sizeof(share_tx));
    issuance_len=encode_tx(STN_TX_ISSUANCE,issuance_record,sizeof(issuance_record),issuance_tx,sizeof(issuance_tx));

    stn_pending_init(&pool);
    assert(stn_share_pending_admit_pair(&pool,share_tx,share_len,issuance_tx,issuance_len,
        &hash,share_id,issuance_id)==STN_PENDING_ACCEPTED);
    assert(pool.count==2u);
    stn_pending_clear(&pool);

    /* Leave only one slot. Atomic admission must refuse without retaining the
     * share half of the pair. */
    stn_pending_init(&pool);
    for(i=0u;i<STN_PENDING_MAX_ENTRIES-1u;++i){
        uint8_t record[STN_SHARE_CANONICAL_SIZE];uint8_t tx[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
        size_t n;share.nonce=100u+i;assert(stn_share_encode(&share,record)==STN_DATA_OK);
        n=encode_tx(STN_TX_SHARE_EVIDENCE,record,sizeof(record),tx,sizeof(tx));
        assert(stn_pending_insert(&pool,tx,n,&hash,dummy_id)==STN_PENDING_ACCEPTED);
    }
    assert(pool.count==STN_PENDING_MAX_ENTRIES-1u);
    assert(stn_share_pending_admit_pair(&pool,share_tx,share_len,issuance_tx,issuance_len,
        &hash,share_id,issuance_id)==STN_PENDING_CAPACITY);
    assert(pool.count==STN_PENDING_MAX_ENTRIES-1u);
    stn_pending_clear(&pool);
    return 0;
}
