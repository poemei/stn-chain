/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_chain.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; \
    fprintf(stderr,"chain line %d: %s\n",__LINE__,#expr); } } while (0)

/* Independent fixed development anchor. Toy body provider returns 44 + zeros.
 * Empty intelligence payload and synthetic signature are NOT semantic approval. */
static const uint8_t genesis[364]={
    [0]=0x53,[1]=0x54,[2]=0x4e,[3]=0x42,[5]=1,[8]=1,[88]=0x44,
    [163]=1,[167]=196,[171]=192,
    [172]=0x53,[173]=0x54,[174]=0x4e,[175]=0x54,[177]=1,[179]=1,[183]=180,
    [184]=0x53,[185]=0x54,[186]=0x4e,[187]=0x52,[189]=1,[191]=1,[192]=1,[256]=3
};

typedef struct hash_control { stn_data_status status; int zero_id; } hash_control;
static stn_data_status toy_hash(void *user,const uint8_t *domain,size_t dn,
    const uint8_t *bytes,size_t n,uint8_t digest[32])
{
    hash_control *c=user;
    size_t i;
    memset(digest,0,32);
    if(c->status!=STN_DATA_OK) { digest[0]=0xff; return c->status; }
    if(dn==sizeof("STN-CHAIN:BLOCK:BODY:1") && memcmp(domain,"STN-CHAIN:BLOCK:BODY:1",dn)==0) {
        digest[0]=0x44; return STN_DATA_OK;
    }
    if(dn==sizeof("STN-CHAIN:BLOCK:ID:1") && memcmp(domain,"STN-CHAIN:BLOCK:ID:1",dn)==0) {
        CHECK(n==168);
        if(c->zero_id) { return STN_DATA_OK; }
    } else { CHECK(dn==sizeof("STN-CHAIN:TX:ID:1") && memcmp(domain,"STN-CHAIN:TX:ID:1",dn)==0); }
    for(i=0;i<dn;++i) { digest[i%32]=(uint8_t)(digest[i%32]*33u+domain[i]); }
    for(i=0;i<n;++i) { digest[i%32]=(uint8_t)(digest[i%32]*33u+bytes[i]); }
    return STN_DATA_OK;
}

static stn_chain_context context(hash_control *hash)
{
    stn_chain_context c={0};
    c.network_id[0]=1; c.genesis_bytes=genesis; c.genesis_length=sizeof(genesis);
    c.hash_provider.hash=toy_hash; c.hash_provider.user=hash;
    return c;
}

/* Test builder patches an independent fixture, not a codec-generated oracle. */
static void number(uint8_t *p,uint64_t n)
{
    size_t i=8;
    while(i!=0) { p[--i]=(uint8_t)(n&255u); n>>=8; }
}
static void child(uint8_t bytes[364],const stn_chain_state *s)
{
    memcpy(bytes,genesis,364); memcpy(bytes+40,s->tip_id,32);
    number(bytes+72,s->height+1); number(bytes+80,s->timestamp);
}
static int same(const stn_chain_state *a,const stn_chain_state *b)
{
    return a->height==b->height && a->timestamp==b->timestamp && a->has_tip==b->has_tip &&
        memcmp(a->network_id,b->network_id,32)==0 && memcmp(a->genesis_id,b->genesis_id,32)==0 &&
        memcmp(a->tip_id,b->tip_id,32)==0;
}

static void valid_and_atomic(void)
{
    hash_control h={STN_DATA_OK,0}; stn_chain_context c=context(&h);
    stn_chain_state empty,s,prior,out;
    stn_chain_report r;
    uint8_t bytes[364], loaded[364], id[32];
    stn_block_span spans[2];
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK);
    CHECK(empty.has_tip==0 && empty.height==0 && empty.timestamp==0);
    out=empty;
    r=stn_chain_validate_sequence(&c,&empty,NULL,0,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && same(&out,&empty) && r.structure==STN_STAGE_NOT_RUN);
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&s);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && s.has_tip==1 && s.height==0);
    CHECK(r.structure==STN_STAGE_PASS && r.link==STN_STAGE_PASS && r.body==STN_STAGE_PASS &&
        r.identifier==STN_STAGE_PASS && r.pow==STN_STAGE_NOT_RUN && r.failing_index==SIZE_MAX);
    CHECK(stn_chain_block_id(genesis,364,&c.hash_provider,id)==STN_DATA_OK && memcmp(id,s.tip_id,32)==0);
    CHECK(memcmp(s.genesis_id,s.tip_id,32)==0);
    prior=s; child(bytes,&s); spans[0].bytes=genesis; spans[0].length=364;
    spans[1].bytes=bytes; spans[1].length=364;
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && out.height==1 && same(&prior,&s));
    memcpy(loaded,bytes,364);
    r=stn_chain_validate_candidate(&c,&s,loaded,364,&prior);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && same(&out,&prior));
    bytes[40]^=1; out=prior;
    r=stn_chain_validate_sequence(&c,&empty,spans,2,&out);
    CHECK(r.reason==STN_CHAIN_PARENT && r.failing_index==1 && r.height_available && r.failing_height==1);
    CHECK(same(&out,&prior));
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&s);
    CHECK(r.reason==STN_CHAIN_PARENT && s.height==0 && memcmp(s.tip_id,s.genesis_id,32)==0);
    bytes[40]^=1;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&s);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && s.height==1);
}

