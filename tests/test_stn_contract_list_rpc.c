#include "stn_contract_list_rpc.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    static const char valid[] =
        "stn0_0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    char bad[sizeof(valid)];

    assert(strlen(valid) == STN_CONTRACT_LIST_RPC_IDENTITY_SIZE);
    assert(stn_contract_list_rpc_identity_valid((const uint8_t *)valid, strlen(valid)) == 1);
    assert(stn_contract_list_rpc_identity_valid(NULL, strlen(valid)) == 0);
    assert(stn_contract_list_rpc_identity_valid((const uint8_t *)valid, strlen(valid) - 1u) == 0);

    memcpy(bad, valid, sizeof(valid));
    bad[0] = 'x';
    assert(stn_contract_list_rpc_identity_valid((const uint8_t *)bad, strlen(bad)) == 0);

    memcpy(bad, valid, sizeof(valid));
    bad[5] = 'A';
    assert(stn_contract_list_rpc_identity_valid((const uint8_t *)bad, strlen(bad)) == 0);
    return 0;
}
