/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_fork.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"fork line %d: %s\n",__LINE__,#e);} } while(0)
/* Deterministic test-only header hashes. No search/mining; real SHA-256
 * computes transaction/body commitments. Never a production fallback. */
typedef struct hook { stn_data_status status; int bad_pow; } hook;
static stn_data_status hashing(void *u,const uint8_t *d,size_t dn,const uint8_t *b,size_t n,uint8_t out[32])
{
    hook *h=u;
    if(h->status!=STN_DATA_OK) { return h->status; }
    if(dn==sizeof("STN-CHAIN:BLOCK:ID:1") && memcmp(d,"STN-CHAIN:BLOCK:ID:1",dn)==0) {
        memset(out,0,32);out[30]=b[87];out[31]=(uint8_t)(b[79]+1);
        if(h->bad_pow==1 && b[79]!=0) { out[0]=128; }
        if(h->bad_pow==2) { memset(out,0,32);out[31]=1; }
        if(h->bad_pow==3 && b[79]!=0) { return STN_DATA_PROVIDER_ERROR; }
        if(h->bad_pow==4 && b[79]!=0) { return STN_DATA_UNRESOLVED; }
        return STN_DATA_OK;
    }
    return stn_sha256(NULL,d,dn,b,n,out);
}
static void fixture(uint8_t b[364])
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(b,0,364);memcpy(b,"STNB",4);b[5]=3;b[8]=1;b[163]=1;b[167]=196;b[171]=192;
    memcpy(b+172,"STNT",4);b[177]=1;b[179]=1;b[183]=180;
    memcpy(b+184,"STNR",4);b[189]=1;b[191]=1;b[192]=1;b[256]=3;
    memcpy(b+88,commitment,32);memset(b+120,255,32);b[120]=127;
}
static void branch(uint8_t b[6][364],stn_block_span spans[6],unsigned split)
{
    unsigned i;
    fixture(b[0]);
    for(i=0;i<6;++i) {
        if(i!=0) {
            memcpy(b[i],b[i-1],364);memset(b[i]+40,0,32);
            b[i][70]=b[i-1][87];b[i][71]=(uint8_t)i;b[i][79]=(uint8_t)i;
            if(i>=split) { b[i][87]=1; }
        }
        spans[i].bytes=b[i];spans[i].length=364;
    }
}
int test_fork(void);
int test_fork(void)
{
    uint8_t a[6][364],b[6][364],saved[6][364];
    stn_block_span as[6],bs[6];stn_pow_policy policy;
    stn_chain_context c={0};hook h={STN_DATA_OK,0};
    stn_reorg_plan p,before;stn_fork_report r;unsigned split,i;
    stn_work easy,hard;stn_fork_result order=STN_FORK_ERROR;
    branch(a,as,99);branch(b,bs,99);memcpy(policy.fixed_target,a[0]+120,32);
    c.network_id[0]=1;c.genesis_bytes=a[0];c.genesis_length=364;
    c.pow_policy=&policy;c.hash_provider.hash=hashing;c.hash_provider.user=&h;
    memcpy(saved,a,sizeof(a));
    r=stn_fork_evaluate(&c,as,2,bs,3,&p);
    CHECK(r.result==STN_FORK_CANDIDATE && p.actionable);
    CHECK(p.ancestor_index==1 && p.detached_count==0 && p.attached_count==1);
    CHECK(p.attach_begin==2 && p.attach_end==3 && p.resulting_height==2);
    CHECK(p.current.cumulative_work.bytes[39]==4 && p.candidate.cumulative_work.bytes[39]==6);
    CHECK(memcmp(saved,a,sizeof(a))==0);
    r=stn_fork_evaluate(&c,as,4,bs,2,&p);
    CHECK(r.result==STN_FORK_CURRENT && !p.actionable && p.resulting_height==3);
    CHECK(p.ancestor_index==1 && p.detached_count==2 && p.attached_count==0);
    r=stn_fork_evaluate(&c,as,4,as,4,&p);
    CHECK(r.result==STN_FORK_TIE && !p.actionable && p.detached_count==0 && p.attached_count==0);
    for(split=1;split<5;++split) {
        branch(b,bs,split);
        r=stn_fork_evaluate(&c,as,5,bs,6,&p);
        CHECK(r.result==STN_FORK_CANDIDATE && p.ancestor_index==split-1);
        CHECK(p.detach_begin==split && p.detach_end==5 && p.detached_count==5-split);
        CHECK(p.attach_begin==split && p.attach_end==6 && p.attached_count==6-split);
        CHECK(p.ancestor_id[31]==split && p.ancestor_id[30]==0);
        r=stn_fork_evaluate(&c,as,5,bs,5,&p);
        CHECK(r.result==STN_FORK_TIE && !p.actionable);
    }
    before=p;
    /* Every malformed candidate truncation preserves the entire output. */
    for(i=0;i<364;++i) {
        bs[1].length=i;
        r=stn_fork_evaluate(&c,as,2,bs,2,&p);
        CHECK(r.result==STN_FORK_INVALID_CANDIDATE && r.validation.failing_index==1);
        CHECK(memcmp(&p,&before,sizeof(p))==0);
    }
    bs[1].length=364;
    b[0][87]=2;
    r=stn_fork_evaluate(&c,as,2,bs,2,&p);
    CHECK(r.result==STN_FORK_INVALID_CANDIDATE && r.validation.reason==STN_CHAIN_GENESIS);
    b[0][87]=0;b[1][8]=2;
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,&p).result==STN_FORK_INVALID_CANDIDATE);
    b[1][8]=1;b[1][120]=128;
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,&p).result==STN_FORK_INVALID_CANDIDATE);
    b[1][120]=127;b[1][151]=254;
    r=stn_fork_evaluate(&c,as,2,bs,2,&p);
    CHECK(r.result==STN_FORK_INVALID_CANDIDATE && r.validation.reason==STN_CHAIN_TARGET);
    b[1][151]=255;b[1][71]=9;
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,&p).result==STN_FORK_INVALID_CANDIDATE);
    b[1][71]=1;b[1][363]=1;
    r=stn_fork_evaluate(&c,as,2,bs,2,&p);
    CHECK(r.result==STN_FORK_INVALID_CANDIDATE && r.validation.reason==STN_CHAIN_BODY);
    b[1][363]=0;h.bad_pow=1;
    r=stn_fork_evaluate(&c,as,1,bs,2,&p);
    CHECK(r.result==STN_FORK_INVALID_CANDIDATE && r.validation.reason==STN_CHAIN_POW);
    CHECK(stn_fork_evaluate(&c,as,2,bs,1,&p).result==STN_FORK_INVALID_CURRENT);
    h.bad_pow=0;h.status=STN_DATA_PROVIDER_ERROR;
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,&p).result==STN_FORK_ERROR);
    h.status=STN_DATA_UNRESOLVED;
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,&p).result==STN_FORK_UNRESOLVED);
    h.status=STN_DATA_OK;
    CHECK(stn_fork_evaluate(&c,as,0,bs,2,&p).result==STN_FORK_INVALID_CURRENT);
    CHECK(stn_fork_evaluate(&c,as,2,bs,65,&p).result==STN_FORK_INVALID_CANDIDATE);
    CHECK(stn_fork_evaluate(NULL,as,2,bs,2,&p).result==STN_FORK_ERROR);
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,NULL).result==STN_FORK_ERROR);
    c.pow_policy=NULL;
    CHECK(stn_fork_evaluate(&c,as,2,bs,2,&p).result==STN_FORK_UNSUPPORTED);
    c.pow_policy=&policy;
    CHECK(memcmp(&p,&before,sizeof(p))==0);
    h.bad_pow=3;
    r=stn_fork_evaluate(&c,as,1,bs,2,&p);
    CHECK(r.result==STN_FORK_ERROR && r.failed_side==2);
    h.bad_pow=4;
    r=stn_fork_evaluate(&c,as,1,bs,2,&p);
    CHECK(r.result==STN_FORK_UNRESOLVED && r.failed_side==2);
    h.bad_pow=0;
    /* Claimed output work is never an input to evaluation. */
    memset(p.candidate.cumulative_work.bytes,255,STN_WORK_SIZE);
    r=stn_fork_evaluate(&c,as,3,bs,2,&p);
    CHECK(r.result==STN_FORK_CURRENT && p.candidate.cumulative_work.bytes[39]==4);
    /* Arithmetic qualification only: three easy blocks lose to one harder
     * block. These different-policy totals are NOT eligible competing chains. */
    CHECK(stn_work_at_height(policy.fixed_target,2,&easy)==STN_DATA_OK);
    policy.fixed_target[0]=15;
    CHECK(stn_work_at_height(policy.fixed_target,0,&hard)==STN_DATA_OK);
    CHECK(stn_work_order(&easy,&hard,&order)==STN_DATA_OK && order==STN_FORK_CANDIDATE);
    CHECK(stn_work_order(&hard,&easy,&order)==STN_DATA_OK && order==STN_FORK_CURRENT);
    CHECK(stn_work_order(&hard,&hard,&order)==STN_DATA_OK && order==STN_FORK_TIE);
    CHECK(stn_work_order(NULL,&hard,&order)==STN_DATA_ARGUMENT && order==STN_FORK_TIE);
    /* Overflow is rejected during history validation, before a plan exists. */
    memset(policy.fixed_target,0,32);policy.fixed_target[31]=1;
    fixture(a[0]);memcpy(a[0]+120,policy.fixed_target,32);
    memcpy(a[1],a[0],364);a[1][79]=1;a[1][71]=1;h.bad_pow=2;before=p;
    r=stn_fork_evaluate(&c,as,1,as,2,&p);
    CHECK(r.result==STN_FORK_CANDIDATE && p.candidate.cumulative_work.bytes[7]==1);
    CHECK(p.actionable);
    /* Real production SHA-256 genesis and direct child, independently fixed. */
    fixture(a[0]);memcpy(a[1],a[0],364);a[1][79]=1;
    c.hash_provider.hash=stn_sha256;c.hash_provider.user=NULL;
    memcpy(policy.fixed_target,a[0]+120,32);
    CHECK(stn_chain_block_id(a[0],364,&c.hash_provider,a[1]+40)==STN_DATA_OK);
    r=stn_fork_evaluate(&c,as,1,as,2,&p);
    CHECK(r.result==STN_FORK_CANDIDATE && p.candidate.cumulative_work.bytes[39]==4);
    printf("Fork choice/planning: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}
