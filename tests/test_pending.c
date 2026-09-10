/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "stn_mining.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do {++checks;if(!(e)){++failures;fprintf(stderr,"pending line %d: %s\n",__LINE__,#e);}} while(0)
#define CAP 65536u
static uint8_t disk[CAP],snapshot[CAP],current[CAP],next[CAP],body[STN_BLOCK_MAX_BODY],templ[STN_BLOCK_MAX_SIZE];
static uint8_t request[STN_RPC_MAX_FRAME],response[STN_RPC_MAX_FRAME],work[8192],other[8192],saved[CAP];
static size_t disk_size;static int disk_fail;
static stn_pending first,second;
static uint8_t long_blocks[71][364];
static stn_block_span long_history[71];
typedef struct validation_modes {stn_stage_status signature,authority,replay;} validation_modes;
/* These are deterministic orchestration hooks, NOT cryptographic providers. */
static stn_stage_status signature(void *u,const uint8_t *d,size_t dn,const uint8_t *b,size_t n,const uint8_t key[32],const uint8_t sig[64])
{(void)d;(void)dn;(void)b;(void)n;(void)key;(void)sig;return ((validation_modes*)u)->signature;}
static stn_stage_status authority(void *u,const stn_record *r,const stn_intelligence *i)
{(void)r;(void)i;return ((validation_modes*)u)->authority;}
static stn_stage_status replay(void *u,const stn_record *r)
{(void)r;return ((validation_modes*)u)->replay;}
static stn_storage_status acquire(void *u){(void)u;return STN_STORAGE_OK;}
static void release(void *u){(void)u;}
static stn_storage_status read_disk(void *u,uint8_t *p,size_t cap,size_t *n)
{(void)u;*n=disk_size;if(!disk_size){return STN_STORAGE_NOT_FOUND;}if(cap<disk_size){return STN_STORAGE_CAPACITY;}memcpy(p,disk,disk_size);return STN_STORAGE_OK;}
static stn_storage_status replace(void *u,const uint8_t *p,size_t n)
{(void)u;if(disk_fail){return STN_STORAGE_IO;}if(n>CAP){return STN_STORAGE_CAPACITY;}memcpy(disk,p,n);disk_size=n;return STN_STORAGE_OK;}
static void record(uint8_t r[232],unsigned nonce)
{
    static const uint8_t base[232]={[0]='S',[1]='T',[2]='N',[3]='R',[5]=1,[7]=1,[8]=1,[40]=2,[72]=3,[111]=10,[115]=52,[117]=1,[125]=5,[126]=1,[128]=1,[129]='a',[131]=1,[132]='b',[134]=1,[135]='c',[136]=1,[168]=4};
    memcpy(r,base,232);r[103]=(uint8_t)nonce;
}
static void genesis(uint8_t p[364])
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(p,0,364);memcpy(p,"STNB",4);p[5]=3;p[8]=1;p[163]=1;p[167]=196;p[171]=192;
    memcpy(p+172,"STNT",4);p[177]=1;p[179]=1;p[183]=180;memcpy(p+184,"STNR",4);
    p[189]=1;p[191]=1;p[192]=1;p[256]=3;memcpy(p+88,commitment,32);
    memset(p+120,255,32);p[120]=127;
}
static stn_data_status admission_hash_error(void *u,const uint8_t *domain,size_t dn,
    const uint8_t *bytes,size_t n,uint8_t digest[32])
{
    (void)u;(void)domain;(void)dn;(void)bytes;(void)n;(void)digest;
    return STN_DATA_PROVIDER_ERROR;
}
static void admission_checks(void)
{
    stn_pending pool={0},before;
    stn_hash_provider hash={stn_sha256,NULL},bad_hash={admission_hash_error,NULL};
    stn_validation_context context={0};stn_validation_report report;
    validation_modes modes={STN_STAGE_PASS,STN_STAGE_PASS,STN_STAGE_PASS};
    uint8_t raw[232],encoded[244],original[244],copy[244],id[32],expected[32],anchor[364];
    stn_transaction tx={1,STN_TX_PUBLICATION,raw,sizeof(raw)};
    stn_block_span block;stn_storage_view active={0};
    size_t n,w,i;unsigned initial_checks=checks,initial_failures=failures;
    genesis(anchor);block.bytes=anchor;block.length=sizeof(anchor);
    active.blocks=&block;active.count=1;active.state.network_id[0]=1;
    context.expected_network[0]=1;context.time_configured=1;context.validation_time=10;
    context.verify_signature=signature;context.signature_user=&modes;
    context.lookup_authority=authority;context.authority_user=&modes;
    context.check_replay=replay;context.replay_user=&modes;
    record(raw,1);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    memcpy(original,encoded,n);
    CHECK(stn_transaction_id(encoded,n,&hash,expected)==STN_DATA_OK);
    CHECK(stn_pending_admit_transaction(&pool,encoded,n,&context,&active,&hash,&report,id)==STN_PENDING_ACCEPTED && pool.count==1 && report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(memcmp(id,expected,32)==0);
    memset(encoded,0,sizeof(encoded));memset(raw,0,sizeof(raw));
    CHECK(stn_pending_lookup(&pool,id,copy,sizeof(copy),&w)==STN_PENDING_ACCEPTED && w==sizeof(original) && memcmp(copy,original,w)==0);
    memcpy(encoded,original,sizeof(encoded));before=pool;
#define ADMISSION_REJECT(result) do { \
    CHECK(stn_pending_admit_transaction(&pool,encoded,sizeof(encoded),&context,&active,&hash,&report,id)==(result)); \
    CHECK(memcmp(&pool,&before,sizeof(pool))==0); \
} while(0)
    ADMISSION_REJECT(STN_PENDING_DUPLICATE);
    encoded[0]=0;ADMISSION_REJECT(STN_PENDING_INVALID);memcpy(encoded,original,sizeof(encoded));
    encoded[5]=2;ADMISSION_REJECT(STN_PENDING_UNSUPPORTED);memcpy(encoded,original,sizeof(encoded));
    encoded[7]=2;ADMISSION_REJECT(STN_PENDING_UNSUPPORTED);memcpy(encoded,original,sizeof(encoded));
    encoded[17]=2;ADMISSION_REJECT(STN_PENDING_UNSUPPORTED);memcpy(encoded,original,sizeof(encoded));
    encoded[19]=2;ADMISSION_REJECT(STN_PENDING_UNSUPPORTED);memcpy(encoded,original,sizeof(encoded));
    encoded[20]=2;ADMISSION_REJECT(STN_PENDING_NETWORK);memcpy(encoded,original,sizeof(encoded));
    encoded[123]=11;ADMISSION_REJECT(STN_PENDING_TIME);memcpy(encoded,original,sizeof(encoded));
    encoded[129]=2;ADMISSION_REJECT(STN_PENDING_UNSUPPORTED);memcpy(encoded,original,sizeof(encoded));
    encoded[138]=255;ADMISSION_REJECT(STN_PENDING_INVALID);memcpy(encoded,original,sizeof(encoded));
    modes.signature=STN_STAGE_REJECT;ADMISSION_REJECT(STN_PENDING_SIGNATURE);modes.signature=STN_STAGE_PASS;
    modes.authority=STN_STAGE_REJECT;ADMISSION_REJECT(STN_PENDING_AUTHORITY);modes.authority=STN_STAGE_PASS;
    modes.replay=STN_STAGE_REJECT;ADMISSION_REJECT(STN_PENDING_REPLAY);modes.replay=STN_STAGE_PASS;
    modes.signature=STN_STAGE_ERROR;ADMISSION_REJECT(STN_PENDING_PROVIDER);modes.signature=STN_STAGE_PASS;
    context.verify_signature=NULL;ADMISSION_REJECT(STN_PENDING_UNAVAILABLE);context.verify_signature=signature;
    context.lookup_authority=NULL;ADMISSION_REJECT(STN_PENDING_UNAVAILABLE);context.lookup_authority=authority;
    context.check_replay=NULL;ADMISSION_REJECT(STN_PENDING_UNAVAILABLE);context.check_replay=replay;
    context.time_configured=0;ADMISSION_REJECT(STN_PENDING_UNAVAILABLE);context.time_configured=1;
    active.state.network_id[0]=2;ADMISSION_REJECT(STN_PENDING_UNAVAILABLE);active.state.network_id[0]=1;
    CHECK(stn_pending_admit_transaction(&pool,encoded,sizeof(encoded),NULL,&active,&hash,&report,id)==STN_PENDING_UNAVAILABLE && memcmp(&pool,&before,sizeof(pool))==0);
    CHECK(stn_pending_admit_transaction(&pool,encoded,sizeof(encoded),&context,&active,&bad_hash,&report,id)==STN_PENDING_PROVIDER && memcmp(&pool,&before,sizeof(pool))==0);
    CHECK(stn_pending_admit_transaction(&pool,encoded,SIZE_MAX,&context,&active,&hash,&report,id)==STN_PENDING_INVALID && memcmp(&pool,&before,sizeof(pool))==0);
    /* Match the genesis signer/nonce with different publication bytes. The
     * all-PASS replay hook cannot override the active-history replay guard. */
    memset(encoded+52,0,32);encoded[115]=0;ADMISSION_REJECT(STN_PENDING_REPLAY);
    memcpy(encoded,original,sizeof(encoded));encoded[243]^=1;ADMISSION_REJECT(STN_PENDING_REPLAY);
    for(i=2;i<=STN_PENDING_MAX_ENTRIES;++i){
        record(raw,(unsigned)i);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
        CHECK(stn_pending_admit_transaction(&pool,encoded,n,&context,&active,&hash,&report,id)==STN_PENDING_ACCEPTED && pool.count==i);
    }
    before=pool;memcpy(encoded,original,sizeof(encoded));ADMISSION_REJECT(STN_PENDING_DUPLICATE);
    record(raw,129);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    ADMISSION_REJECT(STN_PENDING_CAPACITY);
    CHECK(stn_pending_lookup(&pool,expected,copy,sizeof(copy),&w)==STN_PENDING_ACCEPTED && memcmp(copy,original,w)==0);
    stn_pending_clear(&pool);CHECK(pool.count==0 && pool.bytes==0);
#undef ADMISSION_REJECT
    printf("Pending admission: %u checks, %u failures (scripted validation hooks, real SHA-256).\n",checks-initial_checks,failures-initial_failures);
}
static void store_foundation(void)
{
    stn_pending a,b;
    stn_hash_provider hash={stn_sha256,NULL};
    uint8_t raw[232],encoded[244],copy[244],expected[244],id[32],missing[32]={0};
    uint8_t ids[STN_PENDING_MAX_ENTRIES][32],reverse[STN_PENDING_MAX_ENTRIES][32],page[3][32];
    static uint8_t large[STN_TX_MAX_SIZE];
    stn_transaction tx={1,STN_TX_PUBLICATION,raw,sizeof(raw)};
    size_t i,n,w;unsigned initial_checks=checks,initial_failures=failures;
    stn_pending_init(&a);stn_pending_init(&b);
    CHECK(stn_pending_count(&a)==0 && a.bytes==0);
    CHECK(stn_pending_enumerate(&a,0,ids,STN_PENDING_MAX_ENTRIES,&w)==STN_PENDING_ACCEPTED && w==0);
    CHECK(stn_pending_lookup(&a,missing,copy,sizeof(copy),&w)==STN_PENDING_NOT_FOUND && w==0);
    record(raw,1);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    memcpy(expected,encoded,n);
    CHECK(stn_pending_insert(&a,encoded,n,&hash,id)==STN_PENDING_ACCEPTED && stn_pending_count(&a)==1);
    CHECK(stn_transaction_id(expected,n,&hash,missing)==STN_DATA_OK && memcmp(id,missing,32)==0);
    memset(missing,0,32);memset(encoded,0,sizeof(encoded));memset(raw,0,sizeof(raw));
    CHECK(stn_pending_lookup(&a,id,copy,sizeof(copy),&w)==STN_PENDING_ACCEPTED && w==n && memcmp(copy,expected,n)==0);
    memset(copy,0xA5,sizeof(copy));
    CHECK(stn_pending_lookup(&a,id,copy,1,&w)==STN_PENDING_CAPACITY && w==0 && copy[0]==0xA5);
    CHECK(stn_pending_insert(&a,expected,n,&hash,id)==STN_PENDING_DUPLICATE && stn_pending_count(&a)==1);
    CHECK(stn_pending_insert(&a,encoded,n,&hash,id)==STN_PENDING_INVALID && stn_pending_count(&a)==1);
    CHECK(stn_pending_insert(&a,expected,n,NULL,id)==STN_PENDING_PROVIDER && stn_pending_count(&a)==1);
    for(i=2;i<=STN_PENDING_MAX_ENTRIES;++i){
        record(raw,(unsigned)i);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
        CHECK(stn_pending_insert(&a,encoded,n,&hash,id)==STN_PENDING_ACCEPTED && stn_pending_count(&a)==i);
    }
    CHECK(stn_pending_enumerate(&a,0,ids,STN_PENDING_MAX_ENTRIES,&w)==STN_PENDING_ACCEPTED && w==STN_PENDING_MAX_ENTRIES);
    for(i=0;i<STN_PENDING_MAX_ENTRIES;++i){
        CHECK(stn_pending_lookup(&a,ids[i],copy,sizeof(copy),&w)==STN_PENDING_ACCEPTED && w==sizeof(copy));
        if(i!=0){CHECK(memcmp(ids[i-1],ids[i],32)<0);}
    }
    for(i=STN_PENDING_MAX_ENTRIES;i!=0;--i){
        record(raw,(unsigned)i);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
        CHECK(stn_pending_insert(&b,encoded,n,&hash,id)==STN_PENDING_ACCEPTED);
    }
    CHECK(stn_pending_enumerate(&b,0,reverse,STN_PENDING_MAX_ENTRIES,&w)==STN_PENDING_ACCEPTED && w==STN_PENDING_MAX_ENTRIES && memcmp(ids,reverse,sizeof(ids))==0);
    CHECK(stn_pending_enumerate(&b,4,page,3,&w)==STN_PENDING_ACCEPTED && w==3 && memcmp(page,ids+4,sizeof(page))==0);
    CHECK(stn_pending_enumerate(&b,SIZE_MAX,page,3,&w)==STN_PENDING_ACCEPTED && w==0);
    CHECK(stn_pending_enumerate(&b,0,NULL,0,&w)==STN_PENDING_ACCEPTED && w==0);
    record(raw,129);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    CHECK(stn_pending_insert(&a,encoded,n,&hash,id)==STN_PENDING_CAPACITY && a.count==STN_PENDING_MAX_ENTRIES);
    CHECK(stn_pending_insert(&a,expected,sizeof(expected),&hash,id)==STN_PENDING_DUPLICATE);
    CHECK(stn_pending_enumerate(&a,0,reverse,STN_PENDING_MAX_ENTRIES,&w)==STN_PENDING_ACCEPTED && memcmp(ids,reverse,sizeof(ids))==0);
    for(i=0;i<STN_PENDING_MAX_ENTRIES;++i){
        CHECK(stn_pending_lookup(&a,ids[i],copy,sizeof(copy),&w)==STN_PENDING_ACCEPTED && w==sizeof(copy));
        CHECK(stn_transaction_id(copy,w,&hash,id)==STN_DATA_OK && memcmp(id,ids[i],32)==0);
    }
    CHECK(stn_pending_remove(&a,missing)==STN_PENDING_NOT_FOUND && a.count==STN_PENDING_MAX_ENTRIES);
    CHECK(stn_pending_remove(&a,ids[64])==STN_PENDING_ACCEPTED && a.count==STN_PENDING_MAX_ENTRIES-1);
    CHECK(stn_pending_lookup(&a,ids[64],copy,sizeof(copy),&w)==STN_PENDING_NOT_FOUND && w==0);
    CHECK(stn_pending_remove(&a,ids[64])==STN_PENDING_NOT_FOUND);
    CHECK(stn_pending_insert(&a,encoded,n,&hash,id)==STN_PENDING_ACCEPTED && a.count==STN_PENDING_MAX_ENTRIES);
    CHECK(stn_pending_remove(&b,ids[0])==STN_PENDING_ACCEPTED);
    CHECK(stn_pending_remove(&b,ids[STN_PENDING_MAX_ENTRIES-1])==STN_PENDING_ACCEPTED);
    CHECK(stn_pending_enumerate(&b,0,reverse,STN_PENDING_MAX_ENTRIES,&w)==STN_PENDING_ACCEPTED && w==STN_PENDING_MAX_ENTRIES-2 && memcmp(reverse,ids+1,w*32)==0);
    CHECK(stn_pending_lookup(&a,id,copy,sizeof(copy),&w)==STN_PENDING_ACCEPTED);
    stn_pending_clear(&a);stn_pending_clear(&b);
    CHECK(a.count==0 && a.bytes==0 && b.count==0 && b.bytes==0 && memcmp(copy,encoded,n)==0);
    stn_pending_clear(&a);CHECK(stn_pending_count(&a)==0);
    /* Maximal structurally canonical record, not an authenticated publication. */
    memset(large,0,sizeof(large));memcpy(large,"STNT",4);large[5]=1;large[7]=1;
    large[9]=1;large[11]=180;memcpy(large+12,"STNR",4);large[17]=1;large[19]=1;
    large[20]=1;large[84]=3;large[125]=1;
    CHECK(stn_transaction_validate_structure(large,sizeof(large))==STN_DATA_OK);
    for(i=0;i<3;++i){large[115]=(uint8_t)i;CHECK(stn_pending_insert(&a,large,sizeof(large),&hash,id)==STN_PENDING_ACCEPTED);}
    CHECK(stn_pending_enumerate(&a,0,ids,3,&w)==STN_PENDING_ACCEPTED && w==3);
    large[115]=3;
    CHECK(stn_pending_insert(&a,large,sizeof(large),&hash,id)==STN_PENDING_CAPACITY && a.count==3 && a.bytes==3*sizeof(large));
    CHECK(stn_pending_enumerate(&a,0,reverse,3,&w)==STN_PENDING_ACCEPTED && w==3 && memcmp(ids,reverse,3*32)==0);
    CHECK(stn_pending_remove(&a,ids[1])==STN_PENDING_ACCEPTED);
    CHECK(stn_pending_insert(&a,large,sizeof(large),&hash,id)==STN_PENDING_ACCEPTED && a.count==3);
    stn_pending_clear(&a);CHECK(a.count==0 && a.bytes==0);
    CHECK(stn_pending_insert(&a,expected,sizeof(expected),&hash,id)==STN_PENDING_ACCEPTED);
    CHECK(stn_pending_remove(&a,id)==STN_PENDING_ACCEPTED && a.count==0 && a.bytes==0);
    stn_pending_clear(&a);
    printf("Pending store foundation: %u checks, %u failures.\n",checks-initial_checks,failures-initial_failures);
}
static stn_rpc_message call(stn_mining_service *s,uint16_t method,const uint8_t *payload,size_t n)
{
    stn_rpc_message q={1,0,STN_RPC_OK,7,NULL,0},r={0};size_t rn,wn;stn_rpc_service service={s,stn_mining_handle};
    q.method=method;q.payload=payload;q.length=n;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
    CHECK(stn_rpc_dispatch(request,rn,3,&service,response,sizeof(response),&wn)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,wn,&r)==STN_RPC_OK && r.request_id==7);return r;
}
static void rejected(stn_mining_service *s,uint8_t bytes[232],stn_pending_result expected)
{
    size_t count=s->pending->count;stn_rpc_message r=call(s,STN_RPC_SUBMIT_INTELLIGENCE,bytes,232);
    CHECK(r.code==STN_RPC_OK && r.length==56 && r.payload[3]==expected && s->pending->count==count);
}
typedef struct submission_client {
    CRITICAL_SECTION *lock;HANDLE start;stn_rpc_service service;
    uint8_t frame[268];size_t length;int result;
} submission_client;
static DWORD WINAPI submit_client(void *user)
{
    submission_client *client=user;uint8_t reply[128];size_t n=0;stn_rpc_message r;
    (void)WaitForSingleObject(client->start,INFINITE);
    EnterCriticalSection(client->lock);
    client->result=-1;
    if(stn_rpc_dispatch(client->frame,client->length,STN_RPC_SUBMISSION,&client->service,reply,sizeof(reply),&n)==STN_RPC_OK &&
       stn_rpc_decode(reply,n,&r)==STN_RPC_OK){
        client->result=r.code==STN_RPC_INVALID ? 100 :
            r.code==STN_RPC_OK && r.length==36 ? r.payload[3] : -1;
    }
    LeaveCriticalSection(client->lock);return 0;
}
static void rpc_admission_checks(stn_mining_service *s,validation_modes *modes)
{
    uint8_t raw[232],encoded[244],original[244],id[32],zero[32]={0};
    stn_transaction tx={1,STN_TX_PUBLICATION,raw,sizeof(raw)};
    stn_rpc_message r,q={1,STN_RPC_SUBMIT_TRANSACTION,STN_RPC_OK,19,NULL,0};
    stn_rpc_service service={s,stn_mining_handle};
    stn_pending before;size_t i,n,w,old_disk=disk_size;
    unsigned initial_checks=checks,initial_failures=failures;
    record(raw,1);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    memcpy(original,encoded,n);CHECK(stn_transaction_id(encoded,n,&s->chain->hash_provider,id)==STN_DATA_OK);
    r=call(s,STN_RPC_SUBMIT_TRANSACTION,encoded,n);
    CHECK(r.code==STN_RPC_OK && r.length==36 && r.payload[1]==1 && r.payload[3]==0 && memcmp(r.payload+4,id,32)==0 && s->pending->count==1);
    before=*s->pending;
#define RPC_REJECT(value) do { \
    r=call(s,STN_RPC_SUBMIT_TRANSACTION,encoded,sizeof(encoded)); \
    CHECK(r.code==STN_RPC_OK && r.length==36 && r.payload[3]==(value)); \
    CHECK(memcmp(s->pending,&before,sizeof(before))==0); \
    if((value)!=1){CHECK(memcmp(r.payload+4,zero,32)==0);} \
} while(0)
    RPC_REJECT(1);CHECK(memcmp(r.payload+4,id,32)==0);
    encoded[0]=0;RPC_REJECT(3);memcpy(encoded,original,sizeof(encoded));
    encoded[5]=2;RPC_REJECT(4);memcpy(encoded,original,sizeof(encoded));
    encoded[20]=2;RPC_REJECT(3);memcpy(encoded,original,sizeof(encoded));
    modes->signature=STN_STAGE_REJECT;RPC_REJECT(6);modes->signature=STN_STAGE_PASS;
    modes->authority=STN_STAGE_REJECT;RPC_REJECT(6);modes->authority=STN_STAGE_PASS;
    modes->signature=STN_STAGE_UNRESOLVED;RPC_REJECT(7);modes->signature=STN_STAGE_PASS;
    modes->signature=STN_STAGE_ERROR;RPC_REJECT(8);modes->signature=STN_STAGE_PASS;
    memset(encoded+52,0,32);encoded[115]=0;RPC_REJECT(5);memcpy(encoded,original,sizeof(encoded));
    r=call(s,STN_RPC_SUBMIT_TRANSACTION,NULL,0);CHECK(r.code==STN_RPC_OK && r.payload[3]==3 && s->pending->count==1);
    r=call(s,STN_RPC_PENDING,NULL,0);
    CHECK(r.code==STN_RPC_OK && r.length==16 && r.payload[3]==1 && r.payload[7]==128 && r.payload[11]==244 && r.payload[13]==4);
    q.payload=encoded;q.length=sizeof(encoded);
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK);
    CHECK(stn_rpc_dispatch(request,n,STN_RPC_READ,&service,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&r)==STN_RPC_OK && r.code==STN_RPC_FORBIDDEN && s->pending->count==1);
    /* A method-oversized frame within the global RPC bound is rejected before
     * admission; neither this nor a short response buffer can mutate state. */
    n=STN_TX_MAX_SIZE+1u;request[20]=(uint8_t)(n>>24);request[21]=(uint8_t)(n>>16);
    request[22]=(uint8_t)(n>>8);request[23]=(uint8_t)n;
    memset(request+24,0,n);
    CHECK(stn_rpc_dispatch(request,n+24,STN_RPC_SUBMISSION,&service,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&r)==STN_RPC_OK && r.code==STN_RPC_INVALID && memcmp(s->pending,&before,sizeof(before))==0);
    record(raw,2);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK);
    CHECK(stn_rpc_dispatch(request,n,STN_RPC_SUBMISSION,&service,response,24+35,&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&r)==STN_RPC_OK && r.code==STN_RPC_CAPACITY && memcmp(s->pending,&before,sizeof(before))==0);
    for(i=2;i<=STN_PENDING_MAX_ENTRIES;++i){
        record(raw,(unsigned)i);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
        r=call(s,STN_RPC_SUBMIT_TRANSACTION,encoded,n);CHECK(r.code==STN_RPC_OK && r.payload[3]==0 && s->pending->count==i);
    }
    before=*s->pending;memcpy(encoded,original,sizeof(encoded));RPC_REJECT(1);
    record(raw,129);CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);RPC_REJECT(2);
    stn_pending_clear(s->pending);
    {
        submission_client clients[12];HANDLE threads[12]={0},start=CreateEventW(NULL,TRUE,FALSE,NULL);
        CRITICAL_SECTION lock;unsigned accepted=0,duplicates=0,invalid=0;
        CHECK(start!=NULL);InitializeCriticalSection(&lock);
        memcpy(encoded,original,sizeof(encoded));
        for(i=0;i<12 && start!=NULL;++i){
            clients[i].lock=&lock;clients[i].start=start;clients[i].service=service;clients[i].result=-1;
            CHECK(stn_rpc_encode(&q,clients[i].frame,sizeof(clients[i].frame),&clients[i].length)==STN_RPC_OK);
            if(i==0){clients[i].frame[0]=0;}
            threads[i]=CreateThread(NULL,0,submit_client,&clients[i],0,NULL);CHECK(threads[i]!=NULL);
        }
        if(start!=NULL){(void)SetEvent(start);}
        for(i=0;i<12;++i){if(threads[i]!=NULL){
            CHECK(WaitForSingleObject(threads[i],INFINITE)==WAIT_OBJECT_0);CloseHandle(threads[i]);
            if(clients[i].result==0){++accepted;}else if(clients[i].result==1){++duplicates;}else if(clients[i].result==100){++invalid;}
        }}
        CHECK(accepted==1 && duplicates==10 && invalid==1 && s->pending->count==1 && s->pending->bytes==244);
        if(start!=NULL){CloseHandle(start);}DeleteCriticalSection(&lock);
    }
    CHECK(disk_size==old_disk && s->active.height==0);
    stn_pending_clear(s->pending);
