/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_response.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do{if(!(x)){fprintf(stderr,"contract response test failed: %d\n",__LINE__);return 1;}}while(0)

int main(void)
{
    stn_contract_response in,out;uint8_t encoded[512];size_t n=0;static const uint8_t text[]="Accepted. Proceed as written.";size_t i;
    memset(&in,0,sizeof(in));in.version=STN_CONTRACT_RESPONSE_VERSION;for(i=0;i<32;i++){in.contract_id[i]=(uint8_t)(i+1u);in.actor[i]=(uint8_t)(0x80u+i);}for(i=0;i<64;i++)in.signature[i]=(uint8_t)(0x40u+i);in.text=text;in.text_length=(uint32_t)(sizeof(text)-1u);
    CHECK(stn_contract_response_encode(&in,encoded,sizeof(encoded),&n)==STN_CONTRACT_OK);
    CHECK(n==STN_CONTRACT_RESPONSE_HEADER_SIZE+sizeof(text)-1u);
    CHECK(stn_contract_response_decode(encoded,n,&out)==STN_CONTRACT_OK);
    CHECK(out.version==1u&&out.text_length==sizeof(text)-1u);
    CHECK(memcmp(out.contract_id,in.contract_id,32)==0&&memcmp(out.actor,in.actor,32)==0);
    CHECK(memcmp(out.signature,in.signature,64)==0&&memcmp(out.text,text,sizeof(text)-1u)==0);
    CHECK(stn_contract_response_validate_structure(encoded,n)==STN_CONTRACT_OK);
    encoded[0]='X';CHECK(stn_contract_response_validate_structure(encoded,n)==STN_CONTRACT_MAGIC);encoded[0]='S';
    CHECK(stn_contract_response_validate_structure(encoded,n-1u)==STN_CONTRACT_LENGTH);
    in.text_length=0u;CHECK(stn_contract_response_encode(&in,encoded,sizeof(encoded),&n)==STN_CONTRACT_TERMS_LIMIT);
    puts("contract response tests passed");return 0;
}
