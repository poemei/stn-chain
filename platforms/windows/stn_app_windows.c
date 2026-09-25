/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "../stn_backend.h"
#include "stn_windows_storage.h"
#include "stn_windows_peer.h"
#include "stn_mining.h"
#include "stn_internal_miner.h"
#include "stn_config.h"
#include "stn_sha256.h"
#include "../../src/stn_wire_internal.h"
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <io.h>
#include <fcntl.h>
#include <stdint.h>
#ifdef STN_PHASE9_TEST_RUNTIME
#include "../../tests/stn_phase9_runtime.h"
#endif
#define APP_STORAGE_INITIAL (STN_STORAGE_OVERHEAD+4u+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY)
#define APP_RPC_ACCEPT_POLL_MS 250u
#define APP_RPC_IO_TIMEOUT_MS 60000u
static volatile LONG stopping;
static uint64_t chain_timestamp_now(void *user)
{
    time_t now;
    (void)user;
    now=time(NULL);
    return now==(time_t)-1 || now<=0 ? 0u : (uint64_t)now;
}
static CRITICAL_SECTION report_lock;
static int report_lock_ready;
static FILE *report_log;
static BOOL WINAPI stop(DWORD event)
{
    if(event==CTRL_C_EVENT || event==CTRL_BREAK_EVENT){InterlockedExchange(&stopping,1);return TRUE;}
    return FALSE;
}
static int report_open(void)
{
    char directory[MAX_PATH];
    char path[MAX_PATH];
    const char *program_data=getenv("ProgramData");
    if(program_data==NULL || *program_data=='\0'){program_data="C:\\ProgramData";}
    if(_snprintf_s(directory,sizeof(directory),_TRUNCATE,"%s\\STN Chain",program_data)<0){return 0;}
    if(!CreateDirectoryA(directory,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS){return 0;}
    if(_snprintf_s(path,sizeof(path),_TRUNCATE,"%s\\stn-chain.log",directory)<0){return 0;}
    {
        HANDLE handle=CreateFileA(path,FILE_APPEND_DATA,
            FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,NULL);
        int fd;
        if(handle==INVALID_HANDLE_VALUE){return 0;}
        fd=_open_osfhandle((intptr_t)handle,_O_APPEND|_O_TEXT);
        if(fd<0){CloseHandle(handle);return 0;}
        report_log=_fdopen(fd,"a");
        if(report_log==NULL){_close(fd);return 0;}
        return 1;
    }
}
static void report_close(void)
{
    if(report_log!=NULL){fclose(report_log);report_log=NULL;}
    if(report_lock_ready){DeleteCriticalSection(&report_lock);report_lock_ready=0;}
}
static void report_line(const char *event,const char *format,...)
{
    char message[1024],stamp[32];time_t now;struct tm local;va_list args;
    now=time(NULL);
    if(localtime_s(&local,&now)==0){strftime(stamp,sizeof(stamp),"%Y-%m-%dT%H:%M:%S",&local);}
    else{strcpy_s(stamp,sizeof(stamp),"unknown-time");}
    va_start(args,format);vsnprintf_s(message,sizeof(message),_TRUNCATE,format,args);va_end(args);
    if(report_lock_ready){EnterCriticalSection(&report_lock);}
    fprintf(stdout,"[%s] %s\n",event,message);fflush(stdout);
    if(report_log!=NULL){fprintf(report_log,"%s [%s] %s\n",stamp,event,message);fflush(report_log);}
    if(report_lock_ready){LeaveCriticalSection(&report_lock);}
}
static int read_file(const char *path,uint8_t *bytes,size_t cap,size_t *n)
{
    FILE *f=NULL;int extra,bad;
    if(fopen_s(&f,path,"rb")!=0){return 0;}
    *n=fread(bytes,1,cap,f);extra=fgetc(f);bad=ferror(f);(void)fclose(f);
    return !bad && extra==EOF && *n!=0;
}
/* Read only the anchor; storage load subsequently validates the entire history. */
static int history_genesis(const wchar_t *path,uint8_t *bytes,size_t *length)
{
    uint8_t header[16];DWORD read_count;size_t size;int ok;
    BY_HANDLE_FILE_INFORMATION info;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(file==INVALID_HANDLE_VALUE){return GetLastError()==ERROR_FILE_NOT_FOUND ? 0 : -1;}
    ok=GetFileType(file)==FILE_TYPE_DISK && GetFileInformationByHandle(file,&info) &&
        !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT));
    if(ok){ok=ReadFile(file,header,sizeof(header),&read_count,NULL) && read_count==sizeof(header) &&
        memcmp(header,"STNS\0\1\0\0",8)==0 && stn_wire_read(header+8,4)!=0;}
    if(ok){
        size=(size_t)stn_wire_read(header+12,4);
        ok=size>=STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY && size<=STN_BLOCK_MAX_SIZE;
        if(ok){ok=ReadFile(file,bytes,(DWORD)size,&read_count,NULL) && read_count==size;}
        if(ok){*length=size;}
    }
    if(!CloseHandle(file)){ok=0;}
    return ok ? 1 : -1;
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
    memcpy(b+88,commitment,32);
    /* Deterministic bootstrap PoW target: 0000007f...; nonce 51449120. */
    memset(b+120,255,32);b[120]=0;b[121]=0;b[122]=0;b[123]=127;
    b[152]=0;b[153]=0;b[154]=0;b[155]=0;b[156]=3;b[157]=17;b[158]=13;b[159]=32;
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
static stn_peer_status rpc_transfer(stn_windows_peer *peer,const stn_peer_transport *transport,
    uint8_t *bytes,size_t length,int sending,int idle_allowed)
{
    stn_peer_status status;
    peer->operation_deadline_ms=GetTickCount64()+APP_RPC_IO_TIMEOUT_MS;
    status=transfer(transport,bytes,length,sending,idle_allowed);
    peer->operation_deadline_ms=0;
    return status;
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
            size_t n,w;stn_peer_status io=rpc_transfer(&client->peer,&client->transport,request,24,0,1);stn_rpc_code dispatch;
            if(io==STN_PEER_TIMEOUT){continue;}
            if(io!=STN_PEER_OK){break;}
            if(memcmp(request,"STNC",4)!=0 || stn_rpc_payload_length(request,24,&n)!=STN_RPC_OK){break;}
            io=rpc_transfer(&client->peer,&client->transport,request+24,n,0,0);
            if(io!=STN_PEER_OK){break;}
            EnterCriticalSection(client->dispatch_lock);
            dispatch=stn_rpc_dispatch(request,24+n,STN_RPC_READ|STN_RPC_SUBMISSION,
                client->service,response,STN_RPC_MAX_FRAME,&w);
            LeaveCriticalSection(client->dispatch_lock);
            if(dispatch!=STN_RPC_OK || rpc_transfer(&client->peer,&client->transport,response,w,1,0)!=STN_PEER_OK){break;}
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
            size_t n,w;stn_peer_status io=rpc_transfer(&peer,&transport,request,24,0,1);
            if(io==STN_PEER_TIMEOUT){continue;}if(io!=STN_PEER_OK){ok=1;break;}
            if(memcmp(request,"STNC",4)!=0 || stn_rpc_payload_length(request,24,&n)!=STN_RPC_OK){break;}
            if(rpc_transfer(&peer,&transport,request+24,n,0,0)!=STN_PEER_OK){break;}
            if(stn_rpc_dispatch(request,24+n,STN_RPC_READ|STN_RPC_SUBMISSION,service,response,STN_RPC_MAX_FRAME,&w)!=STN_RPC_OK ||
               rpc_transfer(&peer,&transport,response,w,1,0)!=STN_PEER_OK){break;}
        }
    }
    free(request);free(response);stn_windows_peer_close(&peer);return ok;
}
/* Optional outbound lane with private scratch and existing dispatch exclusion. */
typedef struct outbound_runtime {
    stn_peer_outbound manager;stn_windows_peer socket;stn_peer_workspace workspace;
    stn_mining_service *mining;CRITICAL_SECTION *lock;HANDLE thread;
    int connected_reported;uint64_t reported_height;
} outbound_runtime;

