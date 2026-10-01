#ifndef STN_CONTRACT_DETAIL_QUERY_H
#define STN_CONTRACT_DETAIL_QUERY_H
#include "stn_contract_query.h"
int stn_contract_detail_request_valid(const uint8_t *,size_t);
int stn_contract_detail_reply_valid(const uint8_t *,size_t);
stn_rpc_code stn_contract_detail_query(const stn_contract_snapshot *,const uint8_t *,size_t,uint8_t *,size_t,size_t *);
#endif
