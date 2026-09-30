/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
/* [AI:GPT-6 | 2026-09-28 14:52:23 UTC] */
/* Linux includes the complete runtime. The Windows runner extracts the exact
 * bounded INFO helpers and substitutes native locks only for that unit test.
 * Synthetic accepted states below test serialization, not consensus validity. */
#ifdef _WIN32
#include <windows.h>
#include "stn_mining.h"
#include "stn_wire_internal.h"
#include <stdio.h>
#include <string.h>
typedef SRWLOCK pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT
static void pthread_mutex_lock(pthread_mutex_t *p) { AcquireSRWLockExclusive(p); }
static void pthread_mutex_unlock(pthread_mutex_t *p) { ReleaseSRWLockExclusive(p); }
#include "stn_linux_info_under_test.inc"
#define THREAD_RESULT DWORD WINAPI
#define THREAD_RETURN 0
typedef HANDLE test_thread;
static int start_thread(test_thread *t, LPTHREAD_START_ROUTINE f, void *p)
{ *t=CreateThread(NULL,0,f,p,0,NULL);return *t!=NULL; }
static void join_thread(test_thread t) { WaitForSingleObject(t,INFINITE);CloseHandle(t); }
static void pause_ms(unsigned ms) { Sleep(ms); }
static uint64_t test_now(void) { return GetTickCount64(); }
#else
#include "../platforms/linux/stn_app_linux.c"
#define THREAD_RESULT void *
#define THREAD_RETURN NULL
typedef pthread_t test_thread;
static int start_thread(test_thread *t, void *(*f)(void *), void *p)
{ return pthread_create(t,NULL,f,p)==0; }
static void join_thread(test_thread t) { (void)pthread_join(t,NULL); }
static void pause_ms(unsigned ms) { sleep_ms(ms); }
static uint64_t test_now(void) { return monotonic_ms(); }
#endif

static unsigned checks,failures;
#define CHECK(e) do { ++checks; if(!(e)){++failures;fprintf(stderr,"INFO line %d: %s\n",__LINE__,#e);} } while(0)
static pthread_mutex_t dispatch_test_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t gate_lock=PTHREAD_MUTEX_INITIALIZER;
static int held,release_holder,holder_timed_out;
static stn_mining_service test_mining;
static stn_chain_state state_a,state_b;
static unsigned handler_calls;

static void fixture(stn_chain_state *s,unsigned tag,uint64_t height)
{
    memset(s,0,sizeof(*s));s->has_tip=1;s->height=height;
    memset(s->network_id,(int)tag,32);memset(s->genesis_id,(int)(tag+1),32);
    memset(s->tip_id,(int)(tag+2),32);memset(s->current_target,(int)(tag+3),32);
    memset(s->cumulative_work.bytes,(int)(tag+4),STN_WORK_SIZE);
}

static stn_rpc_code mutation(void *u,const stn_rpc_message *q,
    uint8_t *p,size_t cap,size_t *written)
{
    (void)u;(void)q;(void)p;(void)cap;
    ++handler_calls;*written=0;test_mining.active=state_b;
    return STN_RPC_REJECTED;
}

static stn_rpc_code call_info(uint64_t id,uint8_t *response,size_t *written)
{
    uint8_t request[24];size_t length=0;
    stn_rpc_message q={1,STN_RPC_INFO,STN_RPC_OK,0,NULL,0};
    q.request_id=id;
    if(stn_rpc_encode(&q,request,sizeof(request),&length)!=STN_RPC_OK)
        return STN_RPC_INVALID;
    /* NULL mutable state/lock/service deliberately prove the INFO fast path
     * never consults them or performs a storage reconstruction. */
    return dispatch_request(request,length,NULL,NULL,NULL,response,208,written);
}

static THREAD_RESULT hold_dispatch(void *unused)
{
    uint64_t deadline=test_now()+3000u;
    (void)unused;
    pthread_mutex_lock(&dispatch_test_lock);
    pthread_mutex_lock(&gate_lock);held=1;pthread_mutex_unlock(&gate_lock);
    for(;;){
        int release;
        pthread_mutex_lock(&gate_lock);release=release_holder;pthread_mutex_unlock(&gate_lock);
        if(release)break;
        if(test_now()>=deadline){holder_timed_out=1;break;}
        pause_ms(1);
    }
    pthread_mutex_unlock(&dispatch_test_lock);
    return THREAD_RETURN;
}

static THREAD_RESULT alternate_snapshots(void *unused)
{
    unsigned i;(void)unused;
    for(i=0;i<20000u;++i){
        pthread_mutex_lock(&dispatch_test_lock);
        publish_info((i&1u)?&state_a:&state_b);
        pthread_mutex_unlock(&dispatch_test_lock);
    }
    return THREAD_RETURN;
}

