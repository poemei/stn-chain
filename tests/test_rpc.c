/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_rpc.h"
#include "stn_node_service.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"rpc line %d: %s\n",__LINE__,#e);} } while(0)
static uint8_t request[STN_RPC_MAX_FRAME+8],response[STN_RPC_MAX_FRAME+8],large[STN_RPC_MAX_PAYLOAD];
static stn_rpc_message call(uint16_t method,const uint8_t *payload,size_t length,uint32_t allowed,const stn_rpc_service *service)
{
    stn_rpc_message q={1,0,STN_RPC_OK,UINT64_C(0x0102030405060708),NULL,0},r={0};size_t n,w;
    q.method=method;q.payload=payload;q.length=length;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK);
    CHECK(stn_rpc_dispatch(request,n,allowed,service,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&r)==STN_RPC_OK && r.request_id==q.request_id && r.method==q.method);
    return r;
}
typedef struct spy { unsigned calls;int mode; } spy;
static stn_rpc_code handler(void *u,const stn_rpc_message *q,uint8_t *p,size_t cap,size_t *n)
{
    spy *s=u;++s->calls;(void)q;
    if(s->mode==1){*n=cap+1;return STN_RPC_OK;}
    if(s->mode==2){*n=0;return (stn_rpc_code)99;}
    if(s->mode==3){memset(p,0x55,8);*n=8;return STN_RPC_REJECTED;}
    if(cap<184){return STN_RPC_CAPACITY;}memset(p,0,184);*n=184;return STN_RPC_OK;
}
static void codecs(void)
{
    static const uint8_t independent[24]={'S','T','N','C',0,2,0,1,0,1,0,0,1,2,3,4,5,6,7,8,0,0,0,0};
    stn_rpc_message q,before,r;size_t n,w,i;spy s={0};stn_rpc_service service={&s,handler};uint8_t copy[64];
    CHECK(stn_rpc_decode(independent,24,&q)==STN_RPC_OK && q.kind==1 && q.method==1 && q.request_id==UINT64_C(0x0102030405060708));
    CHECK(stn_rpc_encode(&q,copy,sizeof(copy),&n)==STN_RPC_OK && n==24 && memcmp(copy,independent,24)==0);before=q;
    for(i=0;i<24;++i){
        CHECK(stn_rpc_decode(independent,i,&q)!=STN_RPC_OK && memcmp(&q,&before,sizeof(q))==0);
        CHECK(stn_rpc_dispatch(independent,i,7,&service,response,sizeof(response),&w)==STN_RPC_OK);
        CHECK(stn_rpc_decode(response,w,&r)==STN_RPC_OK && r.code==STN_RPC_INVALID && r.request_id==0 && s.calls==0);
    }
    for(i=0;i<8;++i){memcpy(request+i,independent,24);CHECK(stn_rpc_decode(request+i,24,&q)==STN_RPC_OK && q.request_id==before.request_id);}
    memcpy(copy,independent,24);copy[5]=1;
    CHECK(stn_rpc_dispatch(copy,24,7,&service,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&r)==STN_RPC_OK && r.code==STN_RPC_VERSION && s.calls==0);
    memcpy(copy,independent,24);copy[0]=0;CHECK(stn_rpc_decode(copy,24,&q)==STN_RPC_INVALID);
    memcpy(copy,independent,24);copy[11]=1;CHECK(stn_rpc_decode(copy,24,&q)==STN_RPC_INVALID);
    memcpy(copy,independent,24);memset(copy+20,255,4);CHECK(stn_rpc_decode(copy,24,&q)==STN_RPC_INVALID);
    memcpy(copy,independent,24);copy[24]=0;CHECK(stn_rpc_decode(copy,25,&q)==STN_RPC_INVALID);
    CHECK(stn_rpc_decode(copy,SIZE_MAX,&q)==STN_RPC_INVALID);
    q=before;q.method=STN_RPC_BLOCK_HEIGHT;q.payload=large;q.length=7;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_INVALID && n==0);
    memcpy(copy,independent,24);copy[9]=2; /* valid header, invalid method payload */
    CHECK(stn_rpc_dispatch(copy,24,7,&service,response,sizeof(response),&w)==STN_RPC_OK && s.calls==0);
    r=call(0xffff,NULL,0,7,&service);CHECK(r.code==STN_RPC_METHOD && s.calls==0);
    r=call(STN_RPC_INFO,NULL,0,0,&service);CHECK(r.code==STN_RPC_FORBIDDEN && s.calls==0);
    r=call(STN_RPC_INFO,NULL,0,8,&service);CHECK(r.code==STN_RPC_FORBIDDEN && s.calls==0);
    r=call(STN_RPC_ADMIN_CONTROL,NULL,0,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_FORBIDDEN && s.calls==0);
    r=call(STN_RPC_SUBMIT_INTELLIGENCE,large,180,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_FORBIDDEN && s.calls==0);
    r=call(STN_RPC_INFO,NULL,0,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_OK && s.calls==1);
    for(s.mode=1;s.mode<=3;++s.mode){r=call(STN_RPC_INFO,NULL,0,STN_RPC_READ,&service);CHECK(r.code==(s.mode==3?STN_RPC_REJECTED:STN_RPC_PROVIDER) && r.length==0);}
    r=call(STN_RPC_INFO,NULL,0,STN_RPC_READ,NULL);CHECK(r.code==STN_RPC_UNAVAILABLE);
    /* Maximum request payload, fixed nested length, and exact wire round trip. */
    memset(large,0,sizeof(large));large[64]=(uint8_t)(STN_BLOCK_MAX_SIZE>>24);large[65]=(uint8_t)(STN_BLOCK_MAX_SIZE>>16);
    large[66]=(uint8_t)((STN_BLOCK_MAX_SIZE>>8)&255u);large[67]=(uint8_t)(STN_BLOCK_MAX_SIZE&255u);
    q=before;q.method=STN_RPC_SUBMIT_WORK;q.payload=large;q.length=sizeof(large);
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK && n==STN_RPC_MAX_FRAME);
    CHECK(stn_rpc_decode(request,n,&r)==STN_RPC_OK && r.length==sizeof(large));
    CHECK(stn_rpc_encode(&r,response,sizeof(response),&w)==STN_RPC_OK && w==n && memcmp(request,response,n)==0);
    CHECK(stn_rpc_decode(request,n-1,&r)==STN_RPC_INVALID);CHECK(stn_rpc_decode(request,n+1,&r)==STN_RPC_INVALID);
    request[24+67]^=1;CHECK(stn_rpc_decode(request,n,&r)==STN_RPC_INVALID);
    q.kind=2;q.method=STN_RPC_BLOCK_ID;q.length=STN_BLOCK_MAX_SIZE;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK && n==24+STN_BLOCK_MAX_SIZE);
    CHECK(stn_rpc_decode(request,n,&r)==STN_RPC_OK && r.length==STN_BLOCK_MAX_SIZE);
    q.method=STN_RPC_INFO;CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_INVALID);
    q.code=STN_RPC_REJECTED;CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_INVALID);
    q.length=0;q.payload=NULL;CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK && n==24);
    CHECK(stn_rpc_dispatch(independent,24,1,&service,response,23,&w)==STN_RPC_CAPACITY && w==0);
    /* Different object padding and pointer addresses cannot affect bytes. */
    { stn_rpc_message x,y;memset(&x,0x55,sizeof(x));memset(&y,0xaa,sizeof(y));
      x.kind=y.kind=1;x.method=y.method=STN_RPC_BLOCK_HEIGHT;x.code=y.code=STN_RPC_OK;
      x.request_id=y.request_id=UINT64_MAX;x.length=y.length=8;x.payload=independent+12;
      memcpy(copy,independent+12,8);y.payload=copy;
      CHECK(stn_rpc_encode(&x,request,sizeof(request),&n)==STN_RPC_OK);
      CHECK(stn_rpc_encode(&y,response,sizeof(response),&w)==STN_RPC_OK && w==n && memcmp(request,response,n)==0);
    }
}
static void fixture(uint8_t p[364])
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(p,0,364);memcpy(p,"STNB",4);p[5]=3;p[8]=1;p[163]=1;p[167]=196;p[171]=192;
    memcpy(p+172,"STNT",4);p[177]=1;p[179]=1;p[183]=180;
    memcpy(p+184,"STNR",4);p[189]=1;p[191]=1;p[192]=1;p[256]=3;
    memcpy(p+88,commitment,32);memset(p+120,255,32);p[120]=127;
}

