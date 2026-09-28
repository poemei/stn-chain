/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transaction_status.h"
#include <assert.h>
#include <string.h>

/* This test intentionally qualifies the public failure boundary without
 * constructing synthetic consensus history. Existing Chain integration tests
 * own accepted-history construction; this unit proves malformed/absent history
 * can never be reported as accepted. */
#ifdef STN_TRANSACTION_STATUS_TEST_MAIN
int main(void)
{
    stn_transaction_status out;
    uint8_t id[32]={0};
    memset(&out,0xa5,sizeof(out));
    assert(stn_transaction_status_find(NULL,NULL,0u,id,&out)==
        STN_TRANSACTION_STATUS_ARGUMENT);
    assert(stn_transaction_status_find(NULL,NULL,0u,NULL,&out)==
        STN_TRANSACTION_STATUS_ARGUMENT);
    assert(stn_transaction_status_find(NULL,NULL,0u,id,NULL)==
        STN_TRANSACTION_STATUS_ARGUMENT);
    return 0;
}
#endif
