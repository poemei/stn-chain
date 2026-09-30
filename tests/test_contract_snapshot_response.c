/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_snapshot.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"contract snapshot response test failed: %d\n",__LINE__);return 1;}}while(0)

int main(void)
{
    stn_contract_snapshot *snapshot,*clone;
    stn_contract_snapshot_response got;
    stn_contract_response response;
    uint8_t encoded[256];
    static const uint8_t text[]="Response survives accepted snapshot cloning.";
    size_t written=0u,i;
    memset(&response,0,sizeof(response));
    response.version=STN_CONTRACT_RESPONSE_VERSION;
    for(i=0u;i<STN_ADDRESS_ID_SIZE;++i)response.contract_id[i]=(uint8_t)(0x20u+i);
    for(i=0u;i<STN_IDENTITY_PUBLIC_KEY_SIZE;++i)response.actor[i]=(uint8_t)(0x60u+i);
    for(i=0u;i<STN_IDENTITY_SIGNATURE_SIZE;++i)response.signature[i]=(uint8_t)(0xa0u+i);
    response.text=text;response.text_length=(uint32_t)(sizeof(text)-1u);
    CHECK(stn_contract_response_encode(&response,encoded,sizeof(encoded),&written)==STN_CONTRACT_OK);
    snapshot=stn_contract_snapshot_create();CHECK(snapshot!=NULL);
    CHECK(stn_contract_snapshot_response_register(snapshot,encoded,written)==STN_CONTRACT_OK);
    CHECK(stn_contract_snapshot_response_count(snapshot,response.contract_id)==1u);
    CHECK(stn_contract_snapshot_response_at(snapshot,response.contract_id,0u,&got)==STN_CONTRACT_OK);
    CHECK(got.text_length==sizeof(text)-1u && memcmp(got.text,text,sizeof(text)-1u)==0);
    clone=stn_contract_snapshot_clone(snapshot);CHECK(clone!=NULL);
    stn_contract_snapshot_release(snapshot);
    CHECK(stn_contract_snapshot_response_count(clone,response.contract_id)==1u);
    CHECK(stn_contract_snapshot_response_at(clone,response.contract_id,0u,&got)==STN_CONTRACT_OK);
    CHECK(memcmp(got.actor,response.actor,STN_IDENTITY_PUBLIC_KEY_SIZE)==0);
    CHECK(got.text_length==sizeof(text)-1u && memcmp(got.text,text,sizeof(text)-1u)==0);
    CHECK(stn_contract_snapshot_response_register(clone,encoded,written)==STN_CONTRACT_DUPLICATE_APPROVAL);
    stn_contract_snapshot_release(clone);
    puts("contract snapshot response tests passed");
    return 0;
}