static unsigned signature_calls;
static stn_stage_status signature(void *u,const uint8_t *d,size_t dn,const uint8_t *b,size_t n,const uint8_t key[32],const uint8_t sig[64])
{ (void)u;(void)d;(void)dn;(void)b;(void)n;(void)key;(void)sig;++signature_calls;return STN_STAGE_PASS; }
static stn_stage_status authority(void *u,const stn_record *r,const stn_intelligence *i){(void)u;(void)r;(void)i;return STN_STAGE_PASS;}
static stn_stage_status replay(void *u,const stn_record *r){(void)u;(void)r;return STN_STAGE_PASS;}
static void node(void)
{
    static const uint8_t intel[232]={ [0]='S',[1]='T',[2]='N',[3]='R',[5]=1,[7]=1,[8]=1,[40]=2,[72]=3,[111]=10,[115]=52,[117]=1,[125]=5,[126]=1,[128]=1,[129]='a',[131]=1,[132]='b',[134]=1,[135]='c',[136]=1,[168]=4 };
    uint8_t genesis[364],child[364],old[364],id[32],height[8]={0},bad[232],work[432]={0};
    stn_chain_context c={0};stn_pow_policy policy;stn_validation_context v={0};stn_rpc_message r;
    stn_block_span blocks[2];stn_node_service node_service;stn_rpc_service service;
    fixture(genesis);memcpy(policy.fixed_target,genesis+120,32);
    c.network_id[0]=1;c.genesis_bytes=genesis;c.genesis_length=364;c.pow_policy=&policy;c.hash_provider.hash=stn_sha256;
    CHECK(stn_chain_block_id(genesis,364,&c.hash_provider,id)==STN_DATA_OK);
    memcpy(child,genesis,364);memcpy(child+40,id,32);child[79]=1;memcpy(old,child,364);
    blocks[0].bytes=genesis;blocks[0].length=364;blocks[1].bytes=child;blocks[1].length=364;
    node_service.chain=&c;node_service.blocks=blocks;node_service.count=2;node_service.intelligence=&v;
    service.user=&node_service;service.handle=stn_node_service_handle;
    r=call(STN_RPC_INFO,NULL,0,STN_RPC_READ,&service);
    CHECK(r.code==STN_RPC_OK && r.length==184 && r.payload[71]==1 && r.payload[143]==4 && r.payload[183]==2);
    CHECK(memcmp(r.payload,c.network_id,32)==0 && memcmp(r.payload+32,id,32)==0);
    height[7]=1;r=call(STN_RPC_BLOCK_HEIGHT,height,8,STN_RPC_READ,&service);
    CHECK(r.code==STN_RPC_OK && r.length==364 && memcmp(r.payload,child,364)==0);
    r=call(STN_RPC_BLOCK_ID,id,32,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_OK && memcmp(r.payload,genesis,364)==0);
    memset(height,255,8);r=call(STN_RPC_BLOCK_HEIGHT,height,8,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_NOT_FOUND);
    memset(id,255,32);r=call(STN_RPC_BLOCK_ID,id,32,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_NOT_FOUND);
    r=call(STN_RPC_MINING_CONTEXT,NULL,0,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_OK && r.length==76 && r.payload[75]==0 && memcmp(r.payload+32,policy.fixed_target,32)==0);
    memcpy(id,r.payload,32);r=call(STN_RPC_CHECK_WORK_BASE,id,32,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_OK);
    id[0]^=1;r=call(STN_RPC_CHECK_WORK_BASE,id,32,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_STALE);id[0]^=1;
    memcpy(work,id,32);work[66]=1;work[67]=108;memcpy(work+68,child,364);
    r=call(STN_RPC_SUBMIT_WORK,work,sizeof(work),STN_RPC_READ,&service);CHECK(r.code==STN_RPC_FORBIDDEN);
    r=call(STN_RPC_SUBMIT_WORK,work,sizeof(work),STN_RPC_SUBMISSION,&service);CHECK(r.code==STN_RPC_UNAVAILABLE);
    work[0]^=1;r=call(STN_RPC_SUBMIT_WORK,work,sizeof(work),STN_RPC_SUBMISSION,&service);CHECK(r.code==STN_RPC_STALE);
    r=call(STN_RPC_MINING_TEMPLATE,NULL,0,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_UNAVAILABLE);
    r=call(STN_RPC_ADMIN_CONTROL,NULL,0,STN_RPC_ADMIN,&service);CHECK(r.code==STN_RPC_UNAVAILABLE);
    r=call(STN_RPC_INTELLIGENCE_ID,id,32,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_UNAVAILABLE);
    r=call(STN_RPC_INTELLIGENCE_CURSOR,work,40,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_UNAVAILABLE);
    v.expected_network[0]=1;v.time_configured=1;v.validation_time=20;
    r=call(STN_RPC_CHECK_INTELLIGENCE,intel,232,STN_RPC_READ,&service);
    CHECK(r.code==STN_RPC_OK && r.length==20 && r.payload[9]==STN_STAGE_UNRESOLVED && r.payload[15]==STN_ACCEPTANCE_UNRESOLVED);
    v.verify_signature=signature;v.lookup_authority=authority;v.check_replay=replay;
    r=call(STN_RPC_CHECK_INTELLIGENCE,intel,232,STN_RPC_READ,&service);
    CHECK(r.code==STN_RPC_OK && r.payload[15]==STN_ACCEPTANCE_UNDER_CONTEXT && signature_calls==1);
    r=call(STN_RPC_SUBMIT_INTELLIGENCE,intel,232,STN_RPC_SUBMISSION,&service);
    CHECK(r.code==STN_RPC_UNAVAILABLE && signature_calls==2); /* Validated, never admitted. */
    memcpy(bad,intel,232);bad[8]=2;
    r=call(STN_RPC_SUBMIT_INTELLIGENCE,bad,232,STN_RPC_SUBMISSION,&service);CHECK(r.code==STN_RPC_REJECTED && signature_calls==2);
    bad[0]=0;r=call(STN_RPC_SUBMIT_INTELLIGENCE,bad,232,STN_RPC_SUBMISSION,&service);CHECK(r.code==STN_RPC_REJECTED);
    CHECK(memcmp(old,child,364)==0 && node_service.count==2);
    child[363]^=1;r=call(STN_RPC_INFO,NULL,0,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_REJECTED);child[363]^=1;
    c.hash_provider.hash=NULL;r=call(STN_RPC_INFO,NULL,0,STN_RPC_READ,&service);CHECK(r.code==STN_RPC_UNAVAILABLE);
    CHECK(memcmp(old,child,364)==0);
}
int test_rpc(void);
int test_rpc(void)
{
    codecs();node();printf("RPC/node interface: %u checks, %u failures (in-process integration).\n",checks,failures);
    return failures==0 ? 0 : 1;
}
