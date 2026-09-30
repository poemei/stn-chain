/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_response.h"
#include <string.h>

static const uint8_t response_magic[4]={'S','T','R','P'};
static void put16(uint8_t *p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static uint16_t get16(const uint8_t *p){return (uint16_t)(((uint16_t)p[0]<<8)|p[1]);}
static uint32_t get32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}

stn_contract_status stn_contract_response_decode(const uint8_t *input,size_t length,stn_contract_response *out)
{
    stn_contract_response r;uint32_t n;
    if(input==NULL||out==NULL)return STN_CONTRACT_ARGUMENT;
    if(length<STN_CONTRACT_RESPONSE_HEADER_SIZE)return STN_CONTRACT_TRUNCATED;
    if(memcmp(input,response_magic,4)!=0)return STN_CONTRACT_MAGIC;
    memset(&r,0,sizeof(r));r.version=get16(input+4);
    if(r.version!=STN_CONTRACT_RESPONSE_VERSION||get16(input+6)!=0u)return STN_CONTRACT_VERSION_ERROR;
    n=get32(input+72);if(n==0u||n>STN_CONTRACT_RESPONSE_MAX_TEXT)return STN_CONTRACT_TERMS_LIMIT;
    if(length!=STN_CONTRACT_RESPONSE_HEADER_SIZE+(size_t)n)return STN_CONTRACT_LENGTH;
    memcpy(r.contract_id,input+8,32);memcpy(r.actor,input+40,32);r.text_length=n;
    memcpy(r.signature,input+76,64);r.text=input+STN_CONTRACT_RESPONSE_HEADER_SIZE;*out=r;return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_response_statement(const stn_contract_response *r,uint8_t *out,size_t capacity,size_t *written)
{
    size_t total;if(written!=NULL)*written=0;
    if(r==NULL||out==NULL||written==NULL||r->text==NULL)return STN_CONTRACT_ARGUMENT;
    if(r->version!=STN_CONTRACT_RESPONSE_VERSION)return STN_CONTRACT_VERSION_ERROR;
    if(r->text_length==0u||r->text_length>STN_CONTRACT_RESPONSE_MAX_TEXT)return STN_CONTRACT_TERMS_LIMIT;
    total=STN_CONTRACT_RESPONSE_UNSIGNED_HEADER_SIZE+(size_t)r->text_length;if(capacity<total)return STN_CONTRACT_CAPACITY;
    memcpy(out,response_magic,4);put16(out+4,r->version);put16(out+6,0);memcpy(out+8,r->contract_id,32);memcpy(out+40,r->actor,32);put32(out+72,r->text_length);memcpy(out+76,r->text,r->text_length);*written=total;return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_response_encode(const stn_contract_response *r,uint8_t *out,size_t capacity,size_t *written)
{
    size_t total;if(written!=NULL)*written=0;
    if(r==NULL||out==NULL||written==NULL||r->text==NULL)return STN_CONTRACT_ARGUMENT;
    if(r->version!=STN_CONTRACT_RESPONSE_VERSION)return STN_CONTRACT_VERSION_ERROR;
    if(r->text_length==0u||r->text_length>STN_CONTRACT_RESPONSE_MAX_TEXT)return STN_CONTRACT_TERMS_LIMIT;
    total=STN_CONTRACT_RESPONSE_HEADER_SIZE+(size_t)r->text_length;if(capacity<total)return STN_CONTRACT_CAPACITY;
    memcpy(out,response_magic,4);put16(out+4,r->version);put16(out+6,0);memcpy(out+8,r->contract_id,32);memcpy(out+40,r->actor,32);put32(out+72,r->text_length);memcpy(out+76,r->signature,64);memcpy(out+STN_CONTRACT_RESPONSE_HEADER_SIZE,r->text,r->text_length);*written=total;return STN_CONTRACT_OK;
}

stn_contract_status stn_contract_response_validate_structure(const uint8_t *input,size_t length){stn_contract_response r;return stn_contract_response_decode(input,length,&r);}

stn_contract_status stn_contract_response_participant_validate(const stn_contract_response *r,const uint8_t *draft_bytes,size_t draft_length)
{
    stn_contract draft;stn_contract_participant p;stn_address address,identity;uint16_t i;stn_contract_status s;
    if(r==NULL||draft_bytes==NULL)return STN_CONTRACT_ARGUMENT;
    s=stn_contract_decode(draft_bytes,draft_length,&draft);if(s!=STN_CONTRACT_OK)return s;
    if(draft.state!=STN_CONTRACT_STATE_DRAFT||draft.sequence!=0u)return STN_CONTRACT_STATE_ERROR;
    s=stn_contract_address(draft_bytes,draft_length,&address);if(s!=STN_CONTRACT_OK)return s;
    if(memcmp(address.identifier,r->contract_id,32)!=0)return STN_CONTRACT_ADDRESS_ERROR;
    if(stn_address_derive(STN_ADDRESS_IDENTITY,r->actor,32,&identity)!=STN_DATA_OK)return STN_CONTRACT_SIGNATURE_ERROR;
    for(i=0;i<draft.participant_count;++i){s=stn_contract_participant_at(&draft,i,&p);if(s!=STN_CONTRACT_OK)return s;if(memcmp(p.identity,identity.identifier,32)==0)return STN_CONTRACT_OK;}
    return STN_CONTRACT_AUTHORITY_ERROR;
}

stn_contract_status stn_contract_response_signature_verify(const stn_contract_response *r)
{
    uint8_t unsigned_bytes[STN_CONTRACT_RESPONSE_UNSIGNED_HEADER_SIZE+STN_CONTRACT_RESPONSE_MAX_TEXT];
    uint8_t statement[STN_IDENTITY_DOMAIN_SIZE+sizeof(unsigned_bytes)];size_t n=0u,m=0u;stn_contract_status s;
    if(r==NULL)return STN_CONTRACT_ARGUMENT;
    s=stn_contract_response_statement(r,unsigned_bytes,sizeof(unsigned_bytes),&n);if(s!=STN_CONTRACT_OK)return s;
    if(stn_identity_statement(unsigned_bytes,n,statement,sizeof(statement),&m)!=STN_IDENTITY_VALID)return STN_CONTRACT_SIGNATURE_ERROR;
    return stn_identity_verify(r->actor,statement,m,r->signature,STN_IDENTITY_SIGNATURE_SIZE)==STN_IDENTITY_VALID?STN_CONTRACT_OK:STN_CONTRACT_SIGNATURE_ERROR;
}
