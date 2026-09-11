/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_block.h"
#include "stn_intelligence.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; \
    fprintf(stderr, "chain data line %d: %s\n", __LINE__, #expr); } } while (0)

/* Independent canonical fixtures. Empty inner payload is structurally valid,
 * not a semantically valid intelligence report. No real signatures/hashes. */
static const uint8_t tx_fixture[192] = {
    [0]=0x53,[1]=0x54,[2]=0x4e,[3]=0x54,[5]=1,[7]=1,[11]=180,
    [12]=0x53,[13]=0x54,[14]=0x4e,[15]=0x52,[17]=1,[19]=1,[20]=1,[84]=3
};
static const uint8_t block_fixture[364] = {
    [0]=0x53,[1]=0x54,[2]=0x4e,[3]=0x42,[5]=1,[8]=1,[40]=0xaa,
    [79]=1,[87]=2,[88]=0xbb,[119]=0xcc,[163]=1,[167]=196,[171]=192,
    [172]=0x53,[173]=0x54,[174]=0x4e,[175]=0x54,[177]=1,[179]=1,[183]=180,
    [184]=0x53,[185]=0x54,[186]=0x4e,[187]=0x52,[189]=1,[191]=1,[192]=1,[256]=3
};

/* Deterministic NONCRYPTOGRAPHIC provider. Only tests interface plumbing. */
static stn_data_status hash_stub(void *user, const uint8_t *domain, size_t dn,
    const uint8_t *bytes, size_t n, uint8_t digest[32])
{
    size_t i;
    stn_data_status mode = *(stn_data_status *)user;
    CHECK((dn == sizeof("STN-CHAIN:TX:ID:1") &&
           memcmp(domain, "STN-CHAIN:TX:ID:1", dn) == 0) ||
          (dn == sizeof("STN-CHAIN:BLOCK:BODY:1") &&
           memcmp(domain, "STN-CHAIN:BLOCK:BODY:1", dn) == 0));
    memset(digest, 0, 32);
    if (mode != STN_DATA_OK) { digest[0] = 0xff; return mode; }
    for (i = 0; i < dn; ++i) { digest[i % 32] = (uint8_t)(digest[i % 32] * 33u + domain[i]); }
    for (i = 0; i < n; ++i) { digest[i % 32] = (uint8_t)(digest[i % 32] * 33u + bytes[i]); }
    return STN_DATA_OK;
}

static void fixtures(void)
{
    stn_transaction t = {0};
    stn_block b = {0};
    stn_block_header h;
    uint8_t output[365];
    size_t n, i;
    CHECK(stn_transaction_decode(tx_fixture, sizeof(tx_fixture), &t) == STN_DATA_OK);
    CHECK(t.version == 1 && t.type == 1 && t.record_length == 180 && t.record_bytes == tx_fixture + 12);
    memset(output, 0xcd, sizeof(output));
    CHECK(stn_transaction_encode(&t, output, 192, &n) == STN_DATA_OK);
    CHECK(n == 192 && memcmp(output, tx_fixture, 192) == 0 && output[192] == 0xcd);
    CHECK(stn_block_decode(block_fixture, sizeof(block_fixture), &b) == STN_DATA_OK);
    CHECK(b.header.height == 1 && b.header.timestamp == 2 && b.header.previous_hash[0] == 0xaa);
    CHECK(b.header.transaction_commitment[0] == 0xbb && b.header.transaction_commitment[31] == 0xcc);
    CHECK(b.body == block_fixture + 168 && b.header.transaction_count == 1 && b.header.body_length == 196);
    CHECK(stn_block_header_decode(block_fixture, 168, &h) == STN_DATA_OK);
    CHECK(stn_block_body_validate_structure(b.body, 196, 1) == STN_DATA_OK);
    CHECK(stn_block_encode(&b, output, 364, &n) == STN_DATA_OK);
    CHECK(n == 364 && memcmp(output, block_fixture, 364) == 0 && output[364] == 0xcd);
    for (i = 0; i < 192; ++i) { CHECK(stn_transaction_decode(tx_fixture, i, &t) != STN_DATA_OK); }
    CHECK(t.record_bytes == tx_fixture + 12 && t.record_length == 180);
    for (i = 0; i < 364; ++i) { CHECK(stn_block_decode(block_fixture, i, &b) != STN_DATA_OK); }
    CHECK(b.body == block_fixture + 168 && b.header.previous_hash[0] == 0xaa);
    for (i = 0; i < 168; ++i) { CHECK(stn_block_header_decode(block_fixture, i, &h) != STN_DATA_OK); }
    CHECK(h.height == 1 && h.previous_hash[0] == 0xaa);
    for (i = 0; i < 196; ++i) { CHECK(stn_block_body_validate_structure(block_fixture + 168, i, 1) != STN_DATA_OK); }
}

static void malformed(void)
{
    uint8_t tx[193], block[365], out[365], saved[365];
    stn_transaction t;
    stn_block b;
    stn_block_header h;
    stn_transaction_span span = {tx_fixture, 192};
    size_t n = 123;
    unsigned kind;
    memcpy(tx, tx_fixture, 192); tx[192] = 0;
    CHECK(stn_transaction_validate_structure(tx, 193) == STN_DATA_LENGTH);
    tx[0]=0; CHECK(stn_transaction_validate_structure(tx, 192) == STN_DATA_MAGIC);
    memcpy(tx, tx_fixture, 192); tx[5]=2;
    CHECK(stn_transaction_validate_structure(tx, 192) == STN_DATA_VERSION);
    for (kind=0; kind<=6; ++kind) {
        memcpy(tx, tx_fixture, 192); tx[7]=(uint8_t)kind;
        CHECK(stn_transaction_validate_structure(tx, 192) == (kind==1 ? STN_DATA_OK : (kind>=2 && kind<=4 ? STN_DATA_LENGTH : STN_DATA_TYPE)));
    }
    memcpy(tx, tx_fixture, 192); memset(tx+8,0,4);
    CHECK(stn_transaction_validate_structure(tx, 192) == STN_DATA_LENGTH);
    memset(tx+8,0xff,4); CHECK(stn_transaction_validate_structure(tx, 192) == STN_DATA_LENGTH);
    memcpy(tx, tx_fixture, 192); tx[84]=0;
    CHECK(stn_transaction_validate_structure(tx, 192) == STN_DATA_CONTENT);
    memcpy(block, block_fixture, 364); block[364]=0;
    CHECK(stn_block_validate_structure(block,365) == STN_DATA_LENGTH);
    block[0]=0; CHECK(stn_block_validate_structure(block,364) == STN_DATA_MAGIC);
    memcpy(block,block_fixture,364); block[5]=2;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_VERSION);
    memcpy(block,block_fixture,364); block[7]=1;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_CONTENT);
    memcpy(block,block_fixture,364); block[120]=1;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_CONTENT);
    memcpy(block,block_fixture,364); block[159]=1;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_CONTENT);
    memcpy(block,block_fixture,364); block[40]=0;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_CONTENT);
    block[79]=0; CHECK(stn_block_validate_structure(block,364) == STN_DATA_OK); /* fixed dev genesis shape */
    block[40]=1; CHECK(stn_block_validate_structure(block,364) == STN_DATA_CONTENT);
    memcpy(block,block_fixture,364); block[163]=0;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_LENGTH);
    block[163]=17; CHECK(stn_block_validate_structure(block,364) == STN_DATA_LENGTH);
    block[163]=2; CHECK(stn_block_validate_structure(block,364) == STN_DATA_LENGTH);
    memcpy(block,block_fixture,364); block[167]=197;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_LENGTH);
    memcpy(block,block_fixture,364); memset(block+168,0xff,4);
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_LENGTH);
    memcpy(block,block_fixture,364); block[172]=0;
    CHECK(stn_block_validate_structure(block,364) == STN_DATA_CONTENT);
    CHECK(stn_block_body_validate_structure(block_fixture+168,196,0) == STN_DATA_LENGTH);
    CHECK(stn_block_body_validate_structure(block_fixture+168,196,2) == STN_DATA_LENGTH);
    CHECK(stn_block_header_validate_structure(block_fixture,169) == STN_DATA_LENGTH);
    CHECK(stn_transaction_decode(tx_fixture,192,&t) == STN_DATA_OK);
    CHECK(stn_block_decode(block_fixture,364,&b) == STN_DATA_OK); h=b.header;
    memset(out,0xa5,sizeof(out)); memcpy(saved,out,sizeof(out));
    CHECK(stn_transaction_encode(&t,out,191,&n) == STN_DATA_CAPACITY);
    CHECK(n==0 && memcmp(out,saved,sizeof(out))==0);
    CHECK(stn_block_header_encode(&h,out,167,&n) == STN_DATA_CAPACITY);
    CHECK(n==0 && memcmp(out,saved,sizeof(out))==0);
    CHECK(stn_block_body_encode(&span,1,out,195,&n) == STN_DATA_CAPACITY);
    CHECK(n==0 && memcmp(out,saved,sizeof(out))==0);
    CHECK(stn_block_encode(&b,out,363,&n) == STN_DATA_CAPACITY);
    CHECK(n==0 && memcmp(out,saved,sizeof(out))==0);
    h.flags=1; CHECK(stn_block_header_encode(&h,out,sizeof(out),&n) == STN_DATA_CONTENT);
    CHECK(n==0 && memcmp(out,saved,sizeof(out))==0);
    CHECK(stn_transaction_decode(NULL,0,&t) == STN_DATA_ARGUMENT);
    CHECK(stn_transaction_decode(tx_fixture,192,NULL) == STN_DATA_ARGUMENT);
    CHECK(stn_transaction_encode(NULL,out,sizeof(out),&n) == STN_DATA_ARGUMENT);
    CHECK(stn_block_decode(NULL,0,&b) == STN_DATA_ARGUMENT);
    CHECK(stn_block_decode(block_fixture,364,NULL) == STN_DATA_ARGUMENT);
    CHECK(stn_block_header_decode(NULL,168,&h) == STN_DATA_ARGUMENT);
    CHECK(stn_block_body_validate_structure(NULL,196,1) == STN_DATA_ARGUMENT);
    CHECK(stn_block_body_encode(NULL,1,out,sizeof(out),&n) == STN_DATA_ARGUMENT);
    CHECK(stn_block_encode(NULL,out,sizeof(out),&n) == STN_DATA_ARGUMENT);
}

