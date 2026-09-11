/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "../stn_backend.h"
#include "stn_windows_storage.h"
#include "stn_windows_peer.h"
#include "stn_mining.h"
#include "stn_sha256.h"
#include "../../src/stn_wire_internal.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef STN_PHASE9_TEST_RUNTIME
#include "../../tests/stn_phase9_runtime.h"
#endif
#define APP_STORAGE_INITIAL (STN_STORAGE_OVERHEAD+4u+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY)
#define APP_RPC_ACCEPT_POLL_MS 250u
#define APP_RPC_IO_TIMEOUT_MS 60000u
static volatile LONG stopping;
static BOOL WINAPI stop(DWORD event)
{
    if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT){InterlockedExchange(&stopping,1);return TRUE;}
    return FALSE;
}
static int read_file(const char *path,uint8_t *bytes,size_t cap,size_t *n)
{
    FILE *f=NULL;int extra,bad;
    if(fopen_s(&f,path,"rb")!=0){return 0;}
    *n=fread(bytes,1,cap,f);extra=fgetc(f);bad=ferror(f);(void)fclose(f);
    return !bad && extra==EOF && *n!=0;
}
static size_t initial_storage_capacity(const wchar_t *path)
{
    WIN32_FILE_ATTRIBUTE_DATA a;ULONGLONG n;size_t required=APP_STORAGE_INITIAL;
    if(GetFileAttributesExW(path,GetFileExInfoStandard,&a)){
        n=((ULONGLONG)a.nFileSizeHigh<<32)|a.nFileSizeLow;
        if(n<=SIZE_MAX){
            size_t existing=(size_t)n;
            if(existing>required){required=existing;}
        }
    }
    return required;
}
static void development_genesis(uint8_t *b)
{
    static const uint8_t commitment[32]={0xec,0xf9,0x1b,0xfa,0x6a,0x4e,0x06,0xd8,0x6a,0x24,0xd6,0x36,0xee,0xd2,0x5f,0x01,0xcf,0x30,0x29,0x49,0xe8,0x53,0x51,0xd4,0x37,0xe0,0x4c,0x4e,0x49,0x21,0x9a,0x76};
    memset(b,0,364);memcpy(b,"STNB",4);b[5]=3;b[8]=1;b[163]=1;b[167]=196;b[171]=192;
    memcpy(b+172,"STNT",4);b[177]=1;b[179]=1;b[183]=180;
    memcpy(b+184,"STNR",4);b[189]=1;b[191]=1;b[192]=1;b[256]=3;
    memcpy(b+88,commitment,32);memset(b+120,255,32);b[120]=127;
}
static stn_peer_status transfer(const stn_peer_transport *t,uint8_t *p,size_t n,int sending,int idle_allowed)
{
    size_t remaining=n;
    while(n!=0){size_t chunk=n>65536 ? 65536 : n;
        stn_peer_status s=sending ? t->send(t->user,p,chunk) : t->receive(t->user,p,chunk);
        if(s==STN_PEER_TIMEOUT && (!idle_allowed || n!=remaining)){continue;}
        if(s!=STN_PEER_OK){return s;}p+=chunk;n-=chunk;
    }return STN_PEER_OK;
}
typedef struct rpc_client {
    HANDLE thread;
    stn_windows_peer peer;
    stn_peer_transport transport;
    const stn_rpc_service *service;
    CRITICAL_SECTION *dispatch_lock;
    volatile LONG done;
    struct rpc_client *next;
} rpc_client;
static DWORD WINAPI rpc_client_thread(void *user)
{
    rpc_client *client=(rpc_client*)user;
    uint8_t *request=(uint8_t*)malloc(STN_RPC_MAX_FRAME),*response=(uint8_t*)malloc(STN_RPC_MAX_FRAME);
    if(request!=NULL && response!=NULL){
        while(InterlockedCompareExchange(&stopping,0,0)==0){
            size_t n,w;stn_peer_status io=transfer(&client->transport,request,24,0,1);stn_rpc_code dispatch;
            if(io==STN_PEER_TIMEOUT){continue;}
            if(io!=STN_PEER_OK){break;}
            n=(size_t)stn_wire_read(request+20,4);
            if(memcmp(request,"STNC",4)!=0 || n>STN_RPC_MAX_PAYLOAD){break;}
            io=transfer(&client->transport,request+24,n,0,0);
            if(io!=STN_PEER_OK){break;}
            EnterCriticalSection(client->dispatch_lock);
            dispatch=stn_rpc_dispatch(request,24+n,STN_RPC_READ|STN_RPC_SUBMISSION,
                client->service,response,STN_RPC_MAX_FRAME,&w);
            LeaveCriticalSection(client->dispatch_lock);
            if(dispatch!=STN_RPC_OK || transfer(&client->transport,response,w,1,0)!=STN_PEER_OK){break;}
        }
    }
    free(request);free(response);
    InterlockedExchange(&client->done,1);return 0;
}
static void reap_clients(rpc_client **head)
{
    rpc_client **link=head;
    while(*link!=NULL){
        rpc_client *client=*link;
        if(InterlockedCompareExchange(&client->done,0,0)!=0){
            (void)WaitForSingleObject(client->thread,INFINITE);
            (void)CloseHandle(client->thread);
            stn_windows_peer_close(&client->peer);
            *link=client->next;
            free(client);
        }else{link=&client->next;}
    }
}
static void stop_clients(rpc_client **head)
{
    rpc_client *client;
    for(client=*head;client!=NULL;client=client->next){stn_windows_peer_interrupt(&client->peer);}
    while(*head!=NULL){
        client=*head;
        (void)WaitForSingleObject(client->thread,INFINITE);
        (void)CloseHandle(client->thread);
            stn_windows_peer_close(&client->peer);
        *head=client->next;
        free(client);
    }
}
static int serve_once(stn_windows_peer *listener,const stn_rpc_service *service)
{
    stn_windows_peer peer={0};stn_peer_transport transport;uint8_t *request=NULL,*response=NULL;int ok=0;
    if(stn_windows_peer_accept(listener,APP_RPC_IO_TIMEOUT_MS,&peer,&transport)!=STN_PEER_OK){return 0;}
    request=(uint8_t*)malloc(STN_RPC_MAX_FRAME);response=(uint8_t*)malloc(STN_RPC_MAX_FRAME);
    if(request!=NULL && response!=NULL){
        for(;;){
            size_t n,w;stn_peer_status io=transfer(&transport,request,24,0,1);
            if(io==STN_PEER_TIMEOUT){continue;}if(io!=STN_PEER_OK){ok=1;break;}
            n=(size_t)stn_wire_read(request+20,4);
            if(memcmp(request,"STNC",4)!=0 || n>STN_RPC_MAX_PAYLOAD){break;}
            if(transfer(&transport,request+24,n,0,0)!=STN_PEER_OK){break;}
            if(stn_rpc_dispatch(request,24+n,STN_RPC_READ|STN_RPC_SUBMISSION,service,response,STN_RPC_MAX_FRAME,&w)!=STN_RPC_OK ||
               transfer(&transport,response,w,1,0)!=STN_PEER_OK){break;}
        }
    }
    free(request);free(response);stn_windows_peer_close(&peer);return ok;
}
/* Optional outbound lane with private scratch and existing dispatch exclusion. */
typedef struct outbound_runtime {
    stn_peer_outbound manager;stn_windows_peer socket;stn_peer_workspace workspace;
    stn_mining_service *mining;CRITICAL_SECTION *lock;HANDLE thread;
} outbound_runtime;
static DWORD WINAPI outbound_thread(void *user)
{
    outbound_runtime *runtime=user;
    while(InterlockedCompareExchange(&stopping,0,0)==0){
        EnterCriticalSection(runtime->lock);
        runtime->socket.operation_deadline_ms=GetTickCount64()+5000;
        (void)stn_peer_outbound_step(&runtime->manager,GetTickCount64(),runtime->mining->chain,
            runtime->mining->storage,&runtime->workspace,&runtime->mining->active);
        LeaveCriticalSection(runtime->lock);Sleep(100);
    }
    stn_peer_outbound_close(&runtime->manager);return 0;
}
static int candidate_argument(const char *text,stn_peer_candidates *set)
{
    stn_peer_endpoint endpoint;unsigned values[5]={0};size_t i;const char *p=text;
    for(i=0;i<5;++i){
        unsigned digits=0;
        while(*p>='0' && *p<='9'){
            if(++digits>(i==4 ? 5u : 3u)){return 0;}
            values[i]=values[i]*10u+(unsigned)(*p++-'0');
        }
        if(digits==0 || values[i]>(i==4 ? 65535u : 255u)){return 0;}
        if(i<4){if(*p++!=(i==3 ? ':' : '.')){return 0;}}
    }
    if(*p!='\0'){return 0;}
    for(i=0;i<4;++i){endpoint.address[i]=(uint8_t)values[i];}endpoint.port=(uint16_t)values[4];
    {stn_peer_status status=stn_peer_candidate_add(set,&endpoint);return status==STN_PEER_OK || status==STN_PEER_RETAINED;}
}
int stn_windows_app(int argc,char **argv)
{
    const char *data="stn-chain-dev.stns",*genesis_path=NULL,*transaction_path=NULL;
    int dev=0,once=0,i,result=EXIT_FAILURE;unsigned long port=18473;char *end;
    wchar_t relative[260],absolute[260];DWORD path_length;
    uint8_t *genesis=NULL,*body=NULL;
    size_t genesis_length=0,transaction_length=0;stn_block decoded;
    stn_chain_context chain={0};stn_pow_policy policy;stn_mining_service mining={0};stn_pending pending={0};
    stn_windows_storage disk;stn_storage_provider storage;stn_storage_view view;
    stn_windows_peer listener={0};uint16_t bound;stn_storage_status status;
    stn_rpc_service service={&mining,stn_mining_handle};
    rpc_client *clients=NULL;CRITICAL_SECTION dispatch_lock;int lock_ready=0;
    outbound_runtime outbound={0};stn_peer_candidates candidates={0};
    InterlockedExchange(&stopping,0);
    for(i=1;i<argc;++i){
        if(strcmp(argv[i],"--dev")==0){dev=1;}
        else if(strcmp(argv[i],"--peer")==0 && i+1<argc){if(!candidate_argument(argv[++i],&candidates)){goto usage;}}
        else if(strcmp(argv[i],"--once")==0){once=1;}
        else if(strcmp(argv[i],"--data")==0 && i+1<argc){data=argv[++i];}
        else if(strcmp(argv[i],"--genesis")==0 && i+1<argc){genesis_path=argv[++i];}
        else if(strcmp(argv[i],"--transaction")==0 && i+1<argc){transaction_path=argv[++i];}
        else if(strcmp(argv[i],"--rpc-port")==0 && i+1<argc){
            errno=0;port=strtoul(argv[++i],&end,10);
            if(errno!=0 || *end!='\0' || end==argv[i] || port>65535){goto usage;}
        }else{goto usage;}
    }
    if((dev && ((genesis_path==NULL)!=(transaction_path==NULL))) ||
       (!dev && (genesis_path==NULL || transaction_path!=NULL))){goto usage;}
    if(once && candidates.count!=0){goto usage;}
    genesis=malloc(STN_BLOCK_MAX_SIZE);body=malloc(STN_BLOCK_MAX_BODY);mining.template_bytes=malloc(STN_BLOCK_MAX_SIZE);
    if(genesis==NULL || body==NULL || mining.template_bytes==NULL){goto cleanup;}
    if(dev && genesis_path==NULL){development_genesis(genesis);genesis_length=364;memcpy(body,genesis+168,196);transaction_length=192;}
    else{
        if(!read_file(genesis_path,genesis,STN_BLOCK_MAX_SIZE,&genesis_length) ||
           (dev && !read_file(transaction_path,body+4,STN_TX_MAX_SIZE,&transaction_length))){fprintf(stderr,"Cannot read genesis/transaction.\n");goto cleanup;}
        stn_wire_write(body,4,transaction_length);
    }
    if(stn_block_decode(genesis,genesis_length,&decoded)!=STN_DATA_OK || decoded.header.version!=3){fprintf(stderr,"Invalid v3 genesis.\n");goto cleanup;}
    memcpy(chain.network_id,decoded.header.network_id,32);memcpy(policy.fixed_target,decoded.header.reserved_target,32);
    chain.genesis_bytes=genesis;chain.genesis_length=genesis_length;chain.pow_policy=&policy;chain.hash_provider.hash=stn_sha256;
    if(dev && (stn_block_body_validate_structure(body,4+transaction_length,1)!=STN_DATA_OK ||
       memcmp(body+24,chain.network_id,32)!=0)){fprintf(stderr,"Invalid selected transaction or network.\n");goto cleanup;}
    if(MultiByteToWideChar(CP_ACP,0,data,-1,relative,260)==0){goto cleanup;}
    path_length=GetFullPathNameW(relative,260,absolute,NULL);
    if(path_length==0 || path_length>=260 || stn_windows_storage_init(&disk,absolute,&storage)!=STN_STORAGE_OK){fprintf(stderr,"Data path requires a trusted existing local NTFS directory.\n");goto cleanup;}
    {
        size_t cap=initial_storage_capacity(absolute);
        if(cap<STN_STORAGE_OVERHEAD+4u+genesis_length){cap=STN_STORAGE_OVERHEAD+4u+genesis_length;}
        mining.snapshot=(uint8_t*)malloc(cap);mining.workspace.current_bytes=(uint8_t*)malloc(cap);mining.workspace.next_bytes=(uint8_t*)malloc(cap);
        if(mining.snapshot==NULL || mining.workspace.current_bytes==NULL || mining.workspace.next_bytes==NULL){goto cleanup;}
        mining.snapshot_capacity=cap;mining.workspace.current_capacity=cap;mining.workspace.next_capacity=cap;
    }
    mining.chain=&chain;mining.storage=&storage;mining.body=body;mining.body_length=4+transaction_length;mining.transaction_count=1;
    if(!dev){mining.pending=&pending;mining.pending_body=body;mining.pending_body_capacity=STN_BLOCK_MAX_BODY;mining.body=NULL;mining.body_length=0;mining.transaction_count=0;}
    mining.template_capacity=STN_BLOCK_MAX_SIZE;mining.owns_buffers=1;
#ifdef STN_PHASE9_TEST_RUNTIME
    if(!phase9_setup(&mining)){fprintf(stderr,"Test runtime requires its private stop event.\n");goto cleanup;}
#endif
    status=stn_storage_load(&chain,&storage,mining.snapshot,mining.snapshot_capacity,&view);
    if(status==STN_STORAGE_NOT_FOUND){stn_block_span anchor={genesis,genesis_length};
        status=stn_storage_create(&chain,&storage,&anchor,1,mining.snapshot,mining.snapshot_capacity,&mining.active);
    }else if(status==STN_STORAGE_OK){mining.active=view.state;stn_storage_view_release(&view);}
    if(status!=STN_STORAGE_OK){fprintf(stderr,"Storage startup failed: %d (no automatic repair).\n",(int)status);goto cleanup;}
    if(stn_windows_peer_listen((uint16_t)port,&listener,&bound)!=STN_PEER_OK){fprintf(stderr,"Cannot bind loopback RPC port.\n");goto cleanup;}
    if(!SetConsoleCtrlHandler(stop,TRUE)){goto cleanup;}
    InitializeCriticalSection(&dispatch_lock);lock_ready=1;
    printf("STN Chain development node; RPC 127.0.0.1:%u; height %llu\n",(unsigned)bound,(unsigned long long)mining.active.height);
    if(dev){puts("DEVELOPMENT FIXTURE: repeated structural test transaction; no signed intelligence admission or coin.");}
    puts("RPC v1 binary STNC; concurrent loopback clients limited only by host resources; read + solved-work submission. Ctrl+C stops.");fflush(stdout);
    result=EXIT_SUCCESS;
    if(candidates.count!=0){
        size_t cap=8u*1024u*1024u;stn_peer_connector connector={&outbound.socket,stn_windows_peer_open_candidate,stn_windows_peer_close_candidate};
        if(mining.snapshot_capacity>cap){cap=mining.snapshot_capacity;}
        outbound.workspace.storage.current_bytes=malloc(cap);outbound.workspace.storage.current_capacity=cap;
        outbound.workspace.storage.next_bytes=malloc(cap);outbound.workspace.storage.next_capacity=cap;
        outbound.workspace.candidate=malloc(cap);outbound.workspace.candidate_capacity=cap;
        outbound.workspace.frame=malloc(STN_PEER_MAX_FRAME);outbound.workspace.frame_capacity=STN_PEER_MAX_FRAME;
        outbound.workspace.pending=mining.pending;outbound.mining=&mining;outbound.lock=&dispatch_lock;
        if(outbound.workspace.storage.current_bytes==NULL || outbound.workspace.storage.next_bytes==NULL ||
            outbound.workspace.candidate==NULL || outbound.workspace.frame==NULL ||
            stn_peer_outbound_init(&outbound.manager,&candidates,&connector)!=STN_PEER_OK){result=EXIT_FAILURE;goto shutdown;}
        outbound.thread=CreateThread(NULL,0,outbound_thread,&outbound,0,NULL);
        if(outbound.thread==NULL){result=EXIT_FAILURE;goto shutdown;}
    }
    if(once){if(!serve_once(&listener,&service)){result=EXIT_FAILURE;}goto shutdown;}
    while(InterlockedCompareExchange(&stopping,0,0)==0){
#ifdef STN_PHASE9_TEST_RUNTIME
        if(WaitForSingleObject(phase9_stop_event,0)==WAIT_OBJECT_0){break;}
#endif
        stn_windows_peer peer={0};stn_peer_transport transport;rpc_client *client;
        stn_peer_status accepted=stn_windows_peer_accept(&listener,APP_RPC_ACCEPT_POLL_MS,&peer,&transport);
        if(accepted==STN_PEER_TIMEOUT){reap_clients(&clients);continue;}
        if(accepted!=STN_PEER_OK){reap_clients(&clients);Sleep(APP_RPC_ACCEPT_POLL_MS);continue;}
        peer.io_timeout_ms=APP_RPC_IO_TIMEOUT_MS;
        client=(rpc_client*)calloc(1,sizeof(*client));
        if(client==NULL){stn_windows_peer_close(&peer);continue;}
        client->peer=peer;client->transport=transport;client->transport.user=&client->peer;
        client->service=&service;client->dispatch_lock=&dispatch_lock;client->done=0;
        client->thread=CreateThread(NULL,0,rpc_client_thread,client,0,NULL);
        if(client->thread==NULL){stn_windows_peer_close(&client->peer);free(client);continue;}
        client->next=clients;clients=client;reap_clients(&clients);
    }
shutdown:
    InterlockedExchange(&stopping,1);
    if(outbound.thread!=NULL){WaitForSingleObject(outbound.thread,INFINITE);CloseHandle(outbound.thread);}
    stop_clients(&clients);(void)SetConsoleCtrlHandler(stop,FALSE);
cleanup:
#ifdef STN_PHASE9_TEST_RUNTIME
    if(phase9_stop_event!=NULL){CloseHandle(phase9_stop_event);phase9_stop_event=NULL;}
#endif
    stn_windows_peer_close(&listener);if(lock_ready){DeleteCriticalSection(&dispatch_lock);}
    free(outbound.workspace.storage.current_bytes);free(outbound.workspace.storage.next_bytes);free(outbound.workspace.candidate);free(outbound.workspace.frame);
    stn_pending_clear(&pending);free(genesis);free(body);free(mining.snapshot);free(mining.workspace.current_bytes);free(mining.workspace.next_bytes);free(mining.template_bytes);
    return result;
usage:
    puts("Usage: stn-chain --dev [--data PATH] [--rpc-port 18473]\n"
         "   or: stn-chain --genesis BLOCK --data PATH [--rpc-port PORT]\n"
         "Explicit selected content: --dev --genesis BLOCK --transaction STNT.\nRepeat --peer IPv4:PORT for automatic outbound P2P (not with --once).\nLoopback RPC only. --once serves one connection. Port 0 chooses a free port.");
    return argc==1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
