/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_sha256.h"
#include "stn_chain.h"
#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"pow line %d: %s\n",__LINE__,#e);} } while(0)
static unsigned nibble(char c){return c>='a' ? (unsigned)(c-'a'+10) : (unsigned)(c-'0');}
static void unhex(const char *s,uint8_t out[32]){size_t i;for(i=0;i<32;++i){out[i]=(uint8_t)(16*nibble(s[2*i])+nibble(s[2*i+1]));}}
static int equals_hex(const uint8_t hash[32],const char *s){uint8_t expected[32];unhex(s,expected);return memcmp(hash,expected,32)==0;}
static void easiest(uint8_t target[32]){memset(target,255,32);target[0]=127;}

/* Independent wire fixture from specified byte offsets, no encoder. Expected
 * IDs cross-checked with .NET SHA256, separately from CNG adapter under test. */
static void fixture(uint8_t b[364])
{
    memset(b,0,364);
    b[0]=0x53;b[1]=0x54;b[2]=0x4e;b[3]=0x42;b[5]=3;b[8]=1;
    b[163]=1;b[167]=196;b[171]=192;
    b[172]=0x53;b[173]=0x54;b[174]=0x4e;b[175]=0x54;b[177]=1;b[179]=1;b[183]=180;
    b[184]=0x53;b[185]=0x54;b[186]=0x4e;b[187]=0x52;b[189]=1;b[191]=1;b[192]=1;b[256]=3;
    unhex("ecf91bfa6a4e06d86a24d636eed25f01cf302949e85351d437e04c4e49219a76",b+88);
    easiest(b+120);
}

