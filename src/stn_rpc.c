/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_rpc.h"
#include "stn_wire_internal.h"
#include "stn_sha256.h"
#include "stn_sentinel_intelligence.h"
#include "stn_address.h"
#include "stn_contract.h"
#include <string.h>

stn_rpc_code stn_rpc_payload_length(const uint8_t *p,size_t n,size_t *payload_length)
{
    size_t declared;
    if(p==NULL || payload_length==NULL || n!=STN_RPC_HEADER_SIZE){return STN_RPC_INVALID;}
    declared=(size_t)stn_wire_read(p+20,4);
    if(declared>STN_RPC_MAX_PAYLOAD){return STN_RPC_CAPACITY;}
    *payload_length=declared;return STN_RPC_OK;
}

static uint32_t capability(uint16_t method)
{
    switch(method){
    case STN_RPC_GET_CURSOR_REORG_STATUS:case STN_RPC_GET_CONSUMER_RECOVERY_PLAN:
    case STN_RPC_GET_FIRST_ACCEPTED_RECORD:case STN_RPC_GET_NEXT_ACCEPTED_RECORD:
    case STN_RPC_DERIVE_ADDRESS:case STN_RPC_BALANCE:case STN_RPC_CONTRACT_STATE:
    case STN_RPC_PENDING:case STN_RPC_INFO:case STN_RPC_BLOCK_HEIGHT:case STN_RPC_BLOCK_ID:case STN_RPC_GET_ACCEPTED_RECORD:
    case STN_RPC_CHECK_INTELLIGENCE:case STN_RPC_INTELLIGENCE_ID:case STN_RPC_INTELLIGENCE_CURSOR:
    case STN_RPC_MINING_CONTEXT:case STN_RPC_CHECK_WORK_BASE:case STN_RPC_MINING_TEMPLATE:return STN_RPC_READ;
    case STN_RPC_SUBMIT_TRANSACTION:case STN_RPC_SUBMIT_INTELLIGENCE:case STN_RPC_SUBMIT_WORK:case STN_RPC_SUBMIT_SHARE:return STN_RPC_SUBMISSION;
    case STN_RPC_ADMIN_CONTROL:return STN_RPC_ADMIN;
    default:return 0;
    }
}

static int shape(uint16_t method,const uint8_t *p,size_t n)
{
    switch(method){
    case STN_RPC_GET_FIRST_ACCEPTED_RECORD:return n==0;
    case STN_RPC_GET_CURSOR_REORG_STATUS:case STN_RPC_GET_CONSUMER_RECOVERY_PLAN:
    case STN_RPC_GET_NEXT_ACCEPTED_RECORD:return n==45;
    case STN_RPC_DERIVE_ADDRESS:
        return n>=6 &&
            (stn_wire_read(p,2)==STN_ADDRESS_IDENTITY ||
             stn_wire_read(p,2)==STN_ADDRESS_CONTRACT ||
             stn_wire_read(p,2)==STN_ADDRESS_WALLET) &&
            stn_wire_read(p+2,4)==n-6;
    case STN_RPC_BALANCE:{
        stn_address address;
        return n==70 &&
            stn_address_decode((const char *)p,n,&address)==STN_DATA_OK &&
            address.type==STN_ADDRESS_WALLET;
    }
    case STN_RPC_CONTRACT_STATE:{
        stn_address address;
        return n==70 &&
            stn_address_decode((const char *)p,n,&address)==STN_DATA_OK &&
            address.type==STN_ADDRESS_CONTRACT;
    }
    case STN_RPC_SUBMIT_TRANSACTION:return n<=STN_TX_MAX_SIZE;
    case STN_RPC_PENDING:case STN_RPC_INFO:case STN_RPC_MINING_CONTEXT:case STN_RPC_MINING_TEMPLATE:case STN_RPC_ADMIN_CONTROL:return n==0;
    case STN_RPC_BLOCK_HEIGHT:return n==8;
    case STN_RPC_BLOCK_ID:case STN_RPC_INTELLIGENCE_ID:case STN_RPC_CHECK_WORK_BASE:case STN_RPC_GET_ACCEPTED_RECORD:return n==32;
    case STN_RPC_INTELLIGENCE_CURSOR:return n==40;
    case STN_RPC_CHECK_INTELLIGENCE:case STN_RPC_SUBMIT_INTELLIGENCE:return n>=STN_RECORD_OVERHEAD && n<=STN_RECORD_MAX_SIZE;
    case STN_RPC_SUBMIT_SHARE:{
        stn_address miner;
        return n==STN_RPC_SHARE_SUBMISSION_SIZE &&
            stn_address_decode((const char *)(p+32),STN_RPC_MINER_IDENTITY_SIZE,&miner)==STN_DATA_OK &&
            miner.type==STN_ADDRESS_IDENTITY &&
            stn_block_header_validate_structure(p+STN_RPC_SHARE_SUBMISSION_PREFIX,STN_BLOCK_HEADER_SIZE)==STN_DATA_OK;
    }
    case STN_RPC_SUBMIT_WORK:{
        stn_address miner;
        return n>=STN_RPC_MINING_SUBMISSION_PREFIX+STN_BLOCK_HEADER_SIZE &&
            n<=STN_RPC_MINING_SUBMISSION_PREFIX+STN_BLOCK_MAX_SIZE &&
            stn_wire_read(p+64,4)==n-STN_RPC_MINING_SUBMISSION_PREFIX &&
            stn_address_decode((const char *)(p+68),STN_RPC_MINER_IDENTITY_SIZE,&miner)==STN_DATA_OK &&
            miner.type==STN_ADDRESS_IDENTITY;
    }
    default:return 1;
    }
}

