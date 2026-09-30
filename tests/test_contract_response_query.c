/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_response_query.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"response query line %d\n",__LINE__);return 1;}}while(0)
int main(void){
    uint8_t q[74],out[256];size_t n=1;stn_contract_snapshot *s=stn_contract_snapshot_create();
    memcpy(q,"stnc0_",6);memset(q+6,'0',64);memset(q+70,0,4);
    CHECK(stn_contract_response_query_request_valid(q,74));
    CHECK(!stn_contract_response_query_request_valid(q,73));
    CHECK(stn_contract_response_query(NULL,q,74,out,sizeof(out),&n)==STN_RPC_UNAVAILABLE && n==0);
    CHECK(stn_contract_response_query(s,q,74,out,sizeof(out),&n)==STN_RPC_NOT_FOUND);
    memset(out,0,8);CHECK(stn_contract_response_query_reply_valid(out,8));
    out[3]=1;CHECK(!stn_contract_response_query_reply_valid(out,8));
    stn_contract_snapshot_release(s);puts("Contract response query: passed.");return 0;
}