typedef struct internal_miner_runtime {
    stn_internal_miner_worker worker;CRITICAL_SECTION *lock;HANDLE thread;
} internal_miner_runtime;
static DWORD WINAPI outbound_thread(void *user)
{
    outbound_runtime *runtime=user;
    report_line("PEER","Outbound peer manager started.");
    while(InterlockedCompareExchange(&stopping,0,0)==0){
        EnterCriticalSection(runtime->lock);
        runtime->socket.operation_deadline_ms=GetTickCount64()+5000;
        {
            stn_peer_status peer_status=stn_peer_outbound_step(&runtime->manager,GetTickCount64(),
                runtime->mining->chain,runtime->mining->storage,&runtime->workspace,
                &runtime->mining->active);
            uint64_t height=runtime->mining->active.height;
            if(!runtime->connected_reported && peer_status==STN_PEER_OK){
                report_line("PEER","Connected to Chain peer.");
                runtime->connected_reported=1;
                runtime->reported_height=height;
            }
            if(runtime->connected_reported && height!=runtime->reported_height){
                report_line("SYNC","Chain height %llu.",(unsigned long long)height);
                runtime->reported_height=height;
            }
        }
        LeaveCriticalSection(runtime->lock);Sleep(100);
    }
    stn_peer_outbound_close(&runtime->manager);report_line("PEER","Outbound peer manager stopped.");return 0;
}
static DWORD WINAPI internal_miner_thread(void *user)
{
    internal_miner_runtime *runtime=user;
    report_line("MINER","Internal miner started duty=2%% nonce_budget=%llu.",
        (unsigned long long)runtime->worker.nonce_budget);
    while(InterlockedCompareExchange(&stopping,0,0)==0){
        stn_internal_miner_result mined=STN_INTERNAL_MINER_IDLE;
        stn_data_status status;
        ULONGLONG started=GetTickCount64(),elapsed,idle;
        EnterCriticalSection(runtime->lock);
        status=stn_internal_miner_worker_step(&runtime->worker,&mined);
        LeaveCriticalSection(runtime->lock);
        if(status!=STN_DATA_OK){
            report_line("ERROR","Internal miner step failed status=%d.",(int)status);
            Sleep(1000);continue;
        }
        if(mined==STN_INTERNAL_MINER_SHARE){report_line("MINER","Internal miner submitted share.");}
        else if(mined==STN_INTERNAL_MINER_BLOCK){report_line("MINER","Internal miner submitted block.");}
        elapsed=GetTickCount64()-started;
        if(elapsed==0){Sleep(1);}
        else{idle=elapsed*49u;if(idle>60000u)idle=60000u;Sleep((DWORD)idle);}
    }
    report_line("MINER","Internal miner stopped.");
    return 0;
}
static int load_chain_config(const char *path,stn_config *config)
{
    uint8_t bytes[STN_CONFIG_MAX_BYTES];size_t length=0;
    return read_file(path,bytes,sizeof(bytes),&length) &&
        stn_config_decode(bytes,length,config)==STN_DATA_OK;
}
static int candidate_argument(const char *text,stn_peer_candidates *set)
{
    char host[256],service[6];const char *colon;char *end;unsigned long port;
    ADDRINFOA hints,*resolved=NULL,*entry;int accepted=0;
    if(text==NULL || set==NULL){return 0;}
    colon=strrchr(text,':');
    if(colon==NULL || colon==text || colon[1]=='\0' || (size_t)(colon-text)>=sizeof(host)){return 0;}
    memcpy(host,text,(size_t)(colon-text));host[colon-text]='\0';
    errno=0;port=strtoul(colon+1,&end,10);
    if(errno!=0 || end==colon+1 || *end!='\0' || port==0 || port>65535){return 0;}
    _snprintf_s(service,sizeof(service),_TRUNCATE,"%lu",port);
    memset(&hints,0,sizeof(hints));hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
    if(getaddrinfo(host,service,&hints,&resolved)!=0){return 0;}
    for(entry=resolved;entry!=NULL;entry=entry->ai_next){
        const struct sockaddr_in *address;stn_peer_endpoint endpoint;uint32_t ipv4;stn_peer_status status;
        if(entry->ai_family!=AF_INET || entry->ai_addr==NULL || entry->ai_addrlen<(int)sizeof(struct sockaddr_in)){continue;}
        address=(const struct sockaddr_in *)entry->ai_addr;ipv4=ntohl(address->sin_addr.s_addr);
        endpoint.address[0]=(uint8_t)(ipv4>>24);endpoint.address[1]=(uint8_t)(ipv4>>16);
        endpoint.address[2]=(uint8_t)(ipv4>>8);endpoint.address[3]=(uint8_t)ipv4;endpoint.port=(uint16_t)port;
        status=stn_peer_candidate_add(set,&endpoint);
        if(status==STN_PEER_OK || status==STN_PEER_RETAINED){accepted=1;}
    }
    freeaddrinfo(resolved);return accepted;
}