static int response_shape(uint16_t method,const uint8_t *p,size_t n)
{
    switch(method){
    case STN_RPC_GET_FIRST_ACCEPTED_RECORD:case STN_RPC_GET_NEXT_ACCEPTED_RECORD:{
        stn_transaction tx;stn_record record;stn_sentinel_intelligence payload;stn_chain_cursor cursor;
        stn_hash_provider hash={stn_sha256,NULL};uint8_t id[32];
        if(n<125 || n-125>STN_TX_MAX_SIZE || stn_wire_read(p+121,4)!=n-125)return 0;
        if(stn_chain_cursor_decode(p+76,45,&cursor)!=STN_CURSOR_VALID ||
            cursor.height!=stn_wire_read(p+32,8) || memcmp(cursor.block_id,p+40,32)!=0 ||
            cursor.transaction_position!=stn_wire_read(p+72,4))return 0;
        return stn_transaction_decode(p+125,n-125,&tx)==STN_DATA_OK && tx.type==STN_TX_PUBLICATION &&
            stn_record_decode(tx.record_bytes,tx.record_length,&record)==STN_RECORD_OK &&
            stn_sentinel_intelligence_decode(record.payload,record.payload_length,&payload)==STN_SENTINEL_INTELLIGENCE_OK &&
            stn_record_id(tx.record_bytes,tx.record_length,&hash,id)==STN_DATA_OK && memcmp(id,p,32)==0;
    }

    case STN_RPC_GET_ACCEPTED_RECORD:{
        stn_transaction tx;stn_record record;stn_sentinel_intelligence payload;uint8_t id[32];
        stn_hash_provider hash={stn_sha256,NULL};
        if(n<STN_RPC_ACCEPTED_RECORD_PREFIX || n-STN_RPC_ACCEPTED_RECORD_PREFIX>STN_TX_MAX_SIZE ||
            stn_wire_read(p+72,4)!=n-STN_RPC_ACCEPTED_RECORD_PREFIX)return 0;
        return stn_transaction_decode(p+76,n-76,&tx)==STN_DATA_OK && tx.type==STN_TX_PUBLICATION &&
            stn_record_decode(tx.record_bytes,tx.record_length,&record)==STN_RECORD_OK &&
            stn_sentinel_intelligence_decode(record.payload,record.payload_length,&payload)==STN_SENTINEL_INTELLIGENCE_OK &&
            stn_record_id(tx.record_bytes,tx.record_length,&hash,id)==STN_DATA_OK && memcmp(id,p,32)==0;
    }

    case STN_RPC_DERIVE_ADDRESS:{
        stn_address address;
        return (n==69 || n==70) &&
            stn_address_decode((const char *)p,n,&address)==STN_DATA_OK;
    }

    case STN_RPC_BALANCE:
        return n==8;

    case STN_RPC_CONTRACT_STATE:
        return n==26 &&
            stn_wire_read(p,2)>=STN_CONTRACT_STATE_DRAFT &&
            stn_wire_read(p,2)<=STN_CONTRACT_STATE_CLOSED &&
            stn_wire_read(p+2,2)>=STN_CONTRACT_GENERIC &&
            stn_wire_read(p+2,2)<=STN_CONTRACT_SERVICE_AGREEMENT &&
            stn_wire_read(p+20,2)<=STN_CONTRACT_MAX_PARTICIPANTS &&
            stn_wire_read(p+22,4)<=STN_CONTRACT_MAX_TERMS;

    case STN_RPC_SUBMIT_TRANSACTION:
        return n==36 &&
            stn_wire_read(p,2)==1 &&
            stn_wire_read(p+2,2)<=8;

    /*
     * A mining-template response uses the same envelope as SUBMIT_WORK:
     * tip ID + work ID + encoded block length + canonical block.
     *
     * The canonical block may be header-only when its transaction count
     * and body length are both zero.
     */
    case STN_RPC_MINING_TEMPLATE:
        return n>=68+STN_BLOCK_HEADER_SIZE &&
            n<=68+STN_BLOCK_MAX_SIZE &&
            stn_wire_read(p+64,4)==n-68;

    case STN_RPC_SUBMIT_WORK:
        return n==80;

    case STN_RPC_SUBMIT_SHARE:
        return n==32;

    case STN_RPC_SUBMIT_INTELLIGENCE:
        return n==56 &&
            stn_wire_read(p,2)==1 &&
            stn_wire_read(p+2,2)<=STN_PENDING_UNSUPPORTED;

    case STN_RPC_PENDING:
        return n==16 &&
            stn_wire_read(p,4)<=(uint64_t)STN_PENDING_MAX_ENTRIES &&
            stn_wire_read(p+4,4)==STN_PENDING_MAX_ENTRIES &&
            stn_wire_read(p+8,4)<=STN_PENDING_MAX_BYTES &&
            stn_wire_read(p+n-4,4)==STN_PENDING_MAX_BYTES;

    case STN_RPC_INFO:
        return n==184;

    /*
     * Accepted blocks may be transaction-bearing or canonical empty
     * blocks. Structural validity of the block itself remains the
     * responsibility of the Chain/block validation layer.
     */
    case STN_RPC_BLOCK_HEIGHT:
    case STN_RPC_BLOCK_ID:
        return n>=STN_BLOCK_HEADER_SIZE &&
            n<=STN_BLOCK_MAX_SIZE;

    case STN_RPC_CHECK_INTELLIGENCE:
        return n==20;

    case STN_RPC_MINING_CONTEXT:
    case STN_RPC_CHECK_WORK_BASE:
        return n==76;

    default:return 0;
    }
}

