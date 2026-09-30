/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_response_query.h"
#include "stn_wire_internal.h"
#include <string.h>
int stn_contract_response_query_request_valid(const uint8_t *b,size_t n)
{
    stn_address address;
    return b!=NULL && n==74u && stn_address_decode((const char *)b,70,&address)==STN_DATA_OK && address.type==STN_ADDRESS_CONTRACT;
}
int stn_contract_response_query_reply_valid(const uint8_t *b,size_t n)
{
    uint64_t total,length;
    if(b==NULL || n<8u)return 0;
    total=stn_wire_read(b,4);length=stn_wire_read(b+4,4);
    if(n-8u!=length || total>STN_CONTRACT_SNAPSHOT_MAX_RESPONSES)return 0;
    if(total==0u)return length==0u;
    return stn_contract_response_validate_structure(b+8,(size_t)length)==STN_CONTRACT_OK;
}
stn_rpc_code stn_contract_response_query(const stn_contract_snapshot *snapshot,
    const uint8_t *request,size_t length,uint8_t *out,size_t capacity,size_t *written)
{
    stn_address address;const stn_contract_state_store *store;
    stn_contract_snapshot_response saved;stn_contract_response response={0};
    size_t at=0,total,index,n=0;stn_contract_status status;
    if(written!=NULL)*written=0;
    if(out==NULL || written==NULL)return STN_RPC_PROVIDER;
    if(!stn_contract_response_query_request_valid(request,length))return STN_RPC_INVALID;
    if(snapshot==NULL)return STN_RPC_UNAVAILABLE;
    if(stn_address_decode((const char *)request,70,&address)!=STN_DATA_OK)return STN_RPC_INVALID;
    store=stn_contract_snapshot_const_state(snapshot);
    status=stn_contract_state_find(store,address.identifier,&at);
    if(status==STN_CONTRACT_ADDRESS_ERROR)return STN_RPC_NOT_FOUND;
    if(status!=STN_CONTRACT_OK)return STN_RPC_PROVIDER;
    index=(size_t)stn_wire_read(request+70,4);
    total=stn_contract_snapshot_response_count(snapshot,address.identifier);
    if(index>=total && (index!=0 || total!=0))return STN_RPC_NOT_FOUND;
    if(capacity<8u)return STN_RPC_CAPACITY;
    if(total!=0){
        if(stn_contract_snapshot_response_at(snapshot,address.identifier,index,&saved)!=STN_CONTRACT_OK)return STN_RPC_PROVIDER;
        response.version=STN_CONTRACT_RESPONSE_VERSION;
        memcpy(response.contract_id,saved.contract_id,32);memcpy(response.actor,saved.actor,32);
        memcpy(response.signature,saved.signature,64);response.text=saved.text;response.text_length=saved.text_length;
        status=stn_contract_response_encode(&response,out+8,capacity-8,&n);
        if(status!=STN_CONTRACT_OK)return status==STN_CONTRACT_CAPACITY?STN_RPC_CAPACITY:STN_RPC_PROVIDER;
    }
    stn_wire_write(out,4,total);stn_wire_write(out+4,4,n);*written=8u+n;return STN_RPC_OK;
}