static void hash_vectors(void)
{
    static uint8_t million[1000000];
    const char *longmsg="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t d[32],again[32],block[364],roundtrip[364];
    stn_hash_provider p={stn_sha256,NULL};stn_block b;size_t n;
    CHECK(stn_sha256(NULL,NULL,0,NULL,0,d)==STN_DATA_OK);
    CHECK(equals_hex(d,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK(stn_sha256(NULL,NULL,0,(const uint8_t *)"abc",3,d)==STN_DATA_OK);
    CHECK(equals_hex(d,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(stn_sha256(NULL,(const uint8_t *)"a",1,(const uint8_t *)"bc",2,again)==STN_DATA_OK && memcmp(d,again,32)==0);
    CHECK(stn_sha256(NULL,NULL,0,(const uint8_t *)longmsg,strlen(longmsg),d)==STN_DATA_OK);
    CHECK(equals_hex(d,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
    memset(million,'a',sizeof(million));
    CHECK(stn_sha256(NULL,NULL,0,million,sizeof(million),d)==STN_DATA_OK);
    CHECK(equals_hex(d,"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
    memcpy(again,d,32);
    CHECK(stn_sha256(NULL,NULL,1,NULL,0,d)==STN_DATA_ARGUMENT && memcmp(d,again,32)==0);
    CHECK(stn_sha256(NULL,NULL,0,NULL,1,d)==STN_DATA_ARGUMENT && memcmp(d,again,32)==0);
    CHECK(stn_sha256(NULL,NULL,0,NULL,0,NULL)==STN_DATA_ARGUMENT);
    fixture(block);
    CHECK(stn_transaction_id(block+172,192,&p,d)==STN_DATA_OK);
    CHECK(equals_hex(d,"523e35d5006fe26eb9308e53d08455036a2ba3c5366f9bc2eb307161846022f1"));
    CHECK(stn_chain_block_id(block,364,&p,d)==STN_DATA_OK);
    CHECK(equals_hex(d,"368d0a851ef34ec12ed69475830f4728cb94d47c6e9b953254bb3591c15e2825"));
    CHECK(stn_block_decode(block,364,&b)==STN_DATA_OK);
    CHECK(stn_block_encode(&b,roundtrip,sizeof(roundtrip),&n)==STN_DATA_OK && n==364);
    CHECK(stn_chain_block_id(roundtrip,n,&p,again)==STN_DATA_OK && memcmp(d,again,32)==0);
    CHECK(stn_pow_verify(block,364,&p,again)==STN_DATA_OK && memcmp(d,again,32)==0);
    block[159]=1;
    CHECK(stn_chain_block_id(block,364,&p,again)==STN_DATA_OK && memcmp(d,again,32)!=0);
    block[363]=1;
    CHECK(stn_transaction_id(block+172,192,&p,d)==STN_DATA_OK);
    CHECK(!equals_hex(d,"523e35d5006fe26eb9308e53d08455036a2ba3c5366f9bc2eb307161846022f1"));
}

static void targets_and_work(void)
{
    uint8_t t[33]={0},hash[32]; stn_work w,expected={{0}},saved,sum;unsigned k;
    easiest(t);CHECK(stn_target_validate(t,32)==STN_DATA_OK);
    CHECK(stn_target_work(t,&w)==STN_DATA_OK && w.bytes[31]==2);
    expected.bytes[31]=2;CHECK(memcmp(w.bytes,expected.bytes,32)==0);
    CHECK(stn_work_at_height(t,2,&sum)==STN_DATA_OK && sum.bytes[31]==6);
    CHECK(stn_work_at_height(t,UINT64_MAX,&sum)==STN_DATA_OK && sum.bytes[23]==2 && sum.bytes[31]==0);
    memcpy(hash,t,32);CHECK(stn_pow_compare(hash,t)==STN_DATA_OK);
    --hash[31];CHECK(stn_pow_compare(hash,t)==STN_DATA_OK);
    hash[0]=128;CHECK(stn_pow_compare(hash,t)==STN_DATA_WORK);
    CHECK(stn_target_validate(t,31)==STN_DATA_LENGTH);
    CHECK(stn_target_validate(t,33)==STN_DATA_LENGTH);
    t[0]=128;CHECK(stn_target_validate(t,32)==STN_DATA_TARGET);
    memset(t,255,32);CHECK(stn_target_validate(t,32)==STN_DATA_TARGET);
    memset(t,0,32);saved=w;
    CHECK(stn_target_work(t,&w)==STN_DATA_TARGET && memcmp(w.bytes,saved.bytes,32)==0);
    t[31]=1;CHECK(stn_target_work(t,&w)==STN_DATA_OK && w.bytes[0]==128);
    saved=w;CHECK(stn_work_add(&w,&w,&w)==STN_DATA_OVERFLOW && memcmp(w.bytes,saved.bytes,32)==0);
    CHECK(stn_work_at_height(t,1,&w)==STN_DATA_OVERFLOW && memcmp(w.bytes,saved.bytes,32)==0);
    t[31]=2;CHECK(stn_target_work(t,&w)==STN_DATA_OK);
    memset(expected.bytes,0x55,32);CHECK(memcmp(w.bytes,expected.bytes,32)==0);
    /* Independent exact powers: T=2^k-1 => W=2^(256-k). */
    for(k=1;k<=255;++k){
        unsigned bit;memset(t,0,32);memset(expected.bytes,0,32);
        for(bit=0;bit<k;++bit){t[31-bit/8]|=(uint8_t)(1u<<(bit%8));}
        bit=256-k;expected.bytes[31-bit/8]=(uint8_t)(1u<<(bit%8));
        CHECK(stn_target_work(t,&w)==STN_DATA_OK && memcmp(w.bytes,expected.bytes,32)==0);
    }
    CHECK(stn_target_validate(NULL,32)==STN_DATA_ARGUMENT);
    CHECK(stn_target_work(t,NULL)==STN_DATA_ARGUMENT);
    CHECK(stn_work_add(NULL,&w,&sum)==STN_DATA_ARGUMENT);
}

typedef struct fake { stn_data_status status; uint8_t id[32]; } fake;
static stn_data_status fake_provider(void *user,const uint8_t *domain,size_t dn,
    const uint8_t *bytes,size_t n,uint8_t digest[32])
{
    fake *f=user;
    if(f->status!=STN_DATA_OK){memset(digest,0xff,32);return f->status;}
    if(dn==sizeof("STN-CHAIN:BLOCK:ID:1") && memcmp(domain,"STN-CHAIN:BLOCK:ID:1",dn)==0){memcpy(digest,f->id,32);return STN_DATA_OK;}
    return stn_sha256(NULL,domain,dn,bytes,n,digest);
}
static int same(const stn_chain_state *a,const stn_chain_state *b)
{
    return a->height==b->height && a->timestamp==b->timestamp && a->has_tip==b->has_tip &&
    memcmp(a->tip_id,b->tip_id,32)==0 && memcmp(a->genesis_id,b->genesis_id,32)==0 &&
    memcmp(a->network_id,b->network_id,32)==0 && memcmp(a->current_target,b->current_target,32)==0 &&
    memcmp(a->cumulative_work.bytes,b->cumulative_work.bytes,32)==0;
}
static void pow_chain(void)
{
    uint8_t genesis[364],child[364],digest[32],saved_digest[32];
    stn_chain_context c={0};stn_pow_policy policy;stn_chain_state empty,s,next,before,full;
    stn_chain_report r;stn_block_span spans[2];
    fake f={STN_DATA_OK,{0}};stn_hash_provider fp={fake_provider,&f};
    fixture(genesis);easiest(policy.fixed_target);
    c.network_id[0]=1;c.genesis_bytes=genesis;c.genesis_length=364;c.hash_provider.hash=stn_sha256;c.pow_policy=&policy;
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK);
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&s);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && r.pow==STN_STAGE_PASS && r.target==STN_STAGE_PASS && r.work==STN_STAGE_PASS);
    CHECK(s.cumulative_work.bytes[31]==2 && memcmp(s.current_target,policy.fixed_target,32)==0);
    memcpy(child,genesis,364);memcpy(child+40,s.tip_id,32);child[79]=1;
    r=stn_chain_validate_candidate(&c,&s,child,364,&next);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && next.cumulative_work.bytes[31]==4);
    CHECK(equals_hex(next.tip_id,"35eb9fa251a3eeca9412b3c23159fbdbf26d874fa9af8969341d2ef67d8c8726"));
    spans[0].bytes=genesis;spans[0].length=364;spans[1].bytes=child;spans[1].length=364;
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&full);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && same(&full,&next));
    before=next;child[151]--;
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&next);
    CHECK(r.reason==STN_CHAIN_TARGET && r.failing_index==1 && same(&before,&next));
    child[151]++;s.cumulative_work.bytes[31]=3;
    r=stn_chain_validate_candidate(&c,&s,child,364,&next);
    CHECK(r.reason==STN_CHAIN_STATE && same(&before,&next));s.cumulative_work.bytes[31]=2;
    memset(digest,0xa5,32);memcpy(saved_digest,digest,32);
    CHECK(stn_pow_verify(genesis,364,NULL,digest)==STN_DATA_UNRESOLVED && memcmp(digest,saved_digest,32)==0);
    f.status=STN_DATA_PROVIDER_ERROR;
    CHECK(stn_pow_verify(genesis,364,&fp,digest)==STN_DATA_PROVIDER_ERROR && memcmp(digest,saved_digest,32)==0);
    f.status=STN_DATA_CONTENT;CHECK(stn_pow_verify(genesis,364,&fp,digest)==STN_DATA_PROVIDER_ERROR);
    f.status=STN_DATA_OK;memcpy(f.id,policy.fixed_target,32);
    CHECK(stn_pow_verify(genesis,364,&fp,digest)==STN_DATA_OK);
    --f.id[31];CHECK(stn_pow_verify(genesis,364,&fp,digest)==STN_DATA_OK);
    f.id[0]=128;memcpy(saved_digest,digest,32);
    CHECK(stn_pow_verify(genesis,364,&fp,digest)==STN_DATA_WORK && memcmp(digest,saved_digest,32)==0);
    /* Exact hardest-target fixture under a declared fake block-ID provider. */
    memset(policy.fixed_target,0,32);policy.fixed_target[31]=1;
    memcpy(genesis+120,policy.fixed_target,32);memset(f.id,0,32);f.id[31]=1;c.hash_provider=fp;
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK);
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&s);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && s.cumulative_work.bytes[0]==128);
    memcpy(child,genesis,364);memcpy(child+40,s.tip_id,32);child[79]=1;next=s;before=s;
    r=stn_chain_validate_candidate(&c,&s,child,364,&next);
    CHECK(r.reason==STN_CHAIN_WORK && r.detail==STN_DATA_OVERFLOW && same(&next,&before));
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&next);
    CHECK(r.reason==STN_CHAIN_WORK && r.failing_index==1 && same(&next,&before));
    f.id[31]=2;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&next);
    CHECK(r.reason==STN_CHAIN_POW && same(&next,&before));
    f.status=STN_DATA_UNRESOLVED;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&next);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNRESOLVED && same(&next,&before));
    f.status=STN_DATA_PROVIDER_ERROR;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&next);
    CHECK(r.acceptance==STN_ACCEPTANCE_ERROR && same(&next,&before));
    memset(genesis+120,0,32);CHECK(stn_block_validate_structure(genesis,364)==STN_DATA_TARGET);
    genesis[5]=1;c.pow_policy=&policy;
    CHECK(stn_chain_initialize(&c,&next)!=STN_DATA_OK && same(&next,&before));
    fixture(genesis);c.pow_policy=NULL;
    CHECK(stn_chain_initialize(&c,&next)!=STN_DATA_OK && same(&next,&before));
}

int test_pow(void);
int test_pow(void)
{
    hash_vectors();targets_and_work();pow_chain();
    printf("SHA-256/PoW/work: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}