static int recovery_response(uint16_t method,stn_rpc_code code,const uint8_t *p,size_t n)
{
    stn_chain_cursor cursor;
    if(code==STN_RPC_CURRENT)return (method==7 || method==8) && n==0;
    if(code==STN_RPC_COMMON_ANCESTOR)return method==7 && n==40 && p!=NULL;
    if(code==STN_RPC_NO_COMMON_ANCESTOR)return method==7 && n==0;
    if(code==STN_RPC_RECOVER_FROM_START)return method==8 && n==40 && p!=NULL;
    if(code==STN_RPC_RECOVER_AFTER_CURSOR)return method==8 && n==85 && p!=NULL &&
        stn_chain_cursor_decode(p+40,45,&cursor)==STN_CURSOR_VALID && cursor.height<=stn_wire_read(p,8) &&
        (cursor.height!=stn_wire_read(p,8) || memcmp(cursor.block_id,p+8,32)==0);
    return 0;
}

static int message_valid(const stn_rpc_message *m)
{
    if(m->length>STN_RPC_MAX_PAYLOAD || (m->length!=0 && m->payload==NULL) || m->code<STN_RPC_OK || m->code>STN_RPC_RECOVER_FROM_START){return 0;}
    if((m->code==STN_RPC_END && m->method!=5 && m->method!=6) ||
        (m->code==STN_RPC_DETACHED && m->method!=6))return 0;
    if(m->kind==1){return m->code==STN_RPC_OK && shape(m->method,m->payload,m->length);}
    if(m->kind==2 && m->code>=STN_RPC_CURRENT)return recovery_response(m->method,m->code,m->payload,m->length);
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
        if(r.code<STN_RPC_OK || r.code>STN_RPC_RECOVER_FROM_START ||
            (r.code==STN_RPC_END && q.method!=STN_RPC_GET_FIRST_ACCEPTED_RECORD && q.method!=STN_RPC_GET_NEXT_ACCEPTED_RECORD) ||
            (r.code==STN_RPC_DETACHED && q.method!=STN_RPC_GET_NEXT_ACCEPTED_RECORD) || n>payload_capacity ||
            (r.code>=STN_RPC_CURRENT && !recovery_response(q.method,r.code,response+24,n)) ||
            (r.code==STN_RPC_OK && !response_shape(q.method,response+24,n))){r.code=STN_RPC_PROVIDER;}
        if(r.code==STN_RPC_OK || r.code==STN_RPC_COMMON_ANCESTOR || r.code==STN_RPC_RECOVER_FROM_START || r.code==STN_RPC_RECOVER_AFTER_CURSOR){r.payload=response+24;r.length=n;}
    }
    return stn_rpc_encode(&r,response,cap,written);
}