static void maximum(void)
{
    static uint8_t record[STN_RECORD_MAX_SIZE], tx[STN_TX_MAX_SIZE];
    static uint8_t body[STN_BLOCK_MAX_BODY], block[STN_BLOCK_MAX_SIZE+1];
    stn_transaction t={0}; stn_transaction decoded;
    stn_transaction_span spans[STN_BLOCK_MAX_TRANSACTIONS];
    stn_block b={0};
    size_t n, i, bn;
    memcpy(record,tx_fixture+12,180);
    record[112]=0; record[113]=1; record[114]=0; record[115]=0;
    t.version=1; t.type=1; t.record_bytes=record; t.record_length=STN_RECORD_MAX_SIZE;
    CHECK(stn_transaction_encode(&t,tx,sizeof(tx),&n) == STN_DATA_OK);
    CHECK(n==STN_TX_MAX_SIZE && stn_transaction_decode(tx,n,&decoded)==STN_DATA_OK);
    for (i=0;i<n;++i) { CHECK(stn_transaction_validate_structure(tx,i)!=STN_DATA_OK); }
    for (i=0;i<STN_BLOCK_MAX_TRANSACTIONS;++i) { spans[i].bytes=tx; spans[i].length=(uint32_t)n; }
    CHECK(stn_block_body_encode(spans,16,body,sizeof(body),&bn)==STN_DATA_OK);
    CHECK(bn==STN_BLOCK_MAX_BODY);
    b.header.version=1; b.header.height=UINT64_MAX; b.header.timestamp=UINT64_MAX;
    b.header.previous_hash[31]=1; b.header.transaction_count=16;
    b.header.body_length=(uint32_t)bn; b.body=body;
    CHECK(stn_block_encode(&b,block,STN_BLOCK_MAX_SIZE,&n)==STN_DATA_OK);
    CHECK(n==STN_BLOCK_MAX_SIZE && stn_block_validate_structure(block,n)==STN_DATA_OK);
    for (i=0;i<n;++i) { CHECK(stn_block_validate_structure(block,i)!=STN_DATA_OK); }
    CHECK(stn_block_validate_structure(block,sizeof(block))==STN_DATA_LENGTH);
    CHECK(stn_block_body_validate_structure(body,bn,15)==STN_DATA_LENGTH);
    /* Duplicate transactions remain structural; integrity check is separate. */
}