#undef RPC_REJECT
    printf("Pending RPC: %u checks, %u failures (serialized concurrent dispatcher clients).\n",checks-initial_checks,failures-initial_failures);
}
static void candidate_checks(stn_mining_service *s,validation_modes *modes)
{
    stn_pending a={0},b={0},before;stn_pending *original_pool=s->pending;
    stn_storage_view active={0};stn_rpc_message r;stn_validation_report report;
    stn_transaction tx;stn_block block;uint8_t raw[232],encoded[244],id[32];
    uint8_t ids[STN_PENDING_MAX_ENTRIES][32],candidate[8192];
    size_t n,w,i,offset,original_capacity=s->pending_body_capacity;uint32_t count;
    unsigned initial_checks=checks,initial_failures=failures;
    s->pending=&a;
    CHECK(stn_storage_load(s->chain,s->storage,s->snapshot,s->snapshot_capacity,&active)==STN_STORAGE_OK);
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_UNAVAILABLE && a.count==0);
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_UNAVAILABLE && a.count==0);
    record(raw,1);tx.version=1;tx.type=STN_TX_PUBLICATION;tx.record_bytes=raw;tx.record_length=sizeof(raw);
    CHECK(stn_transaction_encode(&tx,encoded,sizeof(encoded),&n)==STN_DATA_OK);
    CHECK(stn_pending_admit_transaction(&a,encoded,n,s->intelligence,&active,&s->chain->hash_provider,&report,id)==STN_PENDING_ACCEPTED);
    before=a;r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);
    CHECK(r.code==STN_RPC_OK && r.length==484 && memcmp(&a,&before,sizeof(a))==0);
    CHECK(stn_block_decode(r.payload+68,r.length-68,&block)==STN_DATA_OK && block.header.transaction_count==1 && block.header.reserved_work_nonce==0);
    CHECK(memcmp(block.body+4,encoded,sizeof(encoded))==0);
    for(i=2;i<=17;++i){record(raw,(unsigned)i);CHECK(stn_pending_admit(&a,raw,sizeof(raw),s->intelligence,&active,&s->chain->hash_provider,&report,id)==STN_PENDING_ACCEPTED);}
    before=a;r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);n=r.length;
    CHECK(r.code==STN_RPC_OK && n<=sizeof(candidate));memcpy(candidate,r.payload,n);
    CHECK(stn_block_decode(candidate+68,n-68,&block)==STN_DATA_OK && block.header.transaction_count==STN_BLOCK_MAX_TRANSACTIONS && block.header.body_length<=STN_BLOCK_MAX_BODY);
    CHECK(stn_pending_enumerate(&a,0,ids,STN_PENDING_MAX_ENTRIES,&w)==STN_PENDING_ACCEPTED && w==17);
    offset=0;
    for(i=0;i<block.header.transaction_count;++i){
        CHECK(stn_transaction_decode(block.body+offset+4,244,&tx)==STN_DATA_OK);
        CHECK(stn_transaction_id(block.body+offset+4,244,&s->chain->hash_provider,id)==STN_DATA_OK && memcmp(id,ids[i],32)==0);
        offset+=248;
    }
    CHECK(offset==block.header.body_length && memcmp(&a,&before,sizeof(a))==0);
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==n && memcmp(candidate,r.payload,n)==0 && memcmp(&a,&before,sizeof(a))==0);
    for(i=17;i!=0;--i){record(raw,(unsigned)i);CHECK(stn_pending_admit(&b,raw,sizeof(raw),s->intelligence,&active,&s->chain->hash_provider,&report,id)==STN_PENDING_ACCEPTED);}
    s->pending=&b;r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);
    CHECK(r.code==STN_RPC_OK && r.length==n && memcmp(candidate,r.payload,n)==0 && b.count==17);
    s->pending=&a;s->pending_body_capacity=3967;
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_CAPACITY && memcmp(&a,&before,sizeof(a))==0);
    s->pending_body_capacity=3968;
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==n && memcmp(candidate,r.payload,n)==0);
    s->pending_body_capacity=original_capacity;
    modes->authority=STN_STAGE_UNRESOLVED;r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);
    CHECK(r.code==STN_RPC_UNAVAILABLE && memcmp(&a,&before,sizeof(a))==0);modes->authority=STN_STAGE_PASS;
    encoded[0]=0;r=call(s,STN_RPC_SUBMIT_TRANSACTION,encoded,sizeof(encoded));
    CHECK(r.code==STN_RPC_OK && r.payload[3]==3 && memcmp(&a,&before,sizeof(a))==0);
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==n && memcmp(candidate,r.payload,n)==0);
    CHECK(stn_pending_remove(&a,ids[0])==STN_PENDING_ACCEPTED);before=a;
    r=call(s,STN_RPC_SUBMIT_WORK,candidate,n);CHECK(r.code==STN_RPC_STALE && memcmp(&a,&before,sizeof(a))==0);
    r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && memcmp(candidate+32,r.payload+32,32)!=0);
    stn_pending_clear(&a);stn_pending_clear(&b);
    /* An already-included structural entry is ineligible, but a template read
     * must not perform pending cleanup. No block acceptance or peer scenario. */
    CHECK(stn_pending_insert(&a,active.blocks[0].bytes+172,192,&s->chain->hash_provider,id)==STN_PENDING_ACCEPTED);
    before=a;r=call(s,STN_RPC_MINING_TEMPLATE,NULL,0);
    CHECK(r.code==STN_RPC_UNAVAILABLE && memcmp(&a,&before,sizeof(a))==0);
    CHECK(stn_pending_assemble(&a,NULL,&active,body,sizeof(body),&w,&count)==STN_DATA_UNRESOLVED && w==0 && count==0);
    stn_pending_clear(&a);stn_storage_view_release(&active);s->pending=original_pool;
    printf("Pending candidates: %u checks, %u failures.\n",checks-initial_checks,failures-initial_failures);
}
int test_pending(void)
{
    static const uint8_t expected_id[32]={0x35,0xb6,0x9c,0x1f,0x12,0xa5,0x54,0x3f,0x98,0x9a,0x10,0xe3,0x69,0x8b,0xec,0x1d,0xa5,0x62,0xa2,0xc8,0x7f,0x81,0x35,0x4e,0x16,0xb7,0x91,0x1b,0xf5,0x18,0xd5,0x0b};
    static const uint8_t expected_work[32]={0x1f,0x6d,0xfb,0xae,0x90,0x65,0xb1,0x7a,0x97,0x00,0xf4,0xbb,0x98,0xb4,0x25,0xc8,0x60,0x5f,0x16,0x6b,0x26,0x2a,0x9b,0xab,0xed,0xa4,0xc5,0x3c,0x89,0x91,0x47,0x1b};
    uint8_t anchor[364],bytes[232],id[32],mask[STN_PENDING_MAX_ENTRIES],alt[3][364];
    stn_chain_context c={0};stn_pow_policy policy;stn_validation_context v={0};stn_mining_service s={0};
    stn_storage_provider provider={NULL,acquire,release,read_disk,replace};stn_storage_view active={0};
    stn_block_span history[4];stn_validation_report report;stn_pending before;
    validation_modes modes={STN_STAGE_PASS,STN_STAGE_PASS,STN_STAGE_PASS};
    stn_rpc_message r;size_t n,w,i,old_size;uint32_t count;int all;
    store_foundation();
    admission_checks();
    genesis(anchor);memcpy(policy.fixed_target,anchor+120,32);c.network_id[0]=1;c.genesis_bytes=anchor;
    c.genesis_length=364;c.hash_provider.hash=stn_sha256;c.pow_policy=&policy;
    v.expected_network[0]=1;v.time_configured=1;v.validation_time=10;
    v.verify_signature=signature;v.signature_user=&modes;v.lookup_authority=authority;v.authority_user=&modes;v.check_replay=replay;v.replay_user=&modes;
    s.chain=&c;s.storage=&provider;s.pending=&first;s.intelligence=&v;s.pending_body=body;s.pending_body_capacity=sizeof(body);
    s.snapshot=snapshot;s.snapshot_capacity=CAP;s.workspace.current_bytes=current;s.workspace.current_capacity=CAP;
    s.workspace.next_bytes=next;s.workspace.next_capacity=CAP;s.template_bytes=templ;s.template_capacity=sizeof(templ);
    history[0].bytes=anchor;history[0].length=364;
    CHECK(stn_storage_create(&c,&provider,history,1,next,CAP,&s.active)==STN_STORAGE_OK);
    CHECK(stn_storage_load(&c,&provider,snapshot,CAP,&active)==STN_STORAGE_OK);
    rpc_admission_checks(&s,&modes);
    candidate_checks(&s,&modes);
    r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_UNAVAILABLE);
    record(bytes,1);r=call(&s,STN_RPC_SUBMIT_INTELLIGENCE,bytes,232);
    CHECK(r.code==STN_RPC_OK && r.length==56 && r.payload[3]==STN_PENDING_ACCEPTED && first.count==1);
    CHECK(memcmp(r.payload+24,expected_id,32)==0 && r.payload[13]==STN_STAGE_PASS && r.payload[15]==STN_STAGE_PASS && r.payload[19]==STN_ACCEPTANCE_UNDER_CONTEXT);
    rejected(&s,bytes,STN_PENDING_DUPLICATE);bytes[231]=1;rejected(&s,bytes,STN_PENDING_REPLAY);
    record(bytes,2);bytes[0]=0;rejected(&s,bytes,STN_PENDING_INVALID);record(bytes,2);
    bytes[5]=2;rejected(&s,bytes,STN_PENDING_UNSUPPORTED);record(bytes,2);
    bytes[8]=2;rejected(&s,bytes,STN_PENDING_NETWORK);record(bytes,2);
    bytes[111]=11;rejected(&s,bytes,STN_PENDING_TIME);record(bytes,2);
    modes.signature=STN_STAGE_REJECT;rejected(&s,bytes,STN_PENDING_SIGNATURE);modes.signature=STN_STAGE_PASS;
    modes.authority=STN_STAGE_REJECT;rejected(&s,bytes,STN_PENDING_AUTHORITY);
    modes.authority=STN_STAGE_UNRESOLVED;rejected(&s,bytes,STN_PENDING_UNAVAILABLE);modes.authority=STN_STAGE_PASS;
    modes.replay=STN_STAGE_REJECT;rejected(&s,bytes,STN_PENDING_REPLAY);modes.replay=STN_STAGE_PASS;
    modes.signature=STN_STAGE_ERROR;rejected(&s,bytes,STN_PENDING_PROVIDER);modes.signature=STN_STAGE_PASS;
    s.intelligence=NULL;rejected(&s,bytes,STN_PENDING_UNAVAILABLE);s.intelligence=&v;
    v.verify_signature=NULL;rejected(&s,bytes,STN_PENDING_UNAVAILABLE);v.verify_signature=signature;
    CHECK(stn_pending_admit(&first,bytes,SIZE_MAX,&v,&active,&c.hash_provider,&report,id)==STN_PENDING_INVALID && first.count==1);
    for(i=2;i<=17;++i){record(bytes,(unsigned)i);r=call(&s,STN_RPC_SUBMIT_INTELLIGENCE,bytes,232);CHECK(r.code==STN_RPC_OK && r.payload[3]==STN_PENDING_ACCEPTED);}
    all=1;for(i=17;i!=0;--i){record(bytes,(unsigned)i);if(stn_pending_admit(&second,bytes,232,&v,&active,&c.hash_provider,&report,id)!=STN_PENDING_ACCEPTED){all=0;}}CHECK(all);
    r=call(&s,STN_RPC_PENDING,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==16 && r.payload[3]==17);
    all=1;for(i=0;i<17;++i){if(memcmp(first.entries[i].id,second.entries[i].id,32)!=0 || (i!=0 && memcmp(first.entries[i-1].id,first.entries[i].id,32)>=0)){all=0;}}CHECK(all);
    r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==4204);n=r.length;memcpy(work,r.payload,n);
    CHECK(memcmp(work+32,expected_work,32)==0 && work[68+163]==16 && work[68+159]==0);
    s.pending=&second;r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==n && memcmp(work,r.payload,n)==0);
    CHECK(stn_pending_assemble(&second,&v,&active,other,3967,&w,&count)==STN_DATA_CAPACITY && w==0 && count==0);
    memset(mask,0,sizeof(mask));mask[0]=1;stn_pending_prune(&second,mask);
    r=call(&s,STN_RPC_SUBMIT_WORK,work,n);CHECK(r.code==STN_RPC_STALE && second.count==16);
    stn_pending_clear(&second);r=call(&s,STN_RPC_SUBMIT_WORK,work,n);CHECK(r.code==STN_RPC_STALE);
    s.pending=&first;modes.authority=STN_STAGE_UNRESOLVED;
    r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_UNAVAILABLE && first.count==17);modes.authority=STN_STAGE_PASS;
    memcpy(other,work,n);other[68+159]=1;before=first;old_size=disk_size;memcpy(saved,disk,disk_size);
    {
        uint8_t included_id[32],prepared[STN_PENDING_MAX_ENTRIES],unchanged[STN_PENDING_MAX_ENTRIES];
        stn_hash_provider bad_hash={admission_hash_error,NULL};
        CHECK(stn_pending_insert(&first,anchor+172,192,&c.hash_provider,included_id)==STN_PENDING_ACCEPTED);
        before=first;
        r=call(&s,STN_RPC_SUBMIT_WORK,other,n);
        CHECK(r.code==STN_RPC_REJECTED && memcmp(&first,&before,sizeof(first))==0);
        memset(prepared,0xA5,sizeof(prepared));memcpy(unchanged,prepared,sizeof(prepared));
        CHECK(stn_pending_inclusions(&first,&active,&bad_hash,prepared)==STN_DATA_PROVIDER_ERROR && memcmp(prepared,unchanged,sizeof(prepared))==0 && memcmp(&first,&before,sizeof(first))==0);
        CHECK(stn_pending_remove(&first,included_id)==STN_PENDING_ACCEPTED);
        before=first;
    }
    r=call(&s,STN_RPC_SUBMIT_WORK,other,n);CHECK(r.code==STN_RPC_REJECTED && memcmp(&first,&before,sizeof(first))==0);
    disk_fail=1;r=call(&s,STN_RPC_SUBMIT_WORK,work,n);CHECK(r.code==STN_RPC_PROVIDER && memcmp(&first,&before,sizeof(first))==0 && disk_size==old_size && memcmp(saved,disk,disk_size)==0);disk_fail=0;
    r=call(&s,STN_RPC_SUBMIT_WORK,work,n);CHECK(r.code==STN_RPC_OK && s.active.height==1 && first.count==1 && first.entries[0].nonce[31]==11);
    CHECK(first.bytes==244 && first.entries[0].length==244);
    record(bytes,1);rejected(&s,bytes,STN_PENDING_REPLAY);
    stn_storage_view_release(&active);CHECK(stn_storage_load(&c,&provider,snapshot,CAP,&active)==STN_STORAGE_OK && active.count==2);
    r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==484);w=r.length;memcpy(other,r.payload,w);other[68+159]=1;
    history[1].bytes=work+68;history[1].length=n-68;history[2].bytes=other+68;history[2].length=w-68;
    CHECK(stn_storage_adopt(&c,&provider,history,3,&s.workspace,&s.active)==STN_STORAGE_OK && first.count==1);
    r=call(&s,STN_RPC_PENDING,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==16 && first.count==0);
    record(bytes,18);r=call(&s,STN_RPC_SUBMIT_INTELLIGENCE,bytes,232);CHECK(r.code==STN_RPC_OK && r.payload[3]==STN_PENDING_ACCEPTED);
    /* A real-SHA256 competing branch wins; unrelated pending survives and
     * detached publications are not automatically resurrected. */
    memcpy(alt[0],anchor,364);CHECK(stn_chain_block_id(anchor,364,&c.hash_provider,alt[0]+40)==STN_DATA_OK);
    alt[0][79]=1;alt[0][87]=1;alt[0][159]=1;
    for(i=1;i<3;++i){memcpy(alt[i],alt[i-1],364);alt[i][79]=(uint8_t)(i+1);alt[i][159]=0;CHECK(stn_chain_block_id(alt[i-1],364,&c.hash_provider,alt[i]+40)==STN_DATA_OK);}
    for(i=0;i<3;++i){history[i+1].bytes=alt[i];history[i+1].length=364;}
    CHECK(stn_storage_adopt(&c,&provider,history,4,&s.workspace,&s.active)==STN_STORAGE_OK);
    r=call(&s,STN_RPC_PENDING,NULL,0);CHECK(r.code==STN_RPC_OK && first.count==1 && first.entries[0].nonce[31]==18);
    r=call(&s,STN_RPC_SUBMIT_WORK,work,n);CHECK(r.code==STN_RPC_STALE && first.count==1);
    stn_storage_view_release(&active);CHECK(stn_storage_load(&c,&provider,snapshot,CAP,&active)==STN_STORAGE_OK);
    all=1;for(i=1;i<=STN_PENDING_MAX_ENTRIES;++i){record(bytes,(unsigned)i);if(stn_pending_admit(&second,bytes,232,&v,&active,&c.hash_provider,&report,id)!=STN_PENDING_ACCEPTED){all=0;}}CHECK(all && second.count==STN_PENDING_MAX_ENTRIES);
    s.pending=&second;r=call(&s,STN_RPC_PENDING,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==16);s.pending=&first;
    record(bytes,129);CHECK(stn_pending_admit(&second,bytes,232,&v,&active,&c.hash_provider,&report,id)==STN_PENDING_CAPACITY && second.count==STN_PENDING_MAX_ENTRIES);
    old_size=disk_size;memcpy(saved,disk,disk_size);stn_pending_clear(&first);stn_pending_clear(&second);
    CHECK(first.count==0 && first.bytes==0 && disk_size==old_size && memcmp(saved,disk,disk_size)==0);
    r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_UNAVAILABLE && s.active.height==3);
    stn_storage_view_release(&active);
    {
        static const uint8_t nonces[70]={0,0,0,2,1,3,3,6,2,0,0,0,0,1,0,2,2,1,0,0,0,0,0,5,0,1,1,3,0,0,3,0,0,4,2,2,1,0,1,0,0,1,0,0,1,4,0,0,3,1,0,0,0,1,1,1,5,0,0,0,2,3,0,0,0,0,0,1,0,1};
        memcpy(long_blocks[0],anchor,364);long_history[0].bytes=long_blocks[0];long_history[0].length=364;
        all=1;
        for(i=1;i<71;++i){
            memcpy(long_blocks[i],long_blocks[i-1],364);long_blocks[i][79]=(uint8_t)i;long_blocks[i][159]=nonces[i-1];
            if(stn_chain_block_id(long_blocks[i-1],364,&c.hash_provider,long_blocks[i]+40)!=STN_DATA_OK){all=0;}
            if(i>=60){
                unsigned nonce;uint8_t digest[32];long_blocks[i][120]=31;
                for(nonce=0;nonce<65536;++nonce){long_blocks[i][158]=(uint8_t)(nonce>>8);long_blocks[i][159]=(uint8_t)nonce;
                    if(stn_pow_verify(long_blocks[i],364,&c.hash_provider,digest)==STN_DATA_OK){break;}}
                if(nonce==65536){all=0;}
            }
            long_history[i].bytes=long_blocks[i];long_history[i].length=364;
        }
        CHECK(all && stn_storage_adopt(&c,&provider,long_history,71,&s.workspace,&s.active)==STN_STORAGE_OK && s.active.height==70);
        record(bytes,1);r=call(&s,STN_RPC_SUBMIT_INTELLIGENCE,bytes,232);CHECK(r.code==STN_RPC_OK && r.payload[3]==STN_PENDING_ACCEPTED);
        r=call(&s,STN_RPC_MINING_TEMPLATE,NULL,0);CHECK(r.code==STN_RPC_OK && r.length==484 && r.payload[147]==71);
        memcpy(work,r.payload,r.length);n=r.length;
        {unsigned nonce;uint8_t digest[32];for(nonce=0;nonce<65536;++nonce){work[226]=(uint8_t)(nonce>>8);work[227]=(uint8_t)nonce;if(stn_pow_verify(work+68,n-68,&c.hash_provider,digest)==STN_DATA_OK){break;}}}
        r=call(&s,STN_RPC_SUBMIT_WORK,work,n);CHECK(r.code==STN_RPC_OK && s.active.height==71 && first.count==0);
        CHECK(stn_storage_load(&c,&provider,snapshot,CAP,&active)==STN_STORAGE_OK && active.state.height==71);
        stn_storage_view_release(&active);
    }
    printf("Pending/assembly: %u checks, %u failures (validation hooks; real SHA-256/PoW).\n",checks,failures);return failures!=0;
}
