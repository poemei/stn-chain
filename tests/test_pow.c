/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_sha256.h"
#include "stn_chain.h"
#include "stn_node_service.h"
#include "stn_peer.h"
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
    CHECK(stn_target_work(t,&w)==STN_DATA_OK && w.bytes[39]==2);
    expected.bytes[39]=2;CHECK(memcmp(w.bytes,expected.bytes,40)==0);
    CHECK(stn_work_at_height(t,2,&sum)==STN_DATA_OK && sum.bytes[39]==6);
    CHECK(stn_work_at_height(t,UINT64_MAX,&sum)==STN_DATA_OK && sum.bytes[31]==2 && sum.bytes[39]==0);
    memcpy(hash,t,32);CHECK(stn_pow_compare(hash,t)==STN_DATA_OK);
    --hash[31];CHECK(stn_pow_compare(hash,t)==STN_DATA_OK);
    hash[0]=128;CHECK(stn_pow_compare(hash,t)==STN_DATA_WORK);
    CHECK(stn_target_validate(t,31)==STN_DATA_LENGTH);
    CHECK(stn_target_validate(t,33)==STN_DATA_LENGTH);
    t[0]=128;CHECK(stn_target_validate(t,32)==STN_DATA_TARGET);
    memset(t,255,32);CHECK(stn_target_validate(t,32)==STN_DATA_TARGET);
    memset(t,0,32);saved=w;
    CHECK(stn_target_work(t,&w)==STN_DATA_TARGET && memcmp(w.bytes,saved.bytes,40)==0);
    t[31]=1;CHECK(stn_target_work(t,&w)==STN_DATA_OK && w.bytes[8]==128);
    saved=w;CHECK(stn_work_add(&w,&w,&w)==STN_DATA_OK && w.bytes[7]==1);
    CHECK(stn_work_at_height(t,1,&w)==STN_DATA_OK && w.bytes[7]==1);
    t[31]=2;CHECK(stn_target_work(t,&w)==STN_DATA_OK);
    memset(expected.bytes,0,8);memset(expected.bytes+8,0x55,32);CHECK(memcmp(w.bytes,expected.bytes,40)==0);
    /* Independent exact powers: T=2^k-1 => W=2^(256-k). */
    for(k=1;k<=255;++k){
        unsigned bit;memset(t,0,32);memset(expected.bytes,0,40);
        for(bit=0;bit<k;++bit){t[31-bit/8]|=(uint8_t)(1u<<(bit%8));}
        bit=256-k;expected.bytes[39-bit/8]=(uint8_t)(1u<<(bit%8));
        CHECK(stn_target_work(t,&w)==STN_DATA_OK && memcmp(w.bytes,expected.bytes,40)==0);
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
    memcmp(a->cumulative_work.bytes,b->cumulative_work.bytes,STN_WORK_SIZE)==0;
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
    CHECK(s.cumulative_work.bytes[39]==2 && memcmp(s.current_target,policy.fixed_target,32)==0);
    memcpy(child,genesis,364);memcpy(child+40,s.tip_id,32);child[79]=1;
    r=stn_chain_validate_candidate(&c,&s,child,364,&next);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && next.cumulative_work.bytes[39]==4);
    CHECK(equals_hex(next.tip_id,"35eb9fa251a3eeca9412b3c23159fbdbf26d874fa9af8969341d2ef67d8c8726"));
    spans[0].bytes=genesis;spans[0].length=364;spans[1].bytes=child;spans[1].length=364;
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&full);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && same(&full,&next));
    before=next;child[151]--;
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&next);
    CHECK(r.reason==STN_CHAIN_TARGET && r.failing_index==1 && same(&before,&next));
    child[151]++;s.cumulative_work.bytes[39]=3;
    r=stn_chain_validate_candidate(&c,&s,child,364,&next);
    CHECK(r.reason==STN_CHAIN_STATE && same(&before,&next));s.cumulative_work.bytes[39]=2;
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
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && s.cumulative_work.bytes[8]==128);
    memcpy(child,genesis,364);memcpy(child+40,s.tip_id,32);child[79]=1;next=s;before=s;
    r=stn_chain_validate_candidate(&c,&s,child,364,&next);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && next.cumulative_work.bytes[7]==1);
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&next);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && next.cumulative_work.bytes[7]==1);
    before=next;f.id[31]=2;
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