static void hashing(void)
{
    stn_data_status mode=STN_DATA_OK;
    stn_hash_provider provider={hash_stub,&mode};
    uint8_t a[32], b[32], before[32], tx[192], body[392], block[560];
    stn_block value;
    stn_transaction_span spans[2]={{tx_fixture,192},{tx_fixture,192}};
    size_t n;
    memset(a,0xa5,32); memcpy(before,a,32);
    CHECK(stn_transaction_id(tx_fixture,192,NULL,a)==STN_DATA_UNRESOLVED);
    CHECK(memcmp(a,before,32)==0);
    mode=STN_DATA_PROVIDER_ERROR;
    CHECK(stn_transaction_id(tx_fixture,192,&provider,a)==STN_DATA_PROVIDER_ERROR && memcmp(a,before,32)==0);
    mode=STN_DATA_UNRESOLVED;
    CHECK(stn_transaction_id(tx_fixture,192,&provider,a)==STN_DATA_UNRESOLVED && memcmp(a,before,32)==0);
    mode=STN_DATA_CONTENT;
    CHECK(stn_transaction_id(tx_fixture,192,&provider,a)==STN_DATA_PROVIDER_ERROR);
    mode=STN_DATA_OK;
    CHECK(stn_transaction_id(tx_fixture,192,&provider,a)==STN_DATA_OK);
    CHECK(stn_transaction_id(tx_fixture,192,&provider,b)==STN_DATA_OK && memcmp(a,b,32)==0);
    memcpy(tx,tx_fixture,192); tx[191]=1; /* witness bytes participate in transaction ID */
    CHECK(stn_transaction_id(tx,192,&provider,b)==STN_DATA_OK && memcmp(a,b,32)!=0);
    CHECK(stn_block_decode(block_fixture,364,&value)==STN_DATA_OK);
    CHECK(stn_block_check_integrity(block_fixture,364,NULL)==STN_DATA_UNRESOLVED);
    CHECK(stn_block_check_integrity(block_fixture,364,&provider)==STN_DATA_COMMITMENT);
    CHECK(stn_block_body_commitment(value.body,196,1,&provider,value.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&value,block,sizeof(block),&n)==STN_DATA_OK);
    CHECK(stn_block_check_integrity(block,n,&provider)==STN_DATA_OK);
    CHECK(stn_block_body_encode(spans,2,body,sizeof(body),&n)==STN_DATA_OK);
    value.body=body; value.header.body_length=(uint32_t)n; value.header.transaction_count=2;
    CHECK(stn_block_body_commitment(body,n,2,&provider,value.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&value,block,sizeof(block),&n)==STN_DATA_OK);
    CHECK(stn_block_validate_structure(block,n)==STN_DATA_OK);
    CHECK(stn_block_check_integrity(block,n,&provider)==STN_DATA_DUPLICATE);
    spans[1].bytes=tx;
    CHECK(stn_block_body_encode(spans,2,body,sizeof(body),&n)==STN_DATA_OK);
    CHECK(stn_block_body_commitment(body,n,2,&provider,value.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&value,block,sizeof(block),&n)==STN_DATA_OK);
    CHECK(stn_block_check_integrity(block,n,&provider)==STN_DATA_OK);
    memcpy(before,a,32); mode=STN_DATA_PROVIDER_ERROR;
    CHECK(stn_block_body_commitment(body,392,2,&provider,a)==STN_DATA_PROVIDER_ERROR && memcmp(a,before,32)==0);
}