static void failures_and_time(void)
{
    hash_control h={STN_DATA_OK,0}; stn_chain_context c=context(&h);
    stn_chain_state empty,s,out,saved;
    stn_chain_report r;
    uint8_t bytes[364],duplicate[560];
    size_t i;
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK);
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&s);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    out=s; saved=s;
    child(bytes,&s); number(bytes+72,0);
    /* Repeated height 0 must also have zero parent to remain structural. */
    memset(bytes+40,0,32);
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_HEIGHT && same(&out,&saved));
    child(bytes,&s); number(bytes+72,2);
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_HEIGHT && same(&out,&saved));
    child(bytes,&s); bytes[8]=2;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_NETWORK && same(&out,&saved));
    child(bytes,&s); bytes[5]=2;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_STRUCTURE && !r.height_available && same(&out,&saved));
    child(bytes,&s); bytes[172]=0;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_STRUCTURE && r.height_available && r.failing_height==1);
    child(bytes,&s); bytes[88]^=1;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_BODY && r.detail==STN_DATA_COMMITMENT && same(&out,&saved));
    child(bytes,&s); bytes[192]=2;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_NETWORK && r.body==STN_STAGE_REJECT && same(&out,&saved));
    memcpy(bytes,genesis,364); bytes[87]=1;
    r=stn_chain_validate_candidate(&c,&empty,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_GENESIS && same(&out,&saved));
    child(bytes,&s);
    memcpy(duplicate,bytes,364); memcpy(duplicate+364,bytes+168,196);
    duplicate[163]=2; duplicate[166]=1; duplicate[167]=0x88;
    r=stn_chain_validate_candidate(&c,&s,duplicate,sizeof(duplicate),&out);
    CHECK(r.reason==STN_CHAIN_BODY && r.detail==STN_DATA_DUPLICATE && same(&out,&saved));
    for(i=0;i<364;++i) {
        r=stn_chain_validate_candidate(&c,&s,bytes,i,&out);
        CHECK(r.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT && same(&out,&saved));
    }
    number(bytes+80,UINT64_MAX);
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && out.timestamp==UINT64_MAX);
    s=out; child(bytes,&s);
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT); /* equality permitted */
    number(bytes+80,UINT64_MAX-1); out=s;
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_TIMESTAMP && same(&out,&s));
    /* Synthetic state boundary exercises overflow guard, not historical trust. */
    s.height=UINT64_MAX; out=s; number(bytes+72,1);
    r=stn_chain_validate_candidate(&c,&s,bytes,364,&out);
    CHECK(r.reason==STN_CHAIN_HEIGHT && same(&out,&s));
}

