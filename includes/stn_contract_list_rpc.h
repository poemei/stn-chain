#ifndef STN_CONTRACT_LIST_RPC_H
#define STN_CONTRACT_LIST_RPC_H

#include <stddef.h>
#include <stdint.h>

#define STN_CONTRACT_LIST_RPC_METHOD 12u
#define STN_CONTRACT_LIST_RPC_IDENTITY_SIZE 69u

/* Contract-list RPC support is intentionally a read-only view over accepted
 * Chain history.  The implementation is wired through the normal STNC
 * dispatcher and never treats pending contracts as accepted state. */
int stn_contract_list_rpc_identity_valid(const uint8_t *identity, size_t length);

#endif
