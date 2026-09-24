/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "stn_storage.h"
#include "stn_windows_storage.h"
#include "stn_sha256.h"
#include "stn_share.h"
#include "stn_share_replay.h"
#include "stn_transaction.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"storage line %d: %s\n",__LINE__,#e);} } while(0)
#define CAP 24000u
static uint8_t a[65][364],b[65][364],encoded[CAP],scratch[CAP],next_bytes[CAP],backup[CAP];
static stn_block_span as[65],bs[65];
static stn_data_status hash_status=STN_DATA_OK;
static stn_data_status test_hash(void *u,const uint8_t *d,size_t dn,const uint8_t *p,size_t n,uint8_t out[32])
{
    (void)u;
    if(hash_status!=STN_DATA_OK) { return hash_status; }
    if(dn==sizeof("STN-CHAIN:BLOCK:ID:1") && memcmp(d,"STN-CHAIN:BLOCK:ID:1",dn)==0) {
        memset(out,0,32);out[30]=p[87];out[31]=(uint8_t)(p[79]+1);
        if(p[159]==255) { out[0]=128; }
        return STN_DATA_OK;
    }
    return stn_sha256(NULL,d,dn,p,n,out);
}
static void fixture(uint8_t p[364])
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(p,0,364);memcpy(p,"STNB",4);p[5]=3;p[8]=1;p[163]=1;p[167]=196;p[171]=192;
    memcpy(p+172,"STNT",4);p[177]=1;p[179]=1;p[183]=180;
    memcpy(p+184,"STNR",4);p[189]=1;p[191]=1;p[192]=1;p[256]=3;
    memcpy(p+88,commitment,32);memset(p+120,255,32);p[120]=127;
}
static void branch(uint8_t blocks[65][364],stn_block_span spans[65],unsigned split)
{
    unsigned i;fixture(blocks[0]);
    for(i=0;i<65;++i) {
        if(i!=0) {
            memcpy(blocks[i],blocks[i-1],364);memset(blocks[i]+40,0,32);
            blocks[i][70]=blocks[i-1][87];blocks[i][71]=(uint8_t)i;blocks[i][79]=(uint8_t)i;
            if(i>=split) { blocks[i][87]=1; }
        }
        if(i>=60) { blocks[i][120]=31; }
        spans[i].bytes=blocks[i];spans[i].length=364;
    }
}
static void seal(uint8_t *p,size_t n)
{
    static const uint8_t domain[]="STN-CHAIN:STORAGE:1";
    CHECK(stn_sha256(NULL,domain,sizeof(domain),p,n-32,p+n-32)==STN_DATA_OK);
}
static void codecs(stn_chain_context *c)
{
    size_t n,i;stn_storage_view view={0},before;stn_chain_state empty={0},expected={0};
    CHECK(stn_storage_encode(c,as,3,encoded,CAP,&n)==STN_STORAGE_OK && n==1148);
    CHECK(memcmp(encoded,"STNS\0\1\0\0\0\0\0\3\0\0\1l",16)==0);
    stn_storage_view_release(&view);CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_OK && view.count==3);
    CHECK(stn_chain_initialize(c,&empty)==STN_DATA_OK);
    CHECK(stn_chain_validate_sequence(c,&empty,as,3,&expected).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(memcmp(expected.tip_id,view.state.tip_id,32)==0 && memcmp(expected.cumulative_work.bytes,view.state.cumulative_work.bytes,STN_WORK_SIZE)==0 && expected.height==view.state.height);
    before=view;memcpy(backup,encoded,n);
    for(i=0;i<n;++i) {
        CHECK(stn_storage_decode(c,encoded,i,&view)!=STN_STORAGE_OK);
        CHECK(memcmp(&view,&before,sizeof(view))==0);
    }
    /* Corruption with original checksum, then with recomputed checksum to
     * prove the checksum does not replace consensus validation. */
    {
        const size_t positions[]={0,5,6,8,11,12,15,16,16+4,16+8,384+71,384+120,384+151,384+159,384+172,384+363};
        for(i=0;i<sizeof(positions)/sizeof(positions[0]);++i) {
            memcpy(encoded,backup,n);encoded[positions[i]]^=255;
            CHECK(stn_storage_decode(c,encoded,n,&view)!=STN_STORAGE_OK);
            seal(encoded,n);
            CHECK(stn_storage_decode(c,encoded,n,&view)!=STN_STORAGE_OK);
            CHECK(memcmp(&view,&before,sizeof(view))==0);
        }
    }
    memcpy(encoded,backup,n);encoded[n]=0;
    CHECK(stn_storage_decode(c,encoded,n+1,&view)!=STN_STORAGE_OK);
    memset(encoded+8,255,4);CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_FORMAT);
    memcpy(encoded,backup,n);memset(encoded+12,255,4);CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_FORMAT);
    memcpy(encoded,backup,n);memset(encoded+8,0,4);CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_FORMAT);
    memcpy(encoded,backup,n);c->network_id[0]=2;
    CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_VALIDATION);c->network_id[0]=1;
    a[0][87]=2;CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_VALIDATION);a[0][87]=0;
    hash_status=STN_DATA_UNRESOLVED;CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_UNRESOLVED);
    hash_status=STN_DATA_PROVIDER_ERROR;CHECK(stn_storage_decode(c,encoded,n,&view)!=STN_STORAGE_OK);hash_status=STN_DATA_OK;
    CHECK(stn_storage_encode(c,as,0,encoded,CAP,&n)==STN_STORAGE_ARGUMENT && n==0);
    CHECK(stn_storage_encode(c,as,65,encoded,CAP,&n)==STN_STORAGE_OK && n==23964);
    stn_storage_view_release(&view);CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_OK && view.count==65 && view.state.height==64 && view.state.cumulative_work.bytes[39]==160);
    CHECK(stn_storage_encode(c,as,1,encoded,10,&n)==STN_STORAGE_CAPACITY && n==0);
    CHECK(stn_storage_encode(c,as,64,encoded,CAP,&n)==STN_STORAGE_OK && n==23596);
    stn_storage_view_release(&view);CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_OK && view.count==64 && view.state.height==63 && view.state.cumulative_work.bytes[39]==152);
    CHECK(stn_storage_decode(NULL,encoded,n,&view)==STN_STORAGE_ARGUMENT);
    CHECK(stn_storage_decode(c,NULL,n,&view)==STN_STORAGE_ARGUMENT);
    CHECK(stn_storage_decode(c,encoded,n,NULL)==STN_STORAGE_ARGUMENT);
    /* Duplicates remain forbidden even with a correct body/storage checksum. */
    CHECK(stn_storage_encode(c,as,2,encoded,CAP,&n)==STN_STORAGE_OK);
    memcpy(encoded+748,a[1]+168,196); /* second body transaction */
    encoded[382]=2;encoded[383]=48; /* child block length 560 */
    encoded[384+163]=2;encoded[384+166]=1;encoded[384+167]=136;
    CHECK(stn_block_body_commitment(encoded+384+168,392,2,&c->hash_provider,encoded+384+88)==STN_DATA_OK);
    n=976;seal(encoded,n);
    CHECK(stn_storage_decode(c,encoded,n,&view)==STN_STORAGE_VALIDATION);
    stn_storage_view_release(&view);stn_chain_state_release(&empty);stn_chain_state_release(&expected);
}