int stn_windows_app(int argc,char **argv)
{
    InitializeCriticalSection(&report_lock);report_lock_ready=1;
    if(!report_open()){report_line("ERROR","Cannot open persistent log; terminal reporting remains active.");}
    const char *data="stn-chain-dev.stns",*genesis_path=NULL,*transaction_path=NULL,*config_path="config\\chain_config.json";
    int dev=0,once=0,i,result=EXIT_FAILURE;unsigned long port=18473;char *end;
    wchar_t relative[260],absolute[260];DWORD path_length;
    uint8_t *genesis=NULL,*body=NULL;
    size_t genesis_length=0,transaction_length=0;stn_block decoded;
    stn_chain_context chain={0};stn_pow_policy policy;stn_mining_service mining={0};stn_pending pending={0};
    stn_windows_storage disk;stn_storage_provider storage;stn_storage_view view;
    stn_windows_peer listener={0};uint16_t bound;stn_storage_status status;
    stn_rpc_service service={&mining,stn_mining_handle};
    rpc_client *clients=NULL;CRITICAL_SECTION dispatch_lock;int lock_ready=0;
    outbound_runtime outbound={0};internal_miner_runtime internal_miner={0};stn_peer_candidates candidates={0};stn_config config={0};
    InterlockedExchange(&stopping,0);
    for(i=1;i<argc;++i){
        if(strcmp(argv[i],"--dev")==0){dev=1;}
        else if(strcmp(argv[i],"--peer")==0 && i+1<argc){if(!candidate_argument(argv[++i],&candidates)){goto usage;}}
        else if(strcmp(argv[i],"--once")==0){once=1;}
        else if(strcmp(argv[i],"--config")==0 && i+1<argc){config_path=argv[++i];}
        else if(strcmp(argv[i],"--data")==0 && i+1<argc){data=argv[++i];}
        else if(strcmp(argv[i],"--genesis")==0 && i+1<argc){genesis_path=argv[++i];}
        else if(strcmp(argv[i],"--transaction")==0 && i+1<argc){transaction_path=argv[++i];}
        else if(strcmp(argv[i],"--rpc-port")==0 && i+1<argc){
            errno=0;port=strtoul(argv[++i],&end,10);
            if(errno!=0 || *end!='\0' || end==argv[i] || port>65535){goto usage;}
        }else{goto usage;}
    }
    if((dev && ((genesis_path==NULL)!=(transaction_path==NULL))) ||
       (!dev && transaction_path!=NULL)){goto usage;}
    if(once && candidates.count!=0){goto usage;}
    genesis=malloc(STN_BLOCK_MAX_SIZE);body=malloc(STN_BLOCK_MAX_BODY);mining.template_bytes=malloc(STN_BLOCK_MAX_SIZE);
    if(genesis==NULL || body==NULL || mining.template_bytes==NULL){goto cleanup;}
    if(MultiByteToWideChar(CP_ACP,0,data,-1,relative,260)==0){goto cleanup;}
    path_length=GetFullPathNameW(relative,260,absolute,NULL);
    if(path_length==0 || path_length>=260 || stn_windows_storage_init(&disk,absolute,&storage)!=STN_STORAGE_OK){fprintf(stderr,"Data path requires a trusted existing local NTFS directory.\n");goto cleanup;}
    if(genesis_path==NULL){
        int existing=dev ? 0 : history_genesis(absolute,genesis,&genesis_length);
        if(existing<0){fprintf(stderr,"Cannot read existing history genesis; history was not replaced.\n");goto cleanup;}
        if(existing==0){development_genesis(genesis);genesis_length=364;}
        if(dev){memcpy(body,genesis+168,196);transaction_length=192;}
    }
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
    {
        size_t cap=initial_storage_capacity(absolute);
        if(cap<STN_STORAGE_OVERHEAD+4u+genesis_length){cap=STN_STORAGE_OVERHEAD+4u+genesis_length;}
        mining.snapshot=(uint8_t*)malloc(cap);mining.workspace.current_bytes=(uint8_t*)malloc(cap);mining.workspace.next_bytes=(uint8_t*)malloc(cap);
        if(mining.snapshot==NULL || mining.workspace.current_bytes==NULL || mining.workspace.next_bytes==NULL){goto cleanup;}
        mining.snapshot_capacity=cap;mining.workspace.current_capacity=cap;mining.workspace.next_capacity=cap;
    }
    mining.chain=&chain;mining.storage=&storage;mining.timestamp_now=chain_timestamp_now;mining.body=body;mining.body_length=4+transaction_length;mining.transaction_count=1;
    if(!dev){mining.pending=&pending;mining.pending_body=body;mining.pending_body_capacity=STN_BLOCK_MAX_BODY;mining.body=NULL;mining.body_length=0;mining.transaction_count=0;}
    mining.template_capacity=STN_BLOCK_MAX_SIZE;mining.owns_buffers=1;
#ifdef STN_PHASE9_TEST_RUNTIME
    if(!phase9_setup(&mining)){fprintf(stderr,"Test runtime requires its private stop event.\n");goto cleanup;}
#endif
    status=stn_storage_load(&chain,&storage,mining.snapshot,mining.snapshot_capacity,&view);
    if(status==STN_STORAGE_NOT_FOUND){stn_block_span anchor={genesis,genesis_length};
        status=stn_storage_create(&chain,&storage,&anchor,1,mining.snapshot,mining.snapshot_capacity,&mining.active);
    }else if(status==STN_STORAGE_OK){stn_chain_state_move(&mining.active,&view.state);stn_storage_view_release(&view);}
    if(status!=STN_STORAGE_OK){fprintf(stderr,"Storage startup failed: %d (no automatic repair).\n",(int)status);goto cleanup;}
    if(stn_windows_peer_listen((uint16_t)port,&listener,&bound)!=STN_PEER_OK){fprintf(stderr,"Cannot bind loopback RPC port.\n");goto cleanup;}
    if(!SetConsoleCtrlHandler(stop,TRUE)){goto cleanup;}
    InitializeCriticalSection(&dispatch_lock);lock_ready=1;
    if(!load_chain_config(config_path,&config)){
        report_line("ERROR","Cannot read valid Chain configuration: %s",config_path);goto cleanup;
    }
    if(config.internal_miner_enabled){
        internal_miner.worker.service=&mining;
        internal_miner.worker.miner.type=STN_ADDRESS_IDENTITY;
        memcpy(internal_miner.worker.miner.identifier,config.miner_wallet.identifier,STN_ADDRESS_ID_SIZE);
        internal_miner.worker.nonce_budget=STN_INTERNAL_MINER_DEFAULT_NONCE_BUDGET;
        internal_miner.worker.duty_permille=STN_INTERNAL_MINER_DEFAULT_DUTY_PERMILLE;
        internal_miner.lock=&dispatch_lock;
        internal_miner.thread=CreateThread(NULL,0,internal_miner_thread,&internal_miner,0,NULL);
        if(internal_miner.thread==NULL){report_line("ERROR","Cannot start internal miner.");goto cleanup;}
    }else{report_line("MINER","Internal miner disabled.");}
    report_line("START","RPC 127.0.0.1:%u height=%llu",(unsigned)bound,(unsigned long long)mining.active.height);
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
        report_line("PEER","Starting outbound peer manager with %u candidate(s).",(unsigned)candidates.count);
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
    /* Close the listener before joining workers: shutdown must stop new
     * accepts immediately while active client sessions are interrupted below. */
    stn_windows_peer_close(&listener);
    if(outbound.thread!=NULL){WaitForSingleObject(outbound.thread,INFINITE);CloseHandle(outbound.thread);}
    if(internal_miner.thread!=NULL){WaitForSingleObject(internal_miner.thread,INFINITE);CloseHandle(internal_miner.thread);}
    stop_clients(&clients);(void)SetConsoleCtrlHandler(stop,FALSE);
cleanup:
#ifdef STN_PHASE9_TEST_RUNTIME
    if(phase9_stop_event!=NULL){CloseHandle(phase9_stop_event);phase9_stop_event=NULL;}
#endif
    stn_windows_peer_close(&listener);if(lock_ready){DeleteCriticalSection(&dispatch_lock);}
    free(outbound.workspace.storage.current_bytes);free(outbound.workspace.storage.next_bytes);free(outbound.workspace.candidate);free(outbound.workspace.frame);
    stn_chain_state_release(&mining.active);stn_pending_clear(&pending);free(genesis);free(body);free(mining.snapshot);free(mining.workspace.current_bytes);free(mining.workspace.next_bytes);free(mining.template_bytes);
    report_line("STOP","STN Chain Windows stopped.");report_close();
    return result;
usage:
    puts("Usage: stn-chain [--data PATH] [--rpc-port PORT] [--peer HOST:PORT] [--config PATH]\n"
         "Resume saved history or initialize the built-in genesis if absent.\n"
         "Repeat --peer HOST:PORT for automatic outbound P2P. Host may be DNS or IPv4.\n"
         "Loopback RPC only. Ctrl+C stops.");
    return argc==1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
