/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "stn_peer.h"
#include "stn_windows_peer.h"
#include "stn_windows_storage.h"
#include "stn_sha256.h"
#include "stn_mining.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"peer line %d: %s\n",__LINE__,#e);} } while(0)
#define CAP 60000u
static uint8_t a[130][364],b[130][364],scratch[CAP],next_bytes[CAP],candidate_bytes[CAP],saved[CAP];
static uint8_t frame[STN_PEER_MAX_FRAME],reply[STN_PEER_MAX_FRAME];
static stn_block_span as[130],bs[130];
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
static void branch(uint8_t blocks[130][364],stn_block_span spans[130],unsigned split)
{
    unsigned i;fixture(blocks[0]);
    for(i=0;i<130;++i) {
        if(i!=0) {
            memcpy(blocks[i],blocks[i-1],364);memset(blocks[i]+40,0,32);
            blocks[i][70]=blocks[i-1][87];blocks[i][71]=(uint8_t)i;blocks[i][79]=(uint8_t)i;
            if(i>=split) { blocks[i][87]=1; }
        }
        if(i>=60) { blocks[i][120]=(uint8_t)(i>=120 ? 7 : 31); }
        spans[i].bytes=blocks[i];spans[i].length=364;
    }
}

typedef struct memory_store { uint8_t bytes[CAP];size_t n;int exists,locked,fail; } memory_store;
static memory_store mem;
static stn_storage_status lock_mem(void *u){memory_store *m=u;if(m->locked){return STN_STORAGE_BUSY;}m->locked=1;return STN_STORAGE_OK;}
static void unlock_mem(void *u){((memory_store *)u)->locked=0;}
static stn_storage_status read_mem(void *u,uint8_t *p,size_t cap,size_t *n){memory_store *m=u;if(!m->exists){return STN_STORAGE_NOT_FOUND;}if(m->n>cap){return STN_STORAGE_CAPACITY;}memcpy(p,m->bytes,m->n);*n=m->n;return STN_STORAGE_OK;}
static stn_storage_status write_mem(void *u,const uint8_t *p,size_t n){memory_store *m=u;if(m->fail || n>CAP){return STN_STORAGE_IO;}memcpy(m->bytes,p,n);m->n=n;m->exists=1;return STN_STORAGE_OK;}
typedef struct mock { const stn_chain_context *c;size_t count,n,offset;stn_peer_session session;int mode; } mock;
static stn_peer_status mock_send(void *u,const uint8_t *p,size_t n)
{
    mock *m=u;stn_peer_status s=stn_peer_serve(m->c,bs,m->count,&m->session,p,n,reply,sizeof(reply),&m->n);size_t i;
    m->offset=0;if(s!=STN_PEER_OK){return s;}
    if(m->mode==2 && reply[7]==STN_PEER_HELLO){reply[12]^=1;}
    if(m->mode==3){reply[5]=1;}
    if(reply[7]==STN_PEER_STATE){
        if(m->mode==1){memset(reply+20,255,64);}
        if(m->mode==6){reply[7]=STN_PEER_HELLO;}
        if(m->mode==11){reply[95]=65;}
        if(m->mode==12){memset(reply+8,255,4);}
        if(m->mode==15){reply[19]=2;reply[95]=3;}
    }
    if(m->mode==7 || m->mode==8){
        if(reply[7]==STN_PEER_HEADERS){for(i=1;i<m->count;++i){reply[20+i*168+(m->mode==7?159:120)]=255;}}
        if(reply[7]==STN_PEER_BLOCK){reply[16+(m->mode==7?159:120)]=255;}
    }
    if(reply[7]==STN_PEER_BLOCK){
        if(m->mode==9){reply[16+172]=0;}
        if(m->mode==10){reply[15]=63;}
    }
    return STN_PEER_OK;
}
static stn_peer_status mock_receive(void *u,uint8_t *p,size_t n)
{
    mock *m=u;if(m->mode==5){return STN_PEER_TIMEOUT;}
    if(m->mode==4 && m->offset!=0){return STN_PEER_DISCONNECTED;}
    if(n>m->n-m->offset){return STN_PEER_DISCONNECTED;}
    memcpy(p,reply+m->offset,n);m->offset+=n;
    if(m->mode==14 && reply[7]==STN_PEER_BLOCK && m->offset==m->n){hash_status=STN_DATA_PROVIDER_ERROR;}
    return STN_PEER_OK;
}
static void reset(stn_chain_context *c,size_t count,stn_chain_state *active)
{
    stn_storage_view v;mem.fail=0;mem.exists=1;
    CHECK(stn_storage_encode(c,as,count,mem.bytes,CAP,&mem.n)==STN_STORAGE_OK);
    CHECK(stn_storage_decode(c,mem.bytes,mem.n,&v)==STN_STORAGE_OK);*active=v.state;stn_storage_view_release(&v);
}
static void framing(stn_chain_context *c)
{
    static const uint8_t independent[]={0x53,0x54,0x4e,0x50,0,2,0,5,0,0,0,4,0,0,0,1};
    stn_peer_message m,before;uint8_t out[128];size_t n,i;stn_peer_session session={0};
    CHECK(stn_peer_decode(independent,sizeof(independent),&m)==STN_PEER_OK && m.type==5 && m.length==4);
    CHECK(stn_peer_encode(m.type,m.payload,m.length,out,sizeof(out),&n)==STN_PEER_OK && n==16 && memcmp(out,independent,16)==0);
    before=m;
    for(i=0;i<16;++i){CHECK(stn_peer_decode(independent,i,&m)!=STN_PEER_OK && memcmp(&m,&before,sizeof(m))==0);}
    out[16]=0;CHECK(stn_peer_decode(out,17,&m)==STN_PEER_PROTOCOL);
    out[0]=0;CHECK(stn_peer_decode(out,16,&m)==STN_PEER_PROTOCOL);out[0]=0x53;
    out[5]=1;CHECK(stn_peer_decode(out,16,&m)==STN_PEER_PROTOCOL);out[5]=2;
    out[7]=99;CHECK(stn_peer_decode(out,16,&m)==STN_PEER_PROTOCOL);out[7]=5;
    memset(out+8,255,4);CHECK(stn_peer_decode(out,16,&m)==STN_PEER_PROTOCOL);
    CHECK(stn_peer_encode(1,NULL,68,out,sizeof(out),&n)==STN_PEER_ARGUMENT && n==0);
    CHECK(stn_peer_encode(1,independent,1,out,12,&n)==STN_PEER_CAPACITY);
    CHECK(stn_peer_serve(c,bs,2,&session,independent,16,out,sizeof(out),&n)==STN_PEER_PROTOCOL && session.handshake==-1);
}
static void synchronization(stn_chain_context *c)
{
    stn_storage_provider p={&mem,lock_mem,unlock_mem,read_mem,write_mem};
    stn_peer_workspace w={{scratch,CAP,next_bytes,CAP},candidate_bytes,CAP,frame,sizeof(frame)};
    mock m={0};stn_peer_transport t={&m,mock_send,mock_receive};stn_chain_state active,before;
    stn_peer_report r;size_t old_n;int mode;unsigned split;
    m.c=c;m.count=3;branch(b,bs,99);reset(c,2,&active);
    r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && r.reused_blocks==2 && r.received_blocks==1 && active.height==2);
    {
        stn_pending pending={0},pending_before;uint8_t id[32],unrelated[192],kept[32];
        w.pending=&pending;
        CHECK(stn_pending_insert(&pending,bs[0].bytes+172,192,&c->hash_provider,id)==STN_PENDING_ACCEPTED);
        memcpy(unrelated,bs[0].bytes+172,192);unrelated[191]^=1;
        CHECK(stn_pending_insert(&pending,unrelated,192,&c->hash_provider,kept)==STN_PENDING_ACCEPTED);
        pending_before=pending;reset(c,1,&active);m.count=3;m.mode=0;memset(&m.session,0,sizeof(m.session));
        mem.fail=1;r=stn_peer_sync(c,&p,&t,&w,&active);
        CHECK(r.status==STN_PEER_STORAGE && memcmp(&pending,&pending_before,sizeof(pending))==0);mem.fail=0;
        m.mode=9;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);
        CHECK(r.status!=STN_PEER_OK && memcmp(&pending,&pending_before,sizeof(pending))==0);
        m.mode=0;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);
        CHECK(r.status==STN_PEER_OK && pending.count==1 && pending.bytes==192 && memcmp(pending.entries[0].id,kept,32)==0);
        pending_before=pending;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);
        CHECK(r.status==STN_PEER_RETAINED && memcmp(&pending,&pending_before,sizeof(pending))==0);
        m.count=4;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);
        CHECK(r.status==STN_PEER_OK && memcmp(&pending,&pending_before,sizeof(pending))==0);
        CHECK(stn_pending_remove(&pending,id)==STN_PENDING_NOT_FOUND && pending.bytes==192);
        stn_pending_clear(&pending);w.pending=NULL;
    }
    memset(&m.session,0,sizeof(m.session));m.mode=1;m.count=2;
    before=active;r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_RETAINED && memcmp(&active,&before,sizeof(active))==0);
    /* Claimed 256-bit maximum work cannot make fewer validated blocks win. */
    for(split=1;split<=4;++split){
        reset(c,5,&active);branch(b,bs,split);m.count=6;m.mode=0;memset(&m.session,0,sizeof(m.session));
        r=stn_peer_sync(c,&p,&t,&w,&active);
        CHECK(r.status==STN_PEER_OK && r.reused_blocks==split && r.received_blocks==6-split && active.height==5);
        m.count=5;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_RETAINED);
    }
    reset(c,5,&active);branch(b,bs,1);m.count=5;memset(&m.session,0,sizeof(m.session));
    before=active;r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_RETAINED && memcmp(active.tip_id,before.tip_id,32)==0);
    for(mode=2;mode<=15;++mode){
        if(mode==13){continue;}reset(c,1,&active);branch(b,bs,99);m.mode=mode;m.count=2;memset(&m.session,0,sizeof(m.session));
        before=active;old_n=mem.n;memcpy(saved,mem.bytes,old_n);
        r=stn_peer_sync(c,&p,&t,&w,&active);hash_status=STN_DATA_OK;
        CHECK(r.status!=STN_PEER_OK && r.status!=STN_PEER_RETAINED);
        CHECK(memcmp(&active,&before,sizeof(active))==0 && mem.n==old_n && memcmp(mem.bytes,saved,old_n)==0);
    }
    reset(c,1,&active);m.mode=0;mem.fail=1;memset(&m.session,0,sizeof(m.session));before=active;old_n=mem.n;memcpy(saved,mem.bytes,old_n);
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_STORAGE && memcmp(&active,&before,sizeof(active))==0 && mem.n==old_n && memcmp(mem.bytes,saved,old_n)==0);mem.fail=0;
    /* Corrupt suffix: prefix is evidence, never activated on failed repair. */
    reset(c,5,&active);mem.bytes[12+3*368+4+363]^=1;memcpy(saved,mem.bytes,mem.n);old_n=mem.n;
    CHECK(stn_chain_initialize(c,&active)==STN_DATA_OK);before=active;m.count=6;branch(b,bs,99);m.mode=9;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.recovery && r.reused_blocks==3 && r.status!=STN_PEER_OK);
    CHECK(memcmp(&active,&before,sizeof(active))==0 && memcmp(saved,mem.bytes,old_n)==0);
    m.mode=0;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && r.recovery && r.reused_blocks==3 && active.height==5);
    mem.bytes[12+4+363]^=1;CHECK(stn_chain_initialize(c,&active)==STN_DATA_OK);
    before=active;old_n=mem.n;memcpy(saved,mem.bytes,old_n);mem.fail=1;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_STORAGE && memcmp(&active,&before,sizeof(active))==0 && memcmp(saved,mem.bytes,old_n)==0);
    mem.fail=0;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_OK && r.reused_blocks==0);
    /* Bad root framing requires full peer evidence, not guessing disk offsets. */
    mem.bytes[0]=0;CHECK(stn_chain_initialize(c,&active)==STN_DATA_OK);m.count=3;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_OK && r.recovery && r.reused_blocks==0 && r.received_blocks==3);
    /* Checksum-only corruption preserves all independently validated blocks. */
    mem.bytes[mem.n-1]^=1;CHECK(stn_chain_initialize(c,&active)==STN_DATA_OK);memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_OK && r.recovery && r.reused_blocks==3 && r.received_blocks==0);
    /* Many weaker peers cannot overturn stronger independently validated evidence. */
    m.count=6;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_OK);
    before=active;m.count=2;m.mode=1;
    for(mode=0;mode<3;++mode){memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_RETAINED && memcmp(&active,&before,sizeof(active))==0);}
    /* Missing store root bootstrap and maximum bounded count. */
    mem.exists=0;m.mode=0;m.count=64;memset(&m.session,0,sizeof(m.session));CHECK(stn_chain_initialize(c,&active)==STN_DATA_OK);
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_OK && active.height==63 && r.received_blocks==64 && m.session.requests==67);    /* Header pagination, deep catch-up, reconnect, reorg and truncated-prefix
     * recovery beyond the former lifetime limit. No peer claims are trusted. */
    m.count=130;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && active.height==129 && r.reused_blocks==64 && r.received_blocks==66);
    before=active;m.count=100;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_RETAINED && memcmp(&active,&before,sizeof(active))==0);
    reset(c,100,&active);branch(b,bs,80);m.count=130;m.mode=4;memset(&m.session,0,sizeof(m.session));before=active;
    r=stn_peer_sync(c,&p,&t,&w,&active);CHECK(r.status==STN_PEER_DISCONNECTED && memcmp(&active,&before,sizeof(active))==0);
    m.mode=0;memset(&m.session,0,sizeof(m.session));r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && active.height==129 && r.reused_blocks==80 && r.received_blocks==50);
    /* The count still advertises 130 but the final block/checksum is truncated. */
    mem.n-=200;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && r.recovery && r.reused_blocks==129 && r.received_blocks==1);
    mem.bytes[0]=0;memset(&m.session,0,sizeof(m.session));
    r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && r.recovery && r.received_blocks==130 && r.reused_blocks==0);
}
typedef struct server_args { stn_windows_peer listener;const stn_chain_context *c;stn_peer_status result; } server_args;
static DWORD WINAPI server_thread(void *u)
{
    server_args *args=u;stn_windows_peer connection={0};stn_peer_transport t;stn_peer_session session={0};size_t n,reply_n;unsigned i;
    args->result=stn_windows_peer_accept(&args->listener,10000,&connection,&t);
    for(i=0;args->result==STN_PEER_OK && i<4;++i){
        args->result=stn_peer_receive(&t,reply,sizeof(reply),&n);if(args->result!=STN_PEER_OK){break;}
        args->result=stn_peer_serve(args->c,bs,2,&session,reply,n,reply,sizeof(reply),&reply_n);if(args->result!=STN_PEER_OK){break;}
        args->result=t.send(t.user,reply,reply_n);
    }
    stn_windows_peer_close(&connection);return 0;
}
static void localhost(stn_chain_context *c)
{
    stn_windows_storage disk;stn_storage_view loaded;wchar_t temp[260],directory[260],path[260];
    server_args args={0};stn_windows_peer connection={0};stn_peer_transport t;uint16_t port;HANDLE thread;
    stn_storage_provider p={&mem,lock_mem,unlock_mem,read_mem,write_mem};
    stn_peer_workspace w={{scratch,CAP,next_bytes,CAP},candidate_bytes,CAP,frame,sizeof(frame)};stn_chain_state active;stn_peer_report r;
    /* Production hashing; independently fixed zero-nonce genesis and child. */
    c->hash_provider.hash=stn_sha256;fixture(a[0]);memcpy(a[1],a[0],364);a[1][79]=1;
    CHECK(stn_chain_block_id(a[0],364,&c->hash_provider,a[1]+40)==STN_DATA_OK);memcpy(b[0],a[0],364);memcpy(b[1],a[1],364);
    CHECK(GetTempPathW(260,temp)>0);CHECK(GetTempFileNameW(temp,L"stn",0,directory)!=0);
    CHECK(DeleteFileW(directory));CHECK(CreateDirectoryW(directory,NULL));
    CHECK(swprintf_s(path,260,L"%ls\\peer-chain.stns",directory)>0);
    CHECK(stn_windows_storage_init(&disk,path,&p)==STN_STORAGE_OK);
    CHECK(stn_storage_create(c,&p,as,1,next_bytes,CAP,&active)==STN_STORAGE_OK);args.c=c;
    CHECK(stn_windows_peer_listen(0,&args.listener,&port)==STN_PEER_OK);
    thread=CreateThread(NULL,0,server_thread,&args,0,NULL);CHECK(thread!=NULL);
    CHECK(stn_windows_peer_connect("127.0.0.1",port,10000,&connection,&t)==STN_PEER_OK);
    r=stn_peer_sync(c,&p,&t,&w,&active);
    CHECK(r.status==STN_PEER_OK && active.height==1 && active.cumulative_work.bytes[39]==4 && r.reused_blocks==1);
    CHECK(stn_storage_load(c,&p,scratch,CAP,&loaded)==STN_STORAGE_OK && memcmp(loaded.state.tip_id,active.tip_id,32)==0);
    stn_windows_peer_close(&connection);CHECK(WaitForSingleObject(thread,15000)==WAIT_OBJECT_0);CHECK(args.result==STN_PEER_OK);CHECK(CloseHandle(thread));
    stn_windows_peer_close(&args.listener);
    /* Real bounded idle accept timeout; closed listener rejects immediately. */
    CHECK(stn_windows_peer_listen(0,&args.listener,&port)==STN_PEER_OK);
    CHECK(stn_windows_peer_accept(&args.listener,10,&connection,&t)==STN_PEER_TIMEOUT);
    stn_windows_peer_close(&args.listener);
    CHECK(stn_windows_peer_connect("127.0.0.1",port,100,&connection,&t)!=STN_PEER_OK);
    CHECK(DeleteFileW(path));CHECK(DeleteFileW(disk.lock_path));CHECK(RemoveDirectoryW(directory));
}
typedef struct partial_args {stn_windows_peer listener;HANDLE entered;stn_peer_status status;uint8_t bytes[4];} partial_args;
static DWORD WINAPI partial_thread(void *u)
{
    partial_args *args=u;stn_windows_peer peer={0};stn_peer_transport t;
    args->status=stn_windows_peer_accept(&args->listener,10000,&peer,&t);
        if(args->status==STN_PEER_OK){
        args->status=t.receive(t.user,args->bytes,1);
        if(args->status==STN_PEER_OK){peer.io_timeout_ms=20;SetEvent(args->entered);args->status=t.receive(t.user,args->bytes+1,3);}
    }
    stn_windows_peer_close(&peer);return 0;
}
static void partial_io(void)
{
    partial_args args={0};stn_windows_peer peer={0};stn_peer_transport t;uint16_t port;HANDLE thread;
    args.entered=CreateEventW(NULL,TRUE,FALSE,NULL);CHECK(args.entered!=NULL);if(args.entered==NULL){return;}
    CHECK(stn_windows_peer_listen(0,&args.listener,&port)==STN_PEER_OK);
    thread=CreateThread(NULL,0,partial_thread,&args,0,NULL);CHECK(thread!=NULL);
    if(thread==NULL){stn_windows_peer_close(&args.listener);CloseHandle(args.entered);return;}
    CHECK(stn_windows_peer_connect("127.0.0.1",port,10000,&peer,&t)==STN_PEER_OK);
    CHECK(t.send(t.user,(const uint8_t*)"ST",2)==STN_PEER_OK);
    CHECK(WaitForSingleObject(args.entered,10000)==WAIT_OBJECT_0);
    CHECK(WaitForSingleObject(thread,100)==WAIT_TIMEOUT);
    CHECK(t.send(t.user,(const uint8_t*)"NC",2)==STN_PEER_OK);
    CHECK(WaitForSingleObject(thread,10000)==WAIT_OBJECT_0);
    CHECK(args.status==STN_PEER_OK && memcmp(args.bytes,"STNC",4)==0);
    CloseHandle(thread);CloseHandle(args.entered);stn_windows_peer_close(&peer);stn_windows_peer_close(&args.listener);
}
typedef struct convergence_server {
    stn_windows_peer listener;
    const stn_chain_context *chain;const stn_block_span *blocks;size_t count;
    const uint8_t *wrong;stn_peer_status result;
} convergence_server;
static DWORD WINAPI convergence_serve(void *user)
{
    convergence_server *s=user;stn_windows_peer peer={0};stn_peer_transport t;
    stn_peer_session session={0};size_t n,w,i;unsigned requests;
    s->result=stn_windows_peer_accept(&s->listener,10000,&peer,&t);
    for(requests=0;s->result==STN_PEER_OK && requests<132;++requests){
        s->result=stn_peer_receive(&t,reply,sizeof(reply),&n);if(s->result!=STN_PEER_OK){break;}
        s->result=stn_peer_serve(s->chain,s->blocks,s->count,&session,reply,n,reply,sizeof(reply),&w);
        if(s->result!=STN_PEER_OK){break;}
        if(s->wrong!=NULL){
            if(reply[7]==STN_PEER_STATE){memset(reply+52,255,40);} /* untrusted work claim */
            if(reply[7]==STN_PEER_HEADERS){
                size_t first=((size_t)reply[12]<<24)|((size_t)reply[13]<<16)|((size_t)reply[14]<<8)|reply[15];
                for(i=0;i<(w-20)/168;++i){if(first+i==60){memcpy(reply+20+i*168,s->wrong,168);}}
            }
            if(reply[7]==STN_PEER_BLOCK && reply[15]==60){memcpy(reply+16,s->wrong,364);}
        }
        s->result=t.send(t.user,reply,w);
    }
    stn_windows_peer_close(&peer);return 0;
}
static stn_peer_report exchange_branch(const stn_chain_context *c,const stn_block_span *blocks,size_t count,
    const uint8_t *wrong,const stn_storage_provider *storage,stn_peer_workspace *workspace,stn_chain_state *active)
{
    convergence_server server={0};stn_windows_peer peer={0};stn_peer_transport t;
    uint16_t port;HANDLE thread;stn_peer_report report={0};
    server.chain=c;server.blocks=blocks;server.count=count;server.wrong=wrong;
    CHECK(stn_windows_peer_listen(0,&server.listener,&port)==STN_PEER_OK);
    thread=CreateThread(NULL,0,convergence_serve,&server,0,NULL);CHECK(thread!=NULL);
    if(thread==NULL){stn_windows_peer_close(&server.listener);report.status=STN_PEER_IO;return report;}
    CHECK(stn_windows_peer_connect("127.0.0.1",port,10000,&peer,&t)==STN_PEER_OK);
    report=stn_peer_sync(c,storage,&t,workspace,active);stn_windows_peer_close(&peer);
    CHECK(WaitForSingleObject(thread,15000)==WAIT_OBJECT_0);
    CHECK(server.result==STN_PEER_DISCONNECTED);CloseHandle(thread);stn_windows_peer_close(&server.listener);
    return report;
}
static void convergence_u64(uint8_t *p,uint64_t value)
{size_t i;for(i=8;i!=0;){--i;p[i]=(uint8_t)value;value>>=8;}}
static stn_data_status convergence_hash(void *u,const uint8_t *d,size_t dn,const uint8_t *p,size_t n,uint8_t out[32])
{
    (void)u;
    if(dn==sizeof("STN-CHAIN:BLOCK:ID:1") && memcmp(d,"STN-CHAIN:BLOCK:ID:1",dn)==0){
        uint64_t height=0,time=0;size_t i;for(i=0;i<8;++i){height=(height<<8)|p[72+i];time=(time<<8)|p[80+i];}
        memset(out,0,32);out[31]=(uint8_t)(time>height*60 ? 2 : 1);return STN_DATA_OK;
    }
    return stn_sha256(NULL,d,dn,p,n,out);
}
static int convergence_solve(uint8_t *p,const stn_hash_provider *hash)
{
    uint8_t digest[32];uint64_t nonce;
    for(nonce=0;nonce<65536;++nonce){convergence_u64(p+152,nonce);if(stn_pow_verify(p,364,hash,digest)==STN_DATA_OK){return 1;}}
    return 0;
}
static stn_rpc_code convergence_rpc(stn_mining_service *s,uint16_t method,const uint8_t *input,size_t length,uint8_t *out,size_t *n)
{
    uint8_t request[512],response[512];size_t rn,wn;stn_rpc_message q={1,0,STN_RPC_OK,1,NULL,0},r;
    stn_rpc_service service={s,stn_mining_handle};q.method=method;q.payload=input;q.length=length;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&rn)==STN_RPC_OK);
    CHECK(stn_rpc_dispatch(request,rn,3,&service,response,sizeof(response),&wn)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,wn,&r)==STN_RPC_OK);*n=r.length;
    if(r.length!=0){memcpy(out,r.payload,r.length);}return r.code;
}
static void convergence(void)
{
    stn_windows_storage disks[2];stn_storage_provider providers[2];stn_chain_context c={0};stn_pow_policy policy;
    stn_chain_state states[2],before[2],prefix;stn_peer_report report;stn_reorg_plan plan;stn_fork_report fork;
    stn_storage_view view;stn_peer_workspace workspace={{scratch,CAP,next_bytes,CAP},candidate_bytes,CAP,frame,sizeof(frame)};
    stn_mining_service mining={0};static uint8_t snapshot[CAP],template_buffer[512],disk_copy[CAP];
    uint8_t target[2][32],wrong[364],stale[512],fresh[512],out[512];size_t n=0,w,i,j,old_size;
    wchar_t temp[260],directory[260],paths[2][260];unsigned high,initial_checks=checks;
    for(high=0;high<2;++high){
        fixture(a[0]);if(high){memset(a[0]+120,0,32);a[0][151]=15;}else{a[0][120]=31;}
        memcpy(policy.fixed_target,a[0]+120,32);c.network_id[0]=1;c.genesis_bytes=a[0];c.genesis_length=364;c.pow_policy=&policy;
        c.hash_provider.hash=high ? convergence_hash : stn_sha256;c.hash_provider.user=NULL;
        CHECK(convergence_solve(a[0],&c.hash_provider));memcpy(b[0],a[0],364);
        as[0].bytes=a[0];as[0].length=364;bs[0].bytes=b[0];bs[0].length=364;
        CHECK(GetTempPathW(260,temp)>0);CHECK(GetTempFileNameW(temp,L"stn",0,directory)!=0);
        CHECK(DeleteFileW(directory));CHECK(CreateDirectoryW(directory,NULL));
        for(j=0;j<2;++j){
            CHECK(swprintf_s(paths[j],260,L"%ls\\node-%u.stns",directory,(unsigned)j)>0);
            CHECK(stn_windows_storage_init(&disks[j],paths[j],&providers[j])==STN_STORAGE_OK);
            CHECK(stn_storage_create(&c,&providers[j],as,1,next_bytes,CAP,&states[j])==STN_STORAGE_OK);
            CHECK(stn_chain_required_target(&c,&states[j],target[j])==STN_DATA_OK);
        }
        CHECK(memcmp(states[0].tip_id,states[1].tip_id,32)==0 && memcmp(states[0].current_target,states[1].current_target,32)==0 && memcmp(states[0].cumulative_work.bytes,states[1].cumulative_work.bytes,40)==0 && memcmp(target[0],target[1],32)==0);
        report=exchange_branch(&c,as,1,NULL,&providers[1],&workspace,&states[1]);CHECK(report.status==STN_PEER_RETAINED);
        prefix=states[0];
        if(!high){
            stn_windows_peer listener={0},connection={0};stn_peer_transport transport;uint16_t port;
            CHECK(stn_windows_peer_listen(0,&listener,&port)==STN_PEER_OK);stn_windows_peer_close(&listener);
            CHECK(stn_windows_peer_connect("127.0.0.1",port,100,&connection,&transport)!=STN_PEER_OK);
            CHECK(memcmp(&states[0],&prefix,sizeof(prefix))==0 && memcmp(&states[1],&prefix,sizeof(prefix))==0);
        }
        /* No peer exchange while each node validates and persists its own branch. */
        for(j=0;j<2;++j){
            uint8_t (*blocks)[364]=j==0 ? a : b;stn_block_span *spans=j==0 ? as : bs;
            for(i=1;i<=60;++i){
                memcpy(blocks[i],blocks[0],364);memcpy(blocks[i]+40,states[j].tip_id,32);
                convergence_u64(blocks[i]+72,(uint64_t)i);convergence_u64(blocks[i]+80,(j==0 ? 1800u : 7200u)*(uint64_t)(i>59 ? 59 : i)/59);
                CHECK(stn_chain_required_target(&c,&states[j],blocks[i]+120)==STN_DATA_OK);
                CHECK(convergence_solve(blocks[i],&c.hash_provider));spans[i].bytes=blocks[i];spans[i].length=364;
                CHECK(stn_chain_validate_candidate(&c,&states[j],blocks[i],364,&states[j]).acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            }
            CHECK(stn_storage_adopt(&c,&providers[j],spans,61,&workspace.storage,&states[j])==STN_STORAGE_OK);
            CHECK(stn_chain_required_target(&c,&states[j],target[j])==STN_DATA_OK);
        }
        CHECK(memcmp(target[0],a[60]+120,32)==0 && memcmp(target[1],b[60]+120,32)==0 && memcmp(target[0],target[1],32)!=0);
        CHECK(high ? (target[0][31]==7 && target[1][31]==30 && states[0].cumulative_work.bytes[7]!=0) : (target[0][0]==15 && target[1][0]==63));
        fork=stn_fork_evaluate_history(&c,bs,61,as,61,&plan);CHECK(fork.result==STN_FORK_CANDIDATE && plan.ancestor_index==0 && plan.detached_count==60 && plan.attached_count==60);
        before[0]=states[0];before[1]=states[1];
        if(!high){
            memset(&mining,0,sizeof(mining));mining.chain=&c;mining.storage=&providers[1];mining.body=a[0]+168;mining.body_length=196;mining.transaction_count=1;
            mining.snapshot=snapshot;mining.snapshot_capacity=CAP;mining.template_bytes=template_buffer;mining.template_capacity=sizeof(template_buffer);mining.workspace=workspace.storage;
            CHECK(convergence_rpc(&mining,STN_RPC_MINING_TEMPLATE,NULL,0,stale,&n)==STN_RPC_OK && memcmp(stale+188,target[1],32)==0);
        }
        /* Invalid peer evidence includes a maximal fabricated work advertisement. */
        memcpy(wrong,b[60],364);memcpy(wrong+120,policy.fixed_target,32);CHECK(convergence_solve(wrong,&c.hash_provider));
        CHECK(stn_storage_load(&c,&providers[0],snapshot,CAP,&view)==STN_STORAGE_OK);stn_storage_view_release(&view);
        CHECK(providers[0].acquire(providers[0].user)==STN_STORAGE_OK);CHECK(providers[0].read(providers[0].user,disk_copy,CAP,&old_size)==STN_STORAGE_OK);providers[0].release(providers[0].user);
        report=exchange_branch(&c,bs,61,wrong,&providers[0],&workspace,&states[0]);CHECK(report.status==STN_PEER_VALIDATION && memcmp(&states[0],&before[0],sizeof(states[0]))==0);
        CHECK(providers[0].acquire(providers[0].user)==STN_STORAGE_OK);CHECK(providers[0].read(providers[0].user,snapshot,CAP,&w)==STN_STORAGE_OK && w==old_size && memcmp(snapshot,disk_copy,w)==0);providers[0].release(providers[0].user);
        report=exchange_branch(&c,bs,61,NULL,&providers[0],&workspace,&states[0]);CHECK(report.status==STN_PEER_RETAINED && memcmp(states[0].cumulative_work.bytes,before[0].cumulative_work.bytes,40)==0);
        report=exchange_branch(&c,as,61,NULL,&providers[1],&workspace,&states[1]);CHECK(report.status==STN_PEER_OK && report.reused_blocks==1 && report.received_blocks==60);
        CHECK(memcmp(states[0].tip_id,states[1].tip_id,32)==0 && memcmp(states[0].current_target,states[1].current_target,32)==0 && memcmp(states[0].cumulative_work.bytes,states[1].cumulative_work.bytes,40)==0);
        for(j=0;j<2;++j){
            memset(&states[j],0,sizeof(states[j])); /* discard runtime state; reopen canonical storage */
            CHECK(stn_windows_storage_init(&disks[j],paths[j],&providers[j])==STN_STORAGE_OK);
            CHECK(stn_storage_load(&c,&providers[j],snapshot,CAP,&view)==STN_STORAGE_OK);
            states[j]=view.state;CHECK(view.count==61 && memcmp(states[j].current_target,before[0].current_target,32)==0 && memcmp(states[j].tip_id,before[0].tip_id,32)==0 && memcmp(states[j].cumulative_work.bytes,before[0].cumulative_work.bytes,40)==0);
            for(i=0;i<61;++i){CHECK(view.blocks[i].length==364 && memcmp(view.blocks[i].bytes,a[i],364)==0);}
            stn_storage_view_release(&view);CHECK(stn_chain_required_target(&c,&states[j],target[j])==STN_DATA_OK && memcmp(target[j],a[60]+120,32)==0);
        }
        if(!high){
            CHECK(convergence_rpc(&mining,STN_RPC_SUBMIT_WORK,stale,n,out,&w)==STN_RPC_STALE && w==0);
            CHECK(convergence_rpc(&mining,STN_RPC_MINING_TEMPLATE,NULL,0,fresh,&w)==STN_RPC_OK && memcmp(fresh+188,target[0],32)==0 && memcmp(fresh+32,stale+32,32)!=0);
        }
        for(j=0;j<2;++j){CHECK(DeleteFileW(paths[j]));CHECK(DeleteFileW(disks[j].lock_path));}CHECK(RemoveDirectoryW(directory));
    }
    printf("Difficulty network convergence: %u targeted checks (real sockets/NTFS; high-work hash fixture separately).\n",checks-initial_checks);
}

int test_peer(void);
int test_peer(void)
{
    stn_chain_context c={0};stn_pow_policy policy;
    branch(a,as,99);branch(b,bs,99);memcpy(policy.fixed_target,a[0]+120,32);
    c.network_id[0]=1;c.genesis_bytes=a[0];c.genesis_length=364;c.pow_policy=&policy;c.hash_provider.hash=test_hash;
    framing(&c);synchronization(&c);localhost(&c);
    partial_io();convergence();
    printf("P2P/sync/recovery: %u checks, %u failures (localhost included).\n",checks,failures);
    return failures==0 ? 0 : 1;
}