static void adjustment(void)
{
    static const struct { uint32_t target; uint64_t span; uint32_t expected; } v[]={
        {10000,3600,10000},{10000,1800,5000},{10000,7200,20000},
        {10000,899,2500},{10000,900,2500},{10000,14400,40000},
        {10000,14401,40000},{7,1800,3},{1,900,1},
        {10000,0,2500},{10000,UINT64_MAX,40000},{10000,3601,10002}
    };
    stn_block_header h[60];uint8_t bootstrap[32],out[32],saved[32],expected[32];
    size_t i,j,k;unsigned before=checks;
    memset(h,0,sizeof(h));memset(saved,0xa5,32);
    for(i=0;i<sizeof(v)/sizeof(v[0]);++i) {
        memset(bootstrap,0,32);memset(expected,0,32);
        for(j=0;j<4;++j) {bootstrap[31-j]=(uint8_t)(v[i].target>>(8*j));expected[31-j]=(uint8_t)(v[i].expected>>(8*j));}
        for(j=0;j<60;++j) {h[j].version=3;h[j].height=(uint64_t)j;h[j].timestamp=0;memcpy(h[j].reserved_target,bootstrap,32);}
        h[59].timestamp=v[i].span;
        for(k=0;k<3;++k) {
            CHECK(stn_target_next(60,bootstrap,h,60,out)==STN_DATA_OK);
            CHECK(memcmp(out,expected,32)==0);
            CHECK(stn_pow_compare(out,expected)==STN_DATA_OK);
        }
    }
    easiest(bootstrap);
    for(i=0;i<60;++i) {memcpy(h[i].reserved_target,bootstrap,32);h[i].timestamp=0;}
    h[59].timestamp=14400;
    CHECK(stn_target_next(60,bootstrap,h,60,out)==STN_DATA_OK && memcmp(out,bootstrap,32)==0);
    h[59].timestamp=3600;
    CHECK(stn_target_next(60,bootstrap,h,60,out)==STN_DATA_OK && memcmp(out,bootstrap,32)==0);
    h[59].timestamp=900;
    CHECK(stn_target_next(60,bootstrap,h,60,out)==STN_DATA_OK);
    CHECK(equals_hex(out,"1fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));
    /* All bytes participate; floor discards the nonzero remainder. */
    unhex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",bootstrap);
    for(i=0;i<60;++i) {memcpy(h[i].reserved_target,bootstrap,32);}
    CHECK(stn_target_next(60,bootstrap,h,60,out)==STN_DATA_OK);
    CHECK(equals_hex(out,"0048d159e26af37bc048d159e26af37bc048d159e26af37bc048d159e26af37b"));
    easiest(bootstrap);
    CHECK(stn_target_next(0,bootstrap,NULL,0,out)==STN_DATA_OK && memcmp(out,bootstrap,32)==0);
    for(i=1;i<60;++i) {
        h[0].version=3;h[0].height=(uint64_t)i-1;memcpy(h[0].reserved_target,bootstrap,32);
        CHECK(stn_target_next((uint64_t)i,bootstrap,h,1,out)==STN_DATA_OK && memcmp(out,bootstrap,32)==0);
    }
    h[0].height=60;memset(h[0].reserved_target,0,32);h[0].reserved_target[31]=7;
    CHECK(stn_target_next(61,bootstrap,h,1,out)==STN_DATA_OK && out[31]==7 && out[0]==0);
    h[0].height=UINT64_MAX-1;
    CHECK(stn_target_next(UINT64_MAX,bootstrap,h,1,out)==STN_DATA_OK && out[31]==7);
    for(i=0;i<60;++i) {h[i].version=3;h[i].height=60+(uint64_t)i;h[i].timestamp=100;memset(h[i].reserved_target,0,32);h[i].reserved_target[31]=7;}
    h[59].timestamp=99;
    CHECK(stn_target_next(120,bootstrap,h,60,out)==STN_DATA_OK && out[31]==1);
    h[59].timestamp=100;
    CHECK(stn_target_next(120,bootstrap,h,60,out)==STN_DATA_OK && out[31]==1);
    h[59].timestamp=3700;
    CHECK(stn_target_next(120,bootstrap,h,60,h[59].reserved_target)==STN_DATA_OK && h[59].reserved_target[31]==7);
    memcpy(out,saved,32);
    CHECK(stn_target_next(120,bootstrap,h,59,out)==STN_DATA_UNRESOLVED && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(120,bootstrap,NULL,60,out)==STN_DATA_UNRESOLVED && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(120,bootstrap,h,61,out)==STN_DATA_CONTENT && memcmp(out,saved,32)==0);
    h[20].height=0;
    CHECK(stn_target_next(120,bootstrap,h,60,out)==STN_DATA_CONTENT && memcmp(out,saved,32)==0);
    h[20].height=80;h[20].version=1;
    CHECK(stn_target_next(120,bootstrap,h,60,out)==STN_DATA_CONTENT && memcmp(out,saved,32)==0);
    h[20].version=3;h[20].reserved_target[0]=128;
    CHECK(stn_target_next(120,bootstrap,h,60,out)==STN_DATA_TARGET && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(0,NULL,NULL,0,out)==STN_DATA_ARGUMENT && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(0,bootstrap,NULL,0,NULL)==STN_DATA_ARGUMENT);
    memset(bootstrap,0,32);
    CHECK(stn_target_next(0,bootstrap,NULL,0,out)==STN_DATA_TARGET && memcmp(out,saved,32)==0);
    bootstrap[0]=128;
    CHECK(stn_target_next(0,bootstrap,NULL,0,out)==STN_DATA_TARGET && memcmp(out,saved,32)==0);
    easiest(bootstrap);h[0].height=0;
    CHECK(stn_target_next(1,bootstrap,h,1,out)==STN_DATA_CONTENT && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(0,bootstrap,h,1,out)==STN_DATA_CONTENT && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(1,bootstrap,NULL,0,out)==STN_DATA_UNRESOLVED && memcmp(out,saved,32)==0);
    CHECK(stn_target_next(0,bootstrap,NULL,0,bootstrap)==STN_DATA_OK && bootstrap[0]==127);
    printf("Difficulty calculation: %u targeted checks.\n",checks-before);
}

static void work_domain(void)
{
    static uint8_t blocks[61][364],encoded[24000];
    stn_block_span spans[61];stn_chain_context c={0};stn_pow_policy policy={{0}};
    stn_chain_state state,prior,recovered;stn_work w,sum,expected={{0}},saved,one={{0}};
    fake f={STN_DATA_OK,{0}};stn_storage_view view;stn_chain_report report;
    stn_node_service service={0};stn_rpc_message q={1,STN_RPC_INFO,STN_RPC_OK,1,NULL,0},wire;
    uint8_t response[256],framed[280],target[32];size_t i,n,length;unsigned start=checks;
    policy.fixed_target[31]=1;one.bytes[39]=1;
    CHECK(stn_work_at_height(policy.fixed_target,UINT64_MAX,&w)==STN_DATA_OK);
    expected.bytes[0]=128;CHECK(memcmp(w.bytes,expected.bytes,40)==0); /* 2^319 */
    CHECK(stn_target_work(policy.fixed_target,&w)==STN_DATA_OK);
    CHECK(stn_work_add(&w,&w,&sum)==STN_DATA_OK && sum.bytes[7]==1); /* 2^256 */
    {stn_fork_result order;CHECK(stn_work_order(&w,&sum,&order)==STN_DATA_OK && order==STN_FORK_CANDIDATE);}
    memset(w.bytes,255,40);w.bytes[39]=254;
    CHECK(stn_work_add(&w,&one,&w)==STN_DATA_OK && w.bytes[39]==255);
    saved=w;CHECK(stn_work_add(&w,&one,&w)==STN_DATA_OVERFLOW && memcmp(w.bytes,saved.bytes,40)==0);
    for(i=1;i<=4;++i){policy.fixed_target[31]=(uint8_t)i;
        CHECK(stn_work_at_height(policy.fixed_target,UINT64_MAX,&w)==STN_DATA_OK);
        CHECK(w.bytes[0]<=128);}
    policy.fixed_target[31]=1;fixture(blocks[0]);memcpy(blocks[0]+120,policy.fixed_target,32);
    c.network_id[0]=1;c.genesis_bytes=blocks[0];c.genesis_length=364;c.pow_policy=&policy;
    c.hash_provider.hash=fake_provider;c.hash_provider.user=&f;f.id[31]=1;
    CHECK(stn_chain_initialize(&c,&state)==STN_DATA_OK);
    for(i=0;i<61;++i){
        if(i!=0){memcpy(blocks[i],blocks[0],364);blocks[i][79]=(uint8_t)i;memcpy(blocks[i]+40,state.tip_id,32);}
        spans[i].bytes=blocks[i];spans[i].length=364;
        CHECK(stn_chain_required_target(&c,&state,target)==STN_DATA_OK && target[31]==1);
        CHECK(stn_chain_validate_candidate(&c,&state,blocks[i],364,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    }
    CHECK(stn_work_at_height(policy.fixed_target,60,&expected)==STN_DATA_OK && memcmp(state.cumulative_work.bytes,expected.bytes,40)==0);
    CHECK(stn_storage_encode(&c,spans,61,encoded,sizeof(encoded),&length)==STN_STORAGE_OK);
    CHECK(stn_storage_decode(&c,encoded,length,&view)==STN_STORAGE_OK);
    recovered=view.state;stn_storage_view_release(&view);
    CHECK(memcmp(recovered.cumulative_work.bytes,state.cumulative_work.bytes,40)==0);
    CHECK(stn_chain_required_target(&c,&recovered,target)==STN_DATA_OK && target[31]==1);
    service.chain=&c;service.blocks=spans;service.count=61;
    CHECK(stn_node_service_handle(&service,&q,response,sizeof(response),&n)==STN_RPC_OK && n==184);
    CHECK(memcmp(response+104,expected.bytes,40)==0 && response[111]!=0 && response[175]==1 && response[183]==61);
    wire=q;wire.kind=2;wire.payload=response;wire.length=n;
    CHECK(stn_rpc_encode(&wire,framed,sizeof(framed),&n)==STN_RPC_OK && framed[5]==2);
    CHECK(stn_rpc_decode(framed,n,&wire)==STN_RPC_OK && memcmp(wire.payload+104,expected.bytes,40)==0);
    wire.length=183;CHECK(stn_rpc_encode(&wire,framed,sizeof(framed),&length)==STN_RPC_INVALID);
    wire.length=185;CHECK(stn_rpc_encode(&wire,framed,sizeof(framed),&length)==STN_RPC_INVALID);
    framed[5]=1;CHECK(stn_rpc_decode(framed,n,&wire)==STN_RPC_VERSION);
    {stn_peer_session session={0};stn_peer_message message;
        session.handshake=1;
        CHECK(stn_peer_encode(STN_PEER_STATE,NULL,0,framed,sizeof(framed),&n)==STN_PEER_OK);
        CHECK(stn_peer_serve(&c,spans,61,&session,framed,n,response,95,&length)==STN_PEER_CAPACITY);
        CHECK(stn_peer_serve(&c,spans,61,&session,framed,n,response,sizeof(response),&length)==STN_PEER_OK && length==96);
        CHECK(stn_peer_decode(response,length,&message)==STN_PEER_OK && message.length==84);
        CHECK(memcmp(message.payload+40,expected.bytes,40)==0 && message.payload[83]==61);
        CHECK(stn_peer_decode(response,length-1,&message)==STN_PEER_PROTOCOL);
    }
    /* Old fixed-target evidence: real target mismatch, never rewritten. */
    memset(policy.fixed_target,255,32);policy.fixed_target[0]=127;
    for(i=0;i<61;++i){memcpy(blocks[i]+120,policy.fixed_target,32);}
    CHECK(stn_chain_initialize(&c,&state)==STN_DATA_OK);
    for(i=0;i<60;++i){CHECK(stn_chain_validate_candidate(&c,&state,blocks[i],364,&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);}
    prior=state;report=stn_chain_validate_candidate(&c,&state,blocks[60],364,&state);
    CHECK(report.reason==STN_CHAIN_TARGET && report.detail==STN_DATA_TARGET && report.failing_height==60);
    CHECK(memcmp(&prior,&state,sizeof(state))==0 && memcmp(blocks[60]+120,policy.fixed_target,32)==0);
    CHECK(stn_storage_encode(&c,spans,61,encoded,sizeof(encoded),&n)==STN_STORAGE_VALIDATION);
    printf("320-bit domain/history: %u targeted checks.\n",checks-start);
}

int test_pow(void);
int test_pow(void)
{
    hash_vectors();targets_and_work();pow_chain();adjustment();work_domain();
    printf("SHA-256/PoW/work: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}
