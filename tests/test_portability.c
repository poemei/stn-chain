/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "../platforms/stn_build_config.h"
#include "stn_peer.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"portability line %d: %s\n",__LINE__,#e);} } while(0)
static void fields(stn_record *r)
{
    r->version=1;r->type=1;memset(r->network_id,0,32);memset(r->signer_public_key,0,32);
    memset(r->nonce,0,32);r->nonce[0]=1;r->issued_at=UINT64_C(0x0102030405060708);
    r->payload=NULL;r->payload_length=0;memset(r->signature,0,64);
}
int test_portability(void);
int test_portability(void)
{
    const uint8_t endian[]={1,2,3,4,5,6,7,8};
    uint8_t x[180],y[180],unaligned[200],tx1[192],tx2[192],id1[32],id2[32];
    stn_record a,b,decoded;stn_transaction t1,t2;stn_hash_provider hash={stn_sha256,NULL};
    stn_block_header h={0},again;uint8_t header[168],header_copy[176];
    stn_work work;uint8_t target[32]={0};stn_peer_message message;size_t n,m,i;
    stn_peer_candidates candidates={0},decoded_candidates={0};
    stn_peer_endpoint endpoint={{127,0,0,1},19000};
    uint8_t discovery[STN_PEER_DISCOVERY_MAX];size_t discovery_length;
    CHECK(STN_HOST_OS>=STN_OS_WINDOWS && STN_HOST_OS<=STN_OS_MACOS);
    CHECK(STN_HOST_ARCH>=STN_ARCH_X86 && STN_HOST_ARCH<=STN_ARCH_ARM64);
    CHECK(STN_PEER_MAX_FRAME<=UINT32_MAX);
    /* Set every field independently, keeping distinct object padding bytes. */
    memset(&a,0x55,sizeof(a));memset(&b,0xaa,sizeof(b));fields(&a);fields(&b);
    CHECK(stn_record_encode(&a,x,sizeof(x),&n)==STN_RECORD_OK && n==180);
    CHECK(stn_record_encode(&b,y,sizeof(y),&m)==STN_RECORD_OK && m==n && memcmp(x,y,n)==0);
    CHECK(memcmp(x,"STNR\0\1\0\1",8)==0 && memcmp(x+104,endian,8)==0);
    for(i=0;i<8;++i){
        memcpy(unaligned+i,x,180);
        CHECK(stn_record_decode(unaligned+i,180,&decoded)==STN_RECORD_OK);
        CHECK(decoded.issued_at==UINT64_C(0x0102030405060708) && decoded.payload_length==0);
        CHECK(stn_record_encode(&decoded,y,sizeof(y),&m)==STN_RECORD_OK && memcmp(x,y,180)==0);
    }
    memset(&t1,0x33,sizeof(t1));memset(&t2,0xcc,sizeof(t2));
    t1.version=1;t1.type=1;t1.record_bytes=x;t1.record_length=180;
    t2.version=1;t2.type=1;t2.record_bytes=y;t2.record_length=180;
    CHECK(stn_transaction_encode(&t1,tx1,sizeof(tx1),&n)==STN_DATA_OK);
    CHECK(stn_transaction_encode(&t2,tx2,sizeof(tx2),&m)==STN_DATA_OK && n==m && memcmp(tx1,tx2,n)==0);
    CHECK(stn_transaction_id(tx1,n,&hash,id1)==STN_DATA_OK);
    CHECK(stn_transaction_id(tx2,m,&hash,id2)==STN_DATA_OK && memcmp(id1,id2,32)==0);
    /* Header integer fields are byte-order defined, independent of object size. */
    h.version=3;h.network_id[0]=1;h.previous_hash[0]=1;h.height=UINT64_C(0x0102030405060708);
    h.timestamp=UINT64_C(0xf1e2d3c4b5a69788);h.reserved_work_nonce=UINT64_C(0x0102030405060708);
    memset(h.reserved_target,255,32);h.reserved_target[0]=127;h.transaction_count=1;h.body_length=196;
    CHECK(stn_block_header_encode(&h,header,168,&n)==STN_DATA_OK && n==168);
    CHECK(memcmp(header+72,endian,8)==0 && memcmp(header+152,endian,8)==0);
    CHECK(memcmp(header+80,"\xf1\xe2\xd3\xc4\xb5\xa6\x97\x88",8)==0);
    for(i=0;i<8;++i){
        memcpy(header_copy+i,header,168);
        CHECK(stn_block_header_decode(header_copy+i,168,&again)==STN_DATA_OK);
        CHECK(again.height==h.height && again.timestamp==h.timestamp && again.reserved_work_nonce==h.reserved_work_nonce);
    }
    CHECK(stn_record_decode(x,SIZE_MAX,&decoded)==STN_RECORD_LIMIT);
    CHECK(stn_transaction_validate_structure(tx1,(size_t)UINT32_MAX)==STN_DATA_LENGTH);
    CHECK(stn_block_header_validate_structure(header,SIZE_MAX)==STN_DATA_LENGTH);
    CHECK(stn_block_body_validate_structure(tx1,192,UINT32_MAX)==STN_DATA_LENGTH);
    /* Truncation to 32 bits cannot turn the bound into a valid length/count. */
    memset(tx1+8,255,4);CHECK(stn_transaction_validate_structure(tx1,192)==STN_DATA_LENGTH);
    CHECK(stn_peer_decode(header_copy,SIZE_MAX,&message)!=STN_PEER_OK);
    /* Phase 12 Block 4 portability contract: candidate/discovery/orchestration
     * values are fixed-width ISO C data and remain independent of OS handles. */
    CHECK(stn_peer_candidate_add(&candidates,&endpoint)==STN_PEER_OK);
    endpoint.port=19001;CHECK(stn_peer_candidate_add(&candidates,&endpoint)==STN_PEER_OK);
    CHECK(stn_peer_discovery_encode(&candidates,NULL,discovery,sizeof(discovery),&discovery_length)==STN_PEER_OK);
    CHECK(discovery_length==14 && discovery[0]==0 && discovery[1]==2);
    CHECK(stn_peer_discovery_admit(&decoded_candidates,NULL,discovery,discovery_length)==STN_PEER_OK);
    CHECK(decoded_candidates.count==2 && decoded_candidates.entries[0].port==19000 && decoded_candidates.entries[1].port==19001);
    CHECK(STN_PEER_OUTBOUND_INTERVAL_MS==UINT64_C(5000));
    target[31]=1;CHECK(stn_target_work(target,&work)==STN_DATA_OK && work.bytes[8]==128);
    target[31]=2;CHECK(stn_target_work(target,&work)==STN_DATA_OK);
    for(i=0;i<32;++i){CHECK(work.bytes[i+8]==0x55);}
    printf("Portability/invariance: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}
