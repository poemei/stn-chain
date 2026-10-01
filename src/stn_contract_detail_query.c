#include "stn_contract_detail_query.h"
#include <string.h>
int stn_contract_detail_request_valid(const uint8_t *p,size_t n){stn_address a;return p&&n==70&&stn_address_decode((const char*)p,n,&a)==STN_DATA_OK&&a.type==STN_ADDRESS_CONTRACT;}
int stn_contract_detail_reply_valid(const uint8_t *p,size_t n){stn_contract c;return stn_contract_decode(p,n,&c)==STN_CONTRACT_OK&&c.state==STN_CONTRACT_STATE_DRAFT&&c.sequence==0;}
stn_rpc_code stn_contract_detail_query(const stn_contract_snapshot *s,const uint8_t *p,size_t n,uint8_t *out,size_t cap,size_t *written)
{
 stn_address a;size_t at;const stn_contract_state_store *store;const stn_contract_state_entry *e;
 if(written)*written=0;if(!out||!written)return STN_RPC_PROVIDER;
 if(!stn_contract_detail_request_valid(p,n))return STN_RPC_INVALID;
 if(!s)return STN_RPC_UNAVAILABLE;
 stn_address_decode((const char*)p,n,&a);store=stn_contract_snapshot_const_state(s);
 if(stn_contract_state_find(store,a.identifier,&at)!=STN_CONTRACT_OK)return STN_RPC_NOT_FOUND;
 e=&store->entries[at];if(e->current.state==STN_CONTRACT_STATE_DRAFT)return STN_RPC_NOT_FOUND;
 if(cap<e->canonical_draft_length)return STN_RPC_CAPACITY;
 memcpy(out,e->canonical_draft,e->canonical_draft_length);*written=e->canonical_draft_length;return STN_RPC_OK;
}
