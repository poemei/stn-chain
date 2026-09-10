/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_rpc.h"
#include "stn_wire_internal.h"
#include <string.h>
static uint32_t capability(uint16_t method)
{
    switch(method){
    case STN_RPC_PENDING:case STN_RPC_INFO:case STN_RPC_BLOCK_HEIGHT:case STN_RPC_BLOCK_ID:
    case STN_RPC_CHECK_INTELLIGENCE:case STN_RPC_INTELLIGENCE_ID:case STN_RPC_INTELLIGENCE_CURSOR:
    case STN_RPC_MINING_CONTEXT:case STN_RPC_CHECK_WORK_BASE:case STN_RPC_MINING_TEMPLATE:return STN_RPC_READ;
    case STN_RPC_SUBMIT_TRANSACTION:case STN_RPC_SUBMIT_INTELLIGENCE:case STN_RPC_SUBMIT_WORK:return STN_RPC_SUBMISSION;
    case STN_RPC_ADMIN_CONTROL:return STN_RPC_ADMIN;
    default:return 0;
    }
}
static int shape(uint16_t method,const uint8_t *p,size_t n)
{
    switch(method){
    case STN_RPC_SUBMIT_TRANSACTION:return n<=STN_TX_MAX_SIZE;
    case STN_RPC_PENDING:case STN_RPC_INFO:case STN_RPC_MINING_CONTEXT:case STN_RPC_MINING_TEMPLATE:case STN_RPC_ADMIN_CONTROL:return n==0;
    case STN_RPC_BLOCK_HEIGHT:return n==8;
    case STN_RPC_BLOCK_ID:case STN_RPC_INTELLIGENCE_ID:case STN_RPC_CHECK_WORK_BASE:return n==32;
    case STN_RPC_INTELLIGENCE_CURSOR:return n==40;
    case STN_RPC_CHECK_INTELLIGENCE:case STN_RPC_SUBMIT_INTELLIGENCE:return n>=STN_RECORD_OVERHEAD && n<=STN_RECORD_MAX_SIZE;
    case STN_RPC_SUBMIT_WORK:return n>=68+STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY && n<=68+STN_BLOCK_MAX_SIZE && stn_wire_read(p+64,4)==n-68;
    default:return 1;
    }
}
static int response_shape(uint16_t method,const uint8_t *p,size_t n)
{
    switch(method){
    case STN_RPC_SUBMIT_TRANSACTION:return n==36 && stn_wire_read(p,2)==1 && stn_wire_read(p+2,2)<=8;
    case STN_RPC_MINING_TEMPLATE:return shape(STN_RPC_SUBMIT_WORK,p,n);
    case STN_RPC_SUBMIT_WORK:return n==80;
    case STN_RPC_SUBMIT_INTELLIGENCE:return n==56 && stn_wire_read(p,2)==1 && stn_wire_read(p+2,2)<=STN_PENDING_UNSUPPORTED;
    case STN_RPC_PENDING:return n==16 &&
        stn_wire_read(p,4)<=(uint64_t)STN_PENDING_MAX_ENTRIES &&
        stn_wire_read(p+4,4)==STN_PENDING_MAX_ENTRIES && stn_wire_read(p+8,4)<=STN_PENDING_MAX_BYTES &&
        stn_wire_read(p+n-4,4)==STN_PENDING_MAX_BYTES;
    case STN_RPC_INFO:return n==184;
    case STN_RPC_BLOCK_HEIGHT:case STN_RPC_BLOCK_ID:return n>=STN_BLOCK_HEADER_SIZE+STN_BLOCK_MIN_BODY && n<=STN_BLOCK_MAX_SIZE;
    case STN_RPC_CHECK_INTELLIGENCE:return n==20;
    case STN_RPC_MINING_CONTEXT:case STN_RPC_CHECK_WORK_BASE:return n==76;
    default:return 0;
    }
}
static int message_valid(const stn_rpc_message *m)
{
    if(m->length>STN_RPC_MAX_PAYLOAD || (m->length!=0 && m->payload==NULL) || m->code<STN_RPC_OK || m->code>STN_RPC_STALE){return 0;}
    if(m->kind==1){return m->code==STN_RPC_OK && shape(m->method,m->payload,m->length);}
    return m->kind==2 && (m->code==STN_RPC_OK ? response_shape(m->method,m->payload,m->length) : m->length==0);
}
stn_rpc_code stn_rpc_decode(const uint8_t *p,size_t n,stn_rpc_message *out)
{
    stn_rpc_message m;
    if(p==NULL || out==NULL || n<24 || n>STN_RPC_MAX_FRAME){return STN_RPC_INVALID;}
    if(memcmp(p,"STNC",4)!=0){return STN_RPC_INVALID;}
    if(stn_wire_read(p+4,2)!=2){return STN_RPC_VERSION;}
    m.kind=(uint16_t)stn_wire_read(p+6,2);m.method=(uint16_t)stn_wire_read(p+8,2);
    m.code=(stn_rpc_code)stn_wire_read(p+10,2);m.request_id=stn_wire_read(p+12,8);
    m.length=(size_t)stn_wire_read(p+20,4);m.payload=p+24;
    if(m.length!=n-24 || !message_valid(&m)){return STN_RPC_INVALID;}
    *out=m;return STN_RPC_OK;
}
stn_rpc_code stn_rpc_encode(const stn_rpc_message *m,uint8_t *p,size_t cap,size_t *written)
{
    if(written!=NULL){*written=0;}
    if(m==NULL || p==NULL || written==NULL || !message_valid(m)){return STN_RPC_INVALID;}
    if(cap<24 || m->length>cap-24){return STN_RPC_CAPACITY;}
    memcpy(p,"STNC",4);stn_wire_write(p+4,2,2);stn_wire_write(p+6,2,m->kind);
    stn_wire_write(p+8,2,m->method);stn_wire_write(p+10,2,m->code);stn_wire_write(p+12,8,m->request_id);stn_wire_write(p+20,4,m->length);
    if(m->length!=0){memmove(p+24,m->payload,m->length);}*written=24+m->length;return STN_RPC_OK;
}
stn_rpc_code stn_rpc_dispatch(const uint8_t *request,size_t length,uint32_t allowed,
    const stn_rpc_service *service,uint8_t *response,size_t cap,size_t *written)
{
    stn_rpc_message q,r={0};stn_rpc_code code;uint32_t required;size_t n=0;
    if(written!=NULL){*written=0;}
    if(response==NULL || written==NULL){return STN_RPC_INVALID;}
    if(cap<24){return STN_RPC_CAPACITY;}
    r.kind=2;code=stn_rpc_decode(request,length,&q);
    if(code!=STN_RPC_OK){r.code=code;return stn_rpc_encode(&r,response,cap,written);}
    if(q.kind!=1){r.code=STN_RPC_INVALID;return stn_rpc_encode(&r,response,cap,written);}
    r.method=q.method;r.request_id=q.request_id;required=capability(q.method);
    if(required==0){r.code=STN_RPC_METHOD;}
    else if((allowed & ~7u)!=0 || (allowed & required)==0){r.code=STN_RPC_FORBIDDEN;}
    else if(service==NULL || service->handle==NULL){r.code=STN_RPC_UNAVAILABLE;}
    else{
        size_t payload_capacity=cap-24;
        if(payload_capacity>STN_RPC_MAX_PAYLOAD){payload_capacity=STN_RPC_MAX_PAYLOAD;}
        r.code=service->handle(service->user,&q,response+24,payload_capacity,&n);
        if(r.code<STN_RPC_OK || r.code>STN_RPC_STALE || n>payload_capacity ||
            (r.code==STN_RPC_OK && !response_shape(q.method,response+24,n))){r.code=STN_RPC_PROVIDER;}
        if(r.code==STN_RPC_OK){r.payload=response+24;r.length=n;}
    }
    return stn_rpc_encode(&r,response,cap,written);
}