static void integration(void)
{
    stn_intelligence v={0}, decoded_payload;
    stn_record r={0}, decoded_record;
    stn_transaction t={0}, decoded_tx;
    stn_block block={0}, decoded_block;
    uint8_t payload[100], record[300], tx[320], body[324], wire[492];
    stn_transaction_span span;
    size_t n;
    v.version=1; v.severity=1; v.source=(const uint8_t *)"sensor.example"; v.source_length=14;
    v.classification=(const uint8_t *)"observation"; v.classification_length=11;
    v.subject=(const uint8_t *)"actor.example"; v.subject_length=13; v.evidence_digest[0]=1;
    CHECK(stn_intelligence_encode(&v,payload,sizeof(payload),&n)==STN_INTELLIGENCE_OK);
    r.version=1; r.type=1; r.nonce[0]=1; r.payload=payload; r.payload_length=(uint32_t)n;
    CHECK(stn_record_encode(&r,record,sizeof(record),&n)==STN_RECORD_OK);
    t.version=1; t.type=1; t.record_bytes=record; t.record_length=(uint32_t)n;
    CHECK(stn_transaction_encode(&t,tx,sizeof(tx),&n)==STN_DATA_OK);
    span.bytes=tx; span.length=(uint32_t)n;
    CHECK(stn_block_body_encode(&span,1,body,sizeof(body),&n)==STN_DATA_OK);
    block.header.version=1; block.header.transaction_count=1;
    block.header.body_length=(uint32_t)n; block.body=body;
    CHECK(stn_block_encode(&block,wire,sizeof(wire),&n)==STN_DATA_OK);
    CHECK(stn_block_decode(wire,n,&decoded_block)==STN_DATA_OK);
    CHECK(stn_transaction_decode(decoded_block.body+4,span.length,&decoded_tx)==STN_DATA_OK);
    CHECK(stn_record_decode(decoded_tx.record_bytes,decoded_tx.record_length,&decoded_record)==STN_RECORD_OK);
    CHECK(stn_intelligence_decode(decoded_record.payload,decoded_record.payload_length,&decoded_payload)==STN_INTELLIGENCE_OK);
    CHECK(decoded_payload.subject_length==13 && memcmp(decoded_payload.subject,"actor.example",13)==0);
}

int test_chain_data(void);
int test_chain_data(void)
{
    fixtures(); malformed(); maximum(); hashing(); integration();
    printf("Transaction/block data: %u checks, %u failures (test hashing only).\n",checks,failures);
    return failures==0 ? 0 : 1;
}