int main(void)
{
    uint8_t response[208],expected_a[184],expected_b[184],request[25];
    size_t n=0,w=0;unsigned i;int started;test_thread worker;stn_rpc_message decoded;
    stn_rpc_message q={1,STN_RPC_INFO,STN_RPC_OK,UINT64_C(0x123456789abcdef0),NULL,0};
    stn_rpc_service service={NULL,mutation};
    fixture(&state_a,17,42);fixture(&state_b,91,42); /* same-height reorg */

    CHECK(call_info(1,response,&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_UNAVAILABLE && w==24);
    publish_info(&state_a);
    CHECK(call_info(q.request_id,response,&w)==STN_RPC_OK && w==208);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_OK);
    CHECK(decoded.request_id==q.request_id && decoded.method==STN_RPC_INFO && decoded.length==184);
    CHECK(memcmp(decoded.payload,state_a.network_id,32)==0);
    CHECK(memcmp(decoded.payload+32,state_a.genesis_id,32)==0);
    CHECK(stn_wire_read(decoded.payload+64,8)==42);
    CHECK(memcmp(decoded.payload+72,state_a.tip_id,32)==0);
    CHECK(memcmp(decoded.payload+104,state_a.cumulative_work.bytes,40)==0);
    CHECK(memcmp(decoded.payload+144,state_a.current_target,32)==0);
    CHECK(stn_wire_read(decoded.payload+176,4)==1 && stn_wire_read(decoded.payload+180,4)==43);
    memcpy(expected_a,decoded.payload,184);
    publish_info(&state_b);CHECK(call_info(2,response,&w)==STN_RPC_OK);
    memcpy(expected_b,response+24,184);
    CHECK(memcmp(expected_a,expected_b,184)!=0);

    /* A held outbound lock must not delay even a newly dispatched INFO. */
    started=start_thread(&worker,hold_dispatch,NULL);CHECK(started);
    if(started){
        int ready=0;
        uint64_t deadline=test_now()+2000u;
        for(;;){pthread_mutex_lock(&gate_lock);ready=held;pthread_mutex_unlock(&gate_lock);if(ready || test_now()>=deadline)break;pause_ms(1);}
        CHECK(ready);
        CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK);
        CHECK(dispatch_request(request,n,&service,&test_mining,&dispatch_test_lock,response,sizeof(response),&w)==STN_RPC_OK);
        CHECK(w==208 && memcmp(response+24,expected_b,184)==0 && handler_calls==0);
        pthread_mutex_lock(&gate_lock);release_holder=1;pthread_mutex_unlock(&gate_lock);
        join_thread(worker);CHECK(!holder_timed_out);
    }

    started=start_thread(&worker,alternate_snapshots,NULL);CHECK(started);
    if(started){
        for(i=0;i<20000u;++i){
            CHECK(call_info(i,response,&w)==STN_RPC_OK && w==208);
            CHECK(memcmp(response+24,expected_a,184)==0 || memcmp(response+24,expected_b,184)==0);
        }
        join_thread(worker);
    }

    /* Retain dispatcher error semantics for version, kind, and bad shape. */
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK);
    request[5]=3;
    CHECK(dispatch_request(request,n,NULL,NULL,NULL,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_VERSION && decoded.request_id==0);
    request[5]=2;request[7]=2; /* INFO response with no required payload */
    CHECK(dispatch_request(request,n,NULL,NULL,NULL,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_INVALID);
    request[7]=1;request[23]=1;request[24]=0;
    CHECK(dispatch_request(request,25,NULL,NULL,NULL,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_INVALID);
    CHECK(cached_info_handle(NULL,&q,response,183,&w)==STN_RPC_CAPACITY && w==0);

    /* Non-INFO stays on the normal handler, with publication even on error. */
    test_mining.active=state_a;publish_info(&state_a);
    q.method=STN_RPC_SUFFIX_STAGE_ABORT;
    CHECK(stn_rpc_encode(&q,request,sizeof(request),&n)==STN_RPC_OK);
    CHECK(dispatch_request(request,n,&service,&test_mining,&dispatch_test_lock,response,sizeof(response),&w)==STN_RPC_OK);
    CHECK(handler_calls==1 && stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_REJECTED);
    CHECK(call_info(99,response,&w)==STN_RPC_OK && memcmp(response+24,expected_b,184)==0);
    state_b.height=3;publish_info(&state_b);CHECK(call_info(100,response,&w)==STN_RPC_OK);
    CHECK(stn_wire_read(response+24+64,8)==3 && stn_wire_read(response+24+180,4)==4);
    state_b.height=UINT32_MAX;publish_info(&state_b);CHECK(call_info(101,response,&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_CAPACITY);
    memset(&state_b,0,sizeof(state_b));publish_info(&state_b);CHECK(call_info(102,response,&w)==STN_RPC_OK);
    CHECK(stn_rpc_decode(response,w,&decoded)==STN_RPC_OK && decoded.code==STN_RPC_UNAVAILABLE);
    printf("Linux INFO helpers: %u checks, %u failures.\n",checks,failures);
    return failures ? 1 : 0;
}
/* [End AI:GPT-6] */
