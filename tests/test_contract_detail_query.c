#include "stn_contract_detail_query.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){stn_contract_snapshot *s=stn_contract_snapshot_create();stn_contract c={0};stn_contract_participant p={0};stn_address a;uint8_t draft[128],out[128];char address[71];size_t n,z,at;
 c.version=1;c.type=1;c.state=1;c.participants=&p;c.participant_count=1;p.role=4;p.identity[0]=1;c.terms=(const uint8_t*)"Exact terms";c.terms_length=11;
 assert(stn_contract_encode(&c,draft,sizeof(draft),&n)==STN_CONTRACT_OK);assert(stn_contract_address(draft,n,&a)==STN_CONTRACT_OK);assert(stn_address_encode(&a,address,sizeof(address),&z)==STN_DATA_OK);
 assert(stn_contract_detail_query(s,(uint8_t*)address,70,out,sizeof(out),&z)==STN_RPC_NOT_FOUND);
 assert(stn_contract_snapshot_register(s,draft,n,&at)==STN_CONTRACT_OK);
 assert(stn_contract_detail_query(s,(uint8_t*)address,70,out,sizeof(out),&z)==STN_RPC_NOT_FOUND);
 stn_contract_snapshot_state(s)->entries[at].current.state=2;
 assert(stn_contract_detail_query(s,(uint8_t*)address,70,out,sizeof(out),&z)==STN_RPC_OK&&z==n&&!memcmp(draft,out,n));
 assert(stn_contract_detail_reply_valid(out,z));assert(!stn_contract_detail_reply_valid(out,z-1));
 assert(stn_contract_detail_query(s,(uint8_t*)address,69,out,sizeof(out),&z)==STN_RPC_INVALID);
 assert(stn_contract_detail_query(s,(uint8_t*)address,70,out,1,&z)==STN_RPC_CAPACITY);
 stn_contract_snapshot_release(s);puts("Contract detail query passed");return 0;}
