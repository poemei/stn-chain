/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_pending.h"
#include "stn_contract_transaction.h"
#include "stn_sha256.h"

#include <stdio.h>
#include <string.h>

static void fill(uint8_t *bytes,size_t length,uint8_t start)
{
    size_t i;
    for(i=0u;i<length;++i)bytes[i]=(uint8_t)(start+(uint8_t)i);
}

#ifdef STN_CONTRACT_PENDING_TEST_MAIN
int main(void)
{
    static const uint8_t terms[]={'p','e','n','d','i','n','g'};
    stn_contract_participant participant;
    stn_contract contract;
    stn_contract_transaction action;
    stn_transaction outer;
    stn_pending pending;
    stn_hash_provider hash={stn_sha256,NULL};
    uint8_t canonical[STN_CONTRACT_MAX_SIZE];
    uint8_t action_bytes[STN_CONTRACT_TX_MAX_SIZE];
    uint8_t transaction[STN_TX_MAX_SIZE];
    uint8_t id[32];
    size_t canonical_length=0u,action_length=0u,transaction_length=0u;

    memset(&participant,0,sizeof(participant));
    fill(participant.identity,sizeof(participant.identity),0x21u);
    participant.role=STN_CONTRACT_ROLE_ISSUER;

    memset(&contract,0,sizeof(contract));
    contract.version=STN_CONTRACT_VERSION;
    contract.type=STN_CONTRACT_GENERIC;
    contract.sequence=0u;
    contract.created_at=1u;
    contract.state=STN_CONTRACT_STATE_DRAFT;
    contract.participants=&participant;
    contract.participant_count=1u;
    contract.terms=terms;
    contract.terms_length=(uint32_t)sizeof(terms);
    if(stn_contract_encode(&contract,canonical,sizeof(canonical),&canonical_length)!=STN_CONTRACT_OK)return 1;

    memset(&action,0,sizeof(action));
    action.version=STN_CONTRACT_TX_VERSION;
    action.action=STN_CONTRACT_ACTION_CREATE;
    action.sequence=0u;
    action.canonical_contract=canonical;
    action.canonical_contract_length=(uint32_t)canonical_length;
    memcpy(action.actor,participant.identity,sizeof(action.actor));
    fill(action.signature,sizeof(action.signature),0x61u);
    if(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)!=STN_CONTRACT_OK)return 2;

    outer.version=1u;
    outer.type=STN_TX_CONTRACT_ACTION;
    outer.record_bytes=action_bytes;
    outer.record_length=(uint32_t)action_length;
    if(stn_transaction_encode(&outer,transaction,sizeof(transaction),&transaction_length)!=STN_DATA_OK)return 3;

    stn_pending_init(&pending);
    if(stn_pending_insert(&pending,transaction,transaction_length,&hash,id)!=STN_PENDING_ACCEPTED)return 4;
    if(stn_pending_count(&pending)!=1u)return 5;
    if(memcmp(pending.entries[0].signer,participant.identity,32u)!=0)return 6;
    if(stn_pending_insert(&pending,transaction,transaction_length,&hash,id)!=STN_PENDING_DUPLICATE)return 7;
    stn_pending_clear(&pending);

    puts("STN Contract pending tests passed.");
    return 0;
}
#endif
