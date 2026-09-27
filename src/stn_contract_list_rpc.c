#include "stn_contract_list_rpc.h"
#include <string.h>

int stn_contract_list_rpc_identity_valid(const uint8_t *identity, size_t length)
{
    size_t i;
    if (identity == NULL || length != STN_CONTRACT_LIST_RPC_IDENTITY_SIZE)
        return 0;
    if (memcmp(identity, "stn0_", 5u) != 0)
        return 0;
    for (i = 5u; i < STN_CONTRACT_LIST_RPC_IDENTITY_SIZE; ++i) {
        const uint8_t c = identity[i];
        if (!((c >= (uint8_t)'0' && c <= (uint8_t)'9') ||
              (c >= (uint8_t)'a' && c <= (uint8_t)'f')))
            return 0;
    }
    return 1;
}