static void providers_and_inputs(void)
{
    hash_control h={STN_DATA_OK,0}; stn_chain_context c=context(&h);
    stn_chain_state empty,out,bad,saved;
    stn_chain_report r;
    uint8_t digest[32],before[32];
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK); out=empty;
    c.hash_provider.hash=NULL;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNRESOLVED && r.body==STN_STAGE_UNRESOLVED && same(&out,&empty));
    c=context(&h); h.status=STN_DATA_UNRESOLVED;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNRESOLVED && same(&out,&empty));
    h.status=STN_DATA_PROVIDER_ERROR;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_ERROR && same(&out,&empty));
    memset(digest,0xa5,32); memcpy(before,digest,32);
    CHECK(stn_chain_block_id(genesis,364,&c.hash_provider,digest)==STN_DATA_PROVIDER_ERROR && memcmp(digest,before,32)==0);
    CHECK(stn_chain_block_id(genesis,364,NULL,digest)==STN_DATA_UNRESOLVED && memcmp(digest,before,32)==0);
    h.status=STN_DATA_OK; h.zero_id=1;
    r=stn_chain_validate_candidate(&c,&empty,genesis,364,&out);
    CHECK(r.reason==STN_CHAIN_ZERO_ID && same(&out,&empty));
    h.zero_id=0;
    bad=empty; bad.height=1; saved=out;
    r=stn_chain_validate_sequence(&c,&bad,NULL,0,&out);
    CHECK(r.reason==STN_CHAIN_STATE && same(&out,&saved));
    bad=empty; bad.network_id[0]=2;
    r=stn_chain_validate_candidate(&c,&bad,genesis,364,&out);
    CHECK(r.reason==STN_CHAIN_STATE && same(&out,&saved));
    r=stn_chain_validate_sequence(&c,&empty,NULL,1,&out);
    CHECK(r.reason==STN_CHAIN_ARGUMENT && same(&out,&saved));
    r=stn_chain_validate_sequence(&c,&empty,NULL,0,NULL);
    CHECK(r.reason==STN_CHAIN_ARGUMENT);
    r=stn_chain_validate_candidate(NULL,&empty,genesis,364,&out);
    CHECK(r.reason==STN_CHAIN_CONTEXT && same(&out,&saved));
    c.genesis_length=363;
    CHECK(stn_chain_initialize(&c,&out)!=STN_DATA_OK && same(&out,&saved));
}

static void batch(void)
{
    static uint8_t blocks[STN_CHAIN_MAX_BATCH][364];
    stn_block_span spans[STN_CHAIN_MAX_BATCH];
    hash_control h={STN_DATA_OK,0}; stn_chain_context c=context(&h);
    stn_chain_state empty,s,out,saved;
    stn_chain_report r;
    size_t i;
    CHECK(stn_chain_initialize(&c,&empty)==STN_DATA_OK); s=empty;
    for(i=0;i<STN_CHAIN_MAX_BATCH;++i) {
        if(i==0) { memcpy(blocks[i],genesis,364); } else { child(blocks[i],&s); }
        spans[i].bytes=blocks[i]; spans[i].length=364;
        r=stn_chain_validate_candidate(&c,&s,blocks[i],364,&s);
        CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    }
    r=stn_chain_validate_sequence(&c,&empty,spans,STN_CHAIN_MAX_BATCH,&out);
    CHECK(r.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT && same(&s,&out) && out.height==63);
    saved=out;
    r=stn_chain_validate_sequence(&c,&empty,spans,STN_CHAIN_MAX_BATCH+1,&out);
    CHECK(r.reason==STN_CHAIN_BATCH_LIMIT && same(&out,&saved));
    blocks[31][40]^=1;
    r=stn_chain_validate_sequence(&c,&empty,spans,STN_CHAIN_MAX_BATCH,&out);
    CHECK(r.reason==STN_CHAIN_PARENT && r.failing_index==31 && r.failing_height==31 && same(&out,&saved));
    /* Later corruption cannot change which failure is reported first. */
    blocks[32][0]=0;
    r=stn_chain_validate_sequence(&c,&empty,spans,STN_CHAIN_MAX_BATCH,&out);
    CHECK(r.reason==STN_CHAIN_PARENT && r.failing_index==31 && same(&out,&saved));
    blocks[31][40]^=1;
    r=stn_chain_validate_sequence(&c,&empty,spans,STN_CHAIN_MAX_BATCH,&out);
    CHECK(r.reason==STN_CHAIN_STRUCTURE && r.failing_index==32 && same(&out,&saved));
}

int test_chain(void);
int test_chain(void)
{
    valid_and_atomic(); failures_and_time(); providers_and_inputs(); batch();
    printf("Chain context: %u checks, %u failures (test hashing, no consensus).\n",checks,failures);
    return failures==0 ? 0 : 1;
}
