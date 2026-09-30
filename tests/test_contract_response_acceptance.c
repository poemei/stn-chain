/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_response_acceptance.h"
#include <stdio.h>

/* Integration coverage for signed fixtures belongs with the Chain candidate
 * test because acceptance requires a real participant identity/signature.
 * This unit test locks the fail-closed API boundaries independently. */
#define CHECK(x) do{if(!(x)){fprintf(stderr,"contract response acceptance test failed: %d\n",__LINE__);return 1;}}while(0)

int main(void)
{
    stn_contract_snapshot *snapshot=stn_contract_snapshot_create();
    stn_transaction tx={0};
    CHECK(snapshot!=NULL);
    CHECK(stn_contract_response_accept(NULL,&tx)==STN_DATA_ARGUMENT);
    CHECK(stn_contract_response_accept(snapshot,NULL)==STN_DATA_ARGUMENT);
    tx.type=STN_TX_TRANSFER;
    CHECK(stn_contract_response_accept(snapshot,&tx)==STN_DATA_TYPE);
    tx.type=STN_TX_CONTRACT_RESPONSE;
    tx.record_bytes=(const uint8_t *)"x";
    tx.record_length=1u;
    CHECK(stn_contract_response_accept(snapshot,&tx)==STN_DATA_CONTENT);
    stn_contract_snapshot_release(snapshot);
    puts("contract response acceptance tests passed");
    return 0;
}