static void share_restart_replay(stn_chain_context *c)
{
    static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
    stn_share_evidence share={0};
    stn_transaction tx={0};
    stn_storage_view view={0};
    stn_block_span history[3];
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE];
    uint8_t transaction[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
    uint8_t share_block[STN_BLOCK_HEADER_SIZE+4u+STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
    size_t transaction_length=0u,storage_length=0u;
    size_t body_length=4u+STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE;

    memcpy(share.template_header,a[1],STN_SHARE_TEMPLATE_HEADER_SIZE);
    share.miner.type=STN_ADDRESS_IDENTITY;
    memset(share.miner.identifier,0x5a,sizeof(share.miner.identifier));
    share.nonce=1u;
    memcpy(share.body_commitment,share.template_header+88u,32u);
    CHECK(c->hash_provider.hash(c->hash_provider.user,
        work_domain,sizeof(work_domain),
        share.template_header,STN_SHARE_TEMPLATE_HEADER_SIZE,
        share.work_id)==STN_DATA_OK);
    CHECK(stn_share_verify_evidence(&share,&c->hash_provider,canonical)==STN_DATA_OK);
    CHECK(stn_share_encode(&share,canonical)==STN_DATA_OK);

    tx.version=1u;
    tx.type=STN_TX_SHARE_EVIDENCE;
    tx.record_bytes=canonical;
    tx.record_length=STN_SHARE_CANONICAL_SIZE;
    CHECK(stn_transaction_encode(&tx,transaction,sizeof(transaction),
        &transaction_length)==STN_DATA_OK);
    CHECK(transaction_length==STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE);

    memset(share_block,0,sizeof(share_block));
    memcpy(share_block,a[2],STN_BLOCK_HEADER_SIZE);
    share_block[162]=0u;share_block[163]=1u;
    share_block[164]=(uint8_t)(body_length>>24);
    share_block[165]=(uint8_t)(body_length>>16);
    share_block[166]=(uint8_t)(body_length>>8);
    share_block[167]=(uint8_t)body_length;
    share_block[168]=(uint8_t)(transaction_length>>24);
    share_block[169]=(uint8_t)(transaction_length>>16);
    share_block[170]=(uint8_t)(transaction_length>>8);
    share_block[171]=(uint8_t)transaction_length;
    memcpy(share_block+172u,transaction,transaction_length);
    CHECK(stn_block_body_commitment(share_block+168u,body_length,1u,
        &c->hash_provider,share_block+88u)==STN_DATA_OK);

    history[0]=as[0];
    history[1]=as[1];
    history[2].bytes=share_block;
    history[2].length=sizeof(share_block);

    CHECK(stn_storage_encode(c,history,3u,encoded,CAP,&storage_length)==STN_STORAGE_OK);
    CHECK(storage_length>0u);

    /* Simulate restart: no replay object is carried across this boundary.
     * Decode reconstructs accepted state solely from canonical STNS history. */
    CHECK(stn_storage_decode(c,encoded,storage_length,&view)==STN_STORAGE_OK);
    CHECK(view.state.shares!=NULL);
    CHECK(view.state.shares->consumed_count==1u);
    CHECK(stn_share_replay_check(view.state.shares,&share)==STN_SHARE_REPLAY_DUPLICATE);
    stn_storage_view_release(&view);

    memset(&view,0,sizeof(view));
    CHECK(stn_storage_decode(c,encoded,storage_length,&view)==STN_STORAGE_OK);
    CHECK(view.state.shares!=NULL);
    CHECK(view.state.shares->consumed_count==1u);
    CHECK(stn_share_replay_check(view.state.shares,&share)==STN_SHARE_REPLAY_DUPLICATE);
    stn_storage_view_release(&view);
}


static void share_reorg_replay(stn_chain_context *c)
{
    static const uint8_t work_domain[]="STN-CHAIN:WORK:ID:1";
    stn_share_evidence detached={0};
    stn_transaction tx={0};
    stn_chain_state detached_state={0},replacement_state={0};
    stn_block_span detached_history[3],replacement_history[3];
    uint8_t canonical[STN_SHARE_CANONICAL_SIZE];
    uint8_t transaction[STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
    uint8_t share_block[STN_BLOCK_HEADER_SIZE+4u+STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE];
    size_t transaction_length=0u;
    size_t body_length=4u+STN_TX_HEADER_SIZE+STN_SHARE_CANONICAL_SIZE;

    memcpy(detached.template_header,a[1],STN_SHARE_TEMPLATE_HEADER_SIZE);
    detached.miner.type=STN_ADDRESS_IDENTITY;
    memset(detached.miner.identifier,0x6b,sizeof(detached.miner.identifier));
    detached.nonce=2u;
    memcpy(detached.body_commitment,detached.template_header+88u,32u);
    CHECK(c->hash_provider.hash(c->hash_provider.user,
        work_domain,sizeof(work_domain),
        detached.template_header,STN_SHARE_TEMPLATE_HEADER_SIZE,
        detached.work_id)==STN_DATA_OK);
    CHECK(stn_share_verify_evidence(&detached,&c->hash_provider,canonical)==STN_DATA_OK);
    CHECK(stn_share_encode(&detached,canonical)==STN_DATA_OK);

    tx.version=1u;
    tx.type=STN_TX_SHARE_EVIDENCE;
    tx.record_bytes=canonical;
    tx.record_length=STN_SHARE_CANONICAL_SIZE;
    CHECK(stn_transaction_encode(&tx,transaction,sizeof(transaction),
        &transaction_length)==STN_DATA_OK);

    memcpy(share_block,a[2],STN_BLOCK_HEADER_SIZE);
    share_block[162]=0u;share_block[163]=1u;
    share_block[164]=(uint8_t)(body_length>>24);
    share_block[165]=(uint8_t)(body_length>>16);
    share_block[166]=(uint8_t)(body_length>>8);
    share_block[167]=(uint8_t)body_length;
    share_block[168]=(uint8_t)(transaction_length>>24);
    share_block[169]=(uint8_t)(transaction_length>>16);
    share_block[170]=(uint8_t)(transaction_length>>8);
    share_block[171]=(uint8_t)transaction_length;
    memcpy(share_block+172u,transaction,transaction_length);
    CHECK(stn_block_body_commitment(share_block+168u,body_length,1u,
        &c->hash_provider,share_block+88u)==STN_DATA_OK);

    detached_history[0]=as[0];
    detached_history[1]=as[1];
    detached_history[2].bytes=share_block;
    detached_history[2].length=sizeof(share_block);
    CHECK(stn_chain_reconstruct_history(c,detached_history,3u,
        &detached_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(detached_state.shares!=NULL);
    CHECK(stn_share_replay_check(detached_state.shares,
        &detached)==STN_SHARE_REPLAY_DUPLICATE);

    /*
     * Reconstruct an alternate accepted branch from the common genesis.
     * Replay state must be derived only from that branch; evidence consumed
     * exclusively by the detached branch cannot survive the reorg.
     */
    branch(b,bs,1u);
    replacement_history[0]=bs[0];
    replacement_history[1]=bs[1];
    replacement_history[2]=bs[2];
    CHECK(stn_chain_reconstruct_history(c,replacement_history,3u,
        &replacement_state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(replacement_state.shares!=NULL);
    CHECK(replacement_state.shares->consumed_count==0u);
    CHECK(stn_share_replay_check(replacement_state.shares,
        &detached)==STN_SHARE_REPLAY_FRESH);

    stn_chain_state_release(&detached_state);
    stn_chain_state_release(&replacement_state);
}

typedef struct memory_store { uint8_t data[CAP],stage[CAP];size_t n;int exists,locked,failure; } memory_store;
static memory_store mem;
static stn_storage_status acquire(void *u){memory_store *m=u;if(m->failure==1 || m->locked){return STN_STORAGE_BUSY;}m->locked=1;return STN_STORAGE_OK;}
static void release(void *u){((memory_store *)u)->locked=0;}
static stn_storage_status read_mem(void *u,uint8_t *p,size_t cap,size_t *n)
{
    memory_store *m=u;if(!m->locked || m->failure==6){return STN_STORAGE_IO;}
    if(!m->exists){return STN_STORAGE_NOT_FOUND;}if(m->n>cap){return STN_STORAGE_CAPACITY;}
    memcpy(p,m->data,m->n);*n=m->n;return STN_STORAGE_OK;
}
static stn_storage_status replace_mem(void *u,const uint8_t *p,size_t n)
{
    memory_store *m=u;if(!m->locked || n>CAP || m->failure==2){return STN_STORAGE_IO;}
    memcpy(m->stage,p,n/2);if(m->failure==3){return STN_STORAGE_IO;}
    memcpy(m->stage,p,n);if(m->failure==4 || m->failure==5){return STN_STORAGE_IO;}
    memcpy(m->data,m->stage,n);m->n=n;m->exists=1;return STN_STORAGE_OK;
}
static void application(stn_chain_context *c)
{
    stn_storage_provider p={&mem,acquire,release,read_mem,replace_mem};
    stn_storage_workspace w={scratch,CAP,next_bytes,CAP};stn_chain_state active={0},before;
    stn_storage_view v={0},saved_view;stn_reorg_plan plan={0},bad;size_t old_n;int f;unsigned split;
    memset(&active,0,sizeof(active));before=active;memset(&v,0,sizeof(v));saved_view=v;
    CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_NOT_FOUND && memcmp(&v,&saved_view,sizeof(v))==0);
    mem.failure=3;CHECK(stn_storage_create(c,&p,as,1,next_bytes,CAP,&active)==STN_STORAGE_IO && !mem.exists && memcmp(&active,&before,sizeof(active))==0);
    mem.failure=0;CHECK(stn_storage_create(c,&p,as,1,next_bytes,CAP,&active)==STN_STORAGE_OK);
    CHECK(stn_storage_create(c,&p,as,1,next_bytes,CAP,&active)==STN_STORAGE_EXISTS);
    stn_storage_view_release(&v);CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_OK && v.state.height==0);
    stn_reorg_plan_release(&plan);CHECK(stn_fork_evaluate(c,as,1,as,2,&plan).result==STN_FORK_CANDIDATE);
    before=active;old_n=mem.n;memcpy(backup,mem.data,old_n);
    for(f=1;f<=6;++f) {
        mem.failure=f;
        CHECK(stn_storage_apply(c,&p,as,2,&plan,&w,&active)!=STN_STORAGE_OK);
        CHECK(memcmp(&active,&before,sizeof(active))==0 && mem.n==old_n && memcmp(mem.data,backup,old_n)==0 && !mem.locked);
    }
    mem.failure=0;bad=plan;bad.attach_begin++;
    CHECK(stn_storage_apply(c,&p,as,2,&bad,&w,&active)==STN_STORAGE_STALE);
    active.height++;CHECK(stn_storage_apply(c,&p,as,2,&plan,&w,&active)==STN_STORAGE_STALE);active=before;
    CHECK(stn_storage_apply(c,&p,as,2,&plan,&w,&active)==STN_STORAGE_OK && active.height==1);
    stn_storage_view_release(&v);CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_OK && v.state.height==active.height && memcmp(v.state.tip_id,active.tip_id,32)==0);
    CHECK(stn_storage_apply(c,&p,as,2,&plan,&w,&active)==STN_STORAGE_NOT_PREFERRED);
    CHECK(stn_storage_apply(c,&p,as,1,&plan,&w,&active)==STN_STORAGE_NOT_PREFERRED);
    stn_reorg_plan_release(&plan);CHECK(stn_fork_evaluate(c,as,2,as,5,&plan).result==STN_FORK_CANDIDATE);
    CHECK(stn_storage_apply(c,&p,as,5,&plan,&w,&active)==STN_STORAGE_OK);
    for(split=1;split<=3;split+=2) {
        /* Reset the fake provider to an independently validated five-block snapshot. */
        CHECK(stn_storage_encode(c,as,5,mem.data,CAP,&mem.n)==STN_STORAGE_OK);
        stn_storage_view_release(&v);CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_OK);stn_chain_state_move(&active,&v.state);
        branch(b,bs,split);stn_reorg_plan_release(&plan);CHECK(stn_fork_evaluate(c,as,5,bs,6,&plan).result==STN_FORK_CANDIDATE);
        before=active;old_n=mem.n;memcpy(backup,mem.data,old_n);
        for(f=2;f<=5;++f) {
            mem.failure=f;CHECK(stn_storage_apply(c,&p,bs,6,&plan,&w,&active)==STN_STORAGE_IO);
            CHECK(memcmp(&active,&before,sizeof(active))==0 && mem.n==old_n && memcmp(mem.data,backup,old_n)==0);
        }
        mem.failure=0;b[5][159]=255;
        CHECK(stn_storage_apply(c,&p,bs,6,&plan,&w,&active)==STN_STORAGE_VALIDATION);b[5][159]=0;
        bad=plan;bad.candidate.cumulative_work.bytes[0]=255;
        CHECK(stn_storage_apply(c,&p,bs,6,&bad,&w,&active)==STN_STORAGE_STALE);
        CHECK(stn_storage_apply(c,&p,bs,6,&plan,&w,&active)==STN_STORAGE_OK && active.height==5);
        stn_storage_view_release(&v);CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_OK && v.state.cumulative_work.bytes[39]==12 && memcmp(v.state.tip_id,active.tip_id,32)==0);
    }
    saved_view=v;mem.data[0]^=1;
    CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_FORMAT && memcmp(&v,&saved_view,sizeof(v))==0);
    CHECK(stn_storage_create(c,&p,as,1,next_bytes,CAP,&active)==STN_STORAGE_EXISTS);
    before=active;CHECK(stn_storage_apply(c,&p,bs,7,&plan,&w,&active)==STN_STORAGE_FORMAT && memcmp(&active,&before,sizeof(active))==0);
    mem.data[0]^=1;--mem.n;
    CHECK(stn_storage_load(c,&p,scratch,CAP,&v)!=STN_STORAGE_OK && memcmp(&v,&saved_view,sizeof(v))==0);
    mem.n=CAP+1;
    CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_CAPACITY && memcmp(&v,&saved_view,sizeof(v))==0);
    stn_storage_view_release(&v);stn_chain_state_release(&active);stn_reorg_plan_release(&plan);
}
static void windows_disk(stn_chain_context *c)
{
    wchar_t temp[260],directory[260],path[260];stn_windows_storage store,other;
    stn_storage_provider p,q;stn_storage_workspace w={scratch,CAP,next_bytes,CAP};
    stn_chain_state active={0},before;stn_storage_view v={0};stn_reorg_plan plan={0};HANDLE h;DWORD got;uint8_t byte;
    CHECK(GetTempPathW(260,temp)>0);CHECK(GetTempFileNameW(temp,L"stn",0,directory)!=0);
    CHECK(DeleteFileW(directory));CHECK(CreateDirectoryW(directory,NULL));
    CHECK(swprintf_s(path,260,L"%ls\\chain.stns",directory)>0);
    CHECK(stn_windows_storage_init(&store,path,&p)==STN_STORAGE_OK);
    CHECK(stn_windows_storage_init(&other,path,&q)==STN_STORAGE_OK);
    CHECK(p.acquire(p.user)==STN_STORAGE_OK);CHECK(q.acquire(q.user)==STN_STORAGE_BUSY);p.release(p.user);
    CHECK(stn_storage_load(c,&p,scratch,CAP,&v)==STN_STORAGE_NOT_FOUND);
    CHECK(stn_storage_create(c,&p,as,5,next_bytes,CAP,&active)==STN_STORAGE_OK);
    branch(b,bs,1);stn_reorg_plan_release(&plan);CHECK(stn_fork_evaluate(c,as,5,bs,6,&plan).result==STN_FORK_CANDIDATE);before=active;
    /* An actual existing staging file must not be overwritten or promoted. */
    h=CreateFileW(store.staging,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(h!=INVALID_HANDLE_VALUE);CHECK(CloseHandle(h));
    CHECK(stn_storage_apply(c,&p,bs,6,&plan,&w,&active)==STN_STORAGE_IO && memcmp(&active,&before,sizeof(active))==0);
    stn_storage_view_release(&v);CHECK(stn_storage_load(c,&q,scratch,CAP,&v)==STN_STORAGE_OK && v.state.height==4);
    CHECK(DeleteFileW(store.staging));
    h=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(h!=INVALID_HANDLE_VALUE);
    CHECK(stn_storage_apply(c,&p,bs,6,&plan,&w,&active)==STN_STORAGE_IO && memcmp(&active,&before,sizeof(active))==0);
    CHECK(CloseHandle(h));stn_storage_view_release(&v);CHECK(stn_storage_load(c,&q,scratch,CAP,&v)==STN_STORAGE_OK && v.state.height==4);
    CHECK(stn_storage_apply(c,&p,bs,6,&plan,&w,&active)==STN_STORAGE_OK);
    stn_storage_view_release(&v);CHECK(stn_storage_load(c,&q,scratch,CAP,&v)==STN_STORAGE_OK && v.state.height==5);
    h=CreateFileW(path,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(h!=INVALID_HANDLE_VALUE);CHECK(ReadFile(h,&byte,1,&got,NULL) && got==1);byte^=1;
    CHECK(SetFilePointer(h,0,NULL,FILE_BEGIN)==0);CHECK(WriteFile(h,&byte,1,&got,NULL) && got==1);CHECK(CloseHandle(h));
    CHECK(stn_storage_load(c,&q,scratch,CAP,&v)==STN_STORAGE_FORMAT);
    CHECK(DeleteFileW(path));
    /* Real SHA-256 startup and extension on actual storage. */
    c->hash_provider.hash=stn_sha256;fixture(a[0]);memcpy(a[1],a[0],364);a[1][79]=1;
    CHECK(stn_chain_block_id(a[0],364,&c->hash_provider,a[1]+40)==STN_DATA_OK);
    CHECK(stn_storage_create(c,&p,as,1,next_bytes,CAP,&active)==STN_STORAGE_OK);
    stn_reorg_plan_release(&plan);CHECK(stn_fork_evaluate(c,as,1,as,2,&plan).result==STN_FORK_CANDIDATE);
    CHECK(stn_storage_apply(c,&p,as,2,&plan,&w,&active)==STN_STORAGE_OK);
    stn_storage_view_release(&v);CHECK(stn_storage_load(c,&q,scratch,CAP,&v)==STN_STORAGE_OK && v.state.height==1 && v.state.cumulative_work.bytes[39]==4);
    CHECK(DeleteFileW(path));CHECK(DeleteFileW(store.lock_path));CHECK(RemoveDirectoryW(directory));
    stn_storage_view_release(&v);stn_chain_state_release(&active);stn_reorg_plan_release(&plan);
}
static void publication_activation_boundary(void)
{
    uint8_t genesis[364],block[364];stn_chain_context c={0};stn_pow_policy policy;
    stn_chain_state state,previous;unsigned i,start=checks;
    fixture(genesis);memcpy(policy.fixed_target,genesis+120,32);
    c.network_id[0]=1;c.genesis_bytes=genesis;c.genesis_length=364;c.pow_policy=&policy;c.hash_provider.hash=test_hash;
    CHECK(stn_chain_initialize(&c,&state)==STN_DATA_OK && state.publication_activation_height==130);
    for(i=0;i<=130;++i){
        memcpy(block,genesis,sizeof(block));block[79]=(uint8_t)i;
        if(i){memcpy(block+40,state.tip_id,32);CHECK(stn_chain_required_target(&c,&state,block+120)==STN_DATA_OK);}
        previous=state;
        if(i<130){CHECK(stn_chain_validate_candidate(&c,&state,block,sizeof(block),&state).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);}
        else {CHECK(stn_chain_validate_candidate(&c,&state,block,sizeof(block),&state).acceptance==STN_ACCEPTANCE_REJECTED);CHECK(memcmp(&previous,&state,sizeof(state))==0);}
    }
    state.publication_activation_height=131;
    CHECK(stn_chain_validate_candidate(&c,&state,block,sizeof(block),&state).reason==STN_CHAIN_STATE);
    stn_chain_state_release(&state);
    printf("Phase 15 activation boundary: %u targeted checks.\n",checks-start);
}
int test_storage(void);
int test_storage(void)
{
    size_t ownership_baseline=stn_chain_test_live_snapshots();
    stn_chain_context c={0};stn_pow_policy policy;
    branch(a,as,99);branch(b,bs,1);memcpy(policy.fixed_target,a[0]+120,32);
    c.network_id[0]=1;c.genesis_bytes=a[0];c.genesis_length=364;c.pow_policy=&policy;c.hash_provider.hash=test_hash;
    codecs(&c);share_restart_replay(&c);share_reorg_replay(&c);application(&c);windows_disk(&c);publication_activation_boundary();
    CHECK(stn_chain_test_live_snapshots()==ownership_baseline);
    printf("Persistence/application: %u checks, %u failures.\n",checks,failures);
    return failures==0 ? 0 : 1;
}
