/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_transaction.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

#define CHECK(e) do { ++checks; if (!(e)) { ++failures; fprintf(stderr,"contract transaction line %d\n",__LINE__); } } while (0)

static void fill(uint8_t *bytes,size_t length,uint8_t start)
{ size_t i; for(i=0;i<length;++i)bytes[i]=(uint8_t)(start+(uint8_t)i); }

int test_contract_transaction(void)
{
    static const uint8_t terms[]={'t','e','s','t'};
    stn_contract_participant participant;
    stn_contract contract;
    stn_contract_transaction tx,decoded;
    uint8_t canonical[STN_CONTRACT_MAX_SIZE];
    uint8_t action[STN_AUTHORITY_ACTION_SIZE];
    uint8_t context[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t authority[STN_AUTHORITY_EVIDENCE_SIZE];
    uint8_t encoded[STN_CONTRACT_TX_MAX_SIZE];
    size_t canonical_length=0u,authority_length=0u,written=0u;

    checks=0u;failures=0u;
    memset(&participant,0,sizeof(participant));
    fill(participant.identity,sizeof(participant.identity),0x10u);
    participant.role=STN_CONTRACT_ROLE_ISSUER;

    memset(&contract,0,sizeof(contract));
    contract.version=STN_CONTRACT_VERSION;
    contract.type=STN_CONTRACT_GENERIC;
    contract.sequence=7u;
    contract.created_at=9u;
    contract.state=STN_CONTRACT_STATE_REVIEW;
    contract.participants=&participant;
    contract.participant_count=1u;
    contract.terms=terms;
    contract.terms_length=(uint32_t)sizeof(terms);
    CHECK(stn_contract_encode(&contract,canonical,sizeof(canonical),&canonical_length)==STN_CONTRACT_OK);

    CHECK(stn_contract_authority_action(STN_CONTRACT_ACTION_APPROVE,action)==STN_CONTRACT_OK);
    CHECK(stn_contract_authority_context(canonical,canonical_length,context)==STN_CONTRACT_OK);
    CHECK(stn_authority_evidence_encode(participant.identity,action,context,authority,sizeof(authority),&authority_length)==STN_AUTHORITY_AUTHORIZED);

    memset(&tx,0,sizeof(tx));
    tx.version=STN_CONTRACT_TX_VERSION;
    tx.action=STN_CONTRACT_ACTION_APPROVE;
    tx.sequence=8u;
    tx.canonical_contract=canonical;
    tx.canonical_contract_length=(uint32_t)canonical_length;
    memcpy(tx.actor,participant.identity,STN_IDENTITY_PUBLIC_KEY_SIZE);
    fill(tx.signature,sizeof(tx.signature),0x80u);
    tx.authority_evidence=authority;
    tx.authority_evidence_length=(uint32_t)authority_length;
    CHECK(stn_contract_transaction_encode(&tx,encoded,sizeof(encoded),&written)==STN_CONTRACT_OK);
    CHECK(stn_contract_transaction_decode(encoded,written,&decoded)==STN_CONTRACT_OK);
    CHECK(decoded.action==STN_CONTRACT_ACTION_APPROVE);
    CHECK(decoded.authority_evidence_length==STN_AUTHORITY_EVIDENCE_SIZE);

    /* CREATE bootstraps a new Contract: no pre-existing Contract grant exists. */
    contract.sequence=0u;
    contract.state=STN_CONTRACT_STATE_DRAFT;
    CHECK(stn_contract_encode(&contract,canonical,sizeof(canonical),&canonical_length)==STN_CONTRACT_OK);
    memset(&tx,0,sizeof(tx));
    tx.version=STN_CONTRACT_TX_VERSION;
    tx.action=STN_CONTRACT_ACTION_CREATE;
    tx.sequence=0u;
    tx.canonical_contract=canonical;
    tx.canonical_contract_length=(uint32_t)canonical_length;
    memcpy(tx.actor,participant.identity,STN_IDENTITY_PUBLIC_KEY_SIZE);
    fill(tx.signature,sizeof(tx.signature),0x40u);
    tx.authority_evidence=NULL;
    tx.authority_evidence_length=0u;
    written=0u;
    CHECK(stn_contract_transaction_encode(&tx,encoded,sizeof(encoded),&written)==STN_CONTRACT_OK);
    CHECK(written==STN_CONTRACT_TX_HEADER_SIZE+canonical_length);
    CHECK(stn_contract_transaction_decode(encoded,written,&decoded)==STN_CONTRACT_OK);
    CHECK(decoded.action==STN_CONTRACT_ACTION_CREATE);
    CHECK(decoded.sequence==0u);
    CHECK(decoded.authority_evidence==NULL);
    CHECK(decoded.authority_evidence_length==0u);
    CHECK(stn_contract_transaction_validate_structure(encoded,written)==STN_CONTRACT_OK);

    /* CREATE rejects smuggled grant bytes. */
    tx.authority_evidence=authority;
    tx.authority_evidence_length=STN_AUTHORITY_EVIDENCE_SIZE;
    CHECK(stn_contract_transaction_encode(&tx,encoded,sizeof(encoded),&written)==STN_CONTRACT_AUTHORITY_ERROR);

    /* Non-CREATE actions still require a complete authority grant. */
    tx.action=STN_CONTRACT_ACTION_AMEND;
    tx.authority_evidence=NULL;
    tx.authority_evidence_length=0u;
    CHECK(stn_contract_transaction_encode(&tx,encoded,sizeof(encoded),&written)==STN_CONTRACT_AUTHORITY_ERROR);

    /* Malformed CREATE authority length is rejected on decode. */
    tx.action=STN_CONTRACT_ACTION_CREATE;
    tx.authority_evidence=NULL;
    tx.authority_evidence_length=0u;
    CHECK(stn_contract_transaction_encode(&tx,encoded,sizeof(encoded),&written)==STN_CONTRACT_OK);
    encoded[19]=1u;
    CHECK(stn_contract_transaction_decode(encoded,written,&decoded)==STN_CONTRACT_AUTHORITY_ERROR);

    printf("Contract transactions: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}

#ifdef STN_CONTRACT_TRANSACTION_TEST_MAIN
int main(void){return test_contract_transaction();}
#endif
