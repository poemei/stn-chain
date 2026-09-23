/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_snapshot.h"
#include "stn_chain.h"
#include "stn_contract_transaction.h"
#include "stn_sha256.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)


static const uint8_t chain_root[32]={
    0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,
    0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a
};
static const uint8_t create_grant_signature[64]={0xed,0xd0,0xdd,0x5a,0x58,0x47,0x38,0xaa,0xd7,0xf5,0x9f,0x1c,0x8f,0x90,0x08,0x2b,0x0d,0x22,0xb1,0x5c,0x4b,0x77,0x6f,0x8a,0xa7,0x4a,0x95,0x4f,0xa4,0x3a,0x4c,0x4c,0xb5,0xf2,0x16,0x31,0xad,0x7a,0x15,0xe9,0xad,0xa4,0x92,0x63,0x58,0x40,0x4b,0x06,0xd5,0x53,0x5c,0xc5,0x2b,0xe0,0xbb,0xd7,0x10,0xd2,0x04,0x05,0x73,0x4b,0x3a,0x05};
static const uint8_t create_action_signature[64]={0x6e,0xea,0x0b,0x72,0xd3,0xe2,0x7b,0xf8,0xa8,0x07,0xe6,0xdd,0xfc,0xf5,0xbf,0x3e,0x31,0x47,0xb7,0x43,0x68,0x40,0x5b,0xab,0xe1,0x74,0x39,0x63,0x42,0x0f,0x34,0x85,0xd1,0x0f,0x3b,0x48,0xa7,0x08,0x1a,0x2c,0xe9,0x67,0xf4,0xdc,0x7a,0x83,0x83,0x35,0x67,0x23,0xf7,0xa1,0x42,0x17,0x3f,0x1a,0xb7,0xa5,0xf7,0xe8,0x78,0x9c,0xa3,0x07};

static const uint8_t amend_grant_signature[64]={0xe9,0x60,0xac,0x55,0x3d,0x12,0xc7,0xfe,0xb7,0xff,0x27,0x2b,0x67,0x80,0x43,0x44,0x84,0x49,0xda,0x78,0x34,0xc0,0xa7,0x81,0xc9,0x6a,0x85,0x2a,0x27,0x19,0xbd,0xe3,0xc3,0xac,0x53,0xf5,0xf8,0xe2,0xe5,0xe3,0x0c,0x79,0x7e,0x07,0x87,0x98,0x6c,0xe5,0xc7,0x01,0x33,0x46,0x17,0x35,0x7e,0x90,0x9d,0xbc,0x51,0x78,0x85,0xd5,0xf9,0x05};
static const uint8_t amend_action_signature[64]={0x25,0x4d,0x1e,0xb4,0x07,0x47,0x79,0x88,0xe0,0x40,0x62,0x89,0x23,0x11,0x40,0xa3,0x01,0x8a,0xc3,0x9d,0x7e,0x8f,0xb8,0xa3,0xe8,0xc5,0x40,0x83,0xb6,0xff,0xd6,0xb8,0xae,0x81,0x5d,0xbc,0x22,0x2d,0x6b,0xfd,0x46,0x85,0x05,0x51,0x93,0x3f,0x2b,0x7a,0xcc,0x73,0xec,0x4a,0x22,0xa0,0xa9,0xab,0xb7,0xf0,0xd0,0xca,0x40,0x10,0x68,0x02};

static const uint8_t approver_two[32]={0x3d,0x40,0x17,0xc3,0xe8,0x43,0x89,0x5a,0x92,0xb7,0x0a,0xa7,0x4d,0x1b,0x7e,0xbc,0x9c,0x98,0x2c,0xcf,0x2e,0xc4,0x96,0x8c,0xc0,0xcd,0x55,0xf1,0x2a,0xf4,0x66,0x0c};
static const uint8_t approver_three[32]={0xfc,0x51,0xcd,0x8e,0x62,0x18,0xa1,0xa3,0x8d,0xa4,0x7e,0xd0,0x02,0x30,0xf0,0x58,0x08,0x16,0xed,0x13,0xba,0x33,0x03,0xac,0x5d,0xeb,0x91,0x15,0x48,0x90,0x80,0x25};
static const uint8_t approve_one_grant_signature[64]={0x98,0x7c,0xc0,0x9e,0x0a,0x94,0x12,0x7d,0xc1,0xb3,0x03,0xac,0xb2,0xdb,0x53,0x6d,0xe8,0xe4,0xeb,0xa2,0x69,0xdc,0xf0,0x0f,0xd0,0x6f,0x8b,0xef,0x11,0x1a,0xeb,0xfd,0x34,0xe4,0xae,0x5a,0xb2,0xf4,0x45,0x5b,0x84,0x36,0x3e,0x71,0xa8,0xd0,0xf8,0x40,0x3c,0x27,0x28,0xb6,0x94,0x0c,0x7b,0x07,0x55,0x02,0x4e,0x7f,0xef,0x2a,0x2a,0x00};
static const uint8_t approve_one_action_signature[64]={0x0c,0x5e,0x0e,0xa0,0x2e,0xc1,0xa5,0x15,0xb1,0xd6,0x03,0x64,0xa3,0x02,0xb4,0x0b,0x17,0x6d,0x6d,0x6e,0xc8,0x2c,0xd4,0x6a,0xcc,0x5e,0x44,0x3f,0xa2,0x03,0x79,0xca,0xaa,0xa2,0x15,0xaa,0x5b,0xc5,0xef,0xb4,0x93,0x19,0x95,0x35,0xca,0x31,0x42,0x22,0x8c,0x2d,0x53,0x4b,0x50,0xff,0xb4,0xf8,0xd2,0x17,0x30,0x88,0x6c,0x5a,0x51,0x07};
static const uint8_t approve_two_grant_signature[64]={0xdd,0xea,0x7f,0xc2,0x1e,0x8a,0x6f,0xcc,0x3a,0x87,0x5e,0xa1,0x8f,0x43,0x0a,0xb4,0x3c,0x41,0x83,0x88,0x71,0x52,0xc0,0xe1,0x08,0xd8,0xbe,0x11,0xc7,0xc8,0xb1,0x20,0x04,0x68,0x91,0xc1,0x0c,0xd2,0x51,0x91,0x05,0x1a,0xaf,0x0c,0xdb,0x7e,0xb3,0x5f,0x29,0xe7,0x4f,0x12,0x70,0xad,0x51,0x52,0x5f,0x03,0x09,0x2d,0x19,0x92,0xb9,0x0f};
static const uint8_t approve_two_action_signature[64]={0x0d,0x6d,0x18,0x9f,0xe6,0xa0,0x1d,0x22,0xf2,0xb9,0x17,0xa9,0xea,0xf9,0xca,0x99,0x13,0x7c,0x8e,0x34,0x0e,0x37,0x29,0x8c,0x3f,0x7d,0x41,0xdb,0xdc,0x07,0x59,0x82,0x6a,0x56,0x7c,0xeb,0xc7,0x00,0xd0,0x3c,0x1b,0xe5,0x98,0x3d,0x82,0xe3,0xa7,0x77,0x37,0x0b,0x84,0xe5,0xa7,0x8e,0x4b,0x3a,0xaf,0x37,0x80,0xd0,0x78,0x5f,0x76,0x06};

static const uint8_t execute_grant_signature[64]={0x94,0xce,0x05,0x51,0xa1,0x06,0x48,0xc6,0xae,0xaf,0x99,0x6b,0x48,0x03,0x5a,0xa2,0xef,0x3c,0x29,0x67,0xbd,0x4c,0x21,0x92,0x97,0xf3,0xfd,0xee,0x72,0x16,0x92,0x0f,0x96,0x91,0xf9,0xa7,0xa6,0x7a,0x02,0xfd,0x56,0x80,0x89,0x85,0xe0,0xd5,0x78,0xbf,0x9b,0xfc,0x34,0x0b,0x40,0x30,0xdc,0x0f,0xa6,0x05,0x75,0x1e,0x25,0xcb,0xfe,0x0d};
static const uint8_t execute_action_signature[64]={0x9c,0x42,0xc4,0xf1,0xc7,0xef,0x07,0x6a,0x50,0xb0,0x0a,0xf6,0xd1,0xfb,0xe4,0xe7,0x18,0xc1,0x82,0x18,0x61,0xa3,0x02,0x64,0x1c,0xcb,0x1a,0xda,0xfe,0x20,0x0e,0x53,0x99,0xf6,0x15,0xbf,0x2c,0xda,0x11,0x59,0xf2,0xd7,0x2c,0x54,0x73,0x5d,0x7c,0xed,0xe7,0x0e,0x69,0x97,0x02,0xe5,0x7a,0x22,0xef,0x91,0x6c,0x58,0x31,0x26,0x2d,0x01};

static void contract_chain_create(void)
{
    static const uint8_t terms[]={'T'};
    stn_contract_participant participants[3]={{0}};
    stn_contract draft={0};
    stn_contract_transaction action={0};
    stn_transaction grant_tx={0},contract_tx={0};
    stn_transaction_span span;
    stn_block block={0};
    stn_chain_context context={0};
    stn_chain_state empty={0},genesis_state={0},accepted={0},rejected={0};
    stn_chain_report report;
    stn_hash_provider provider={stn_sha256,NULL};
    stn_address address;
    stn_contract_state_store *state;
    uint8_t draft_bytes[STN_CONTRACT_MAX_SIZE];
    uint8_t authority_action[STN_AUTHORITY_ACTION_SIZE];
    uint8_t authority_context[STN_AUTHORITY_CONTEXT_SIZE];
    uint8_t evidence[STN_AUTHORITY_EVIDENCE_SIZE];
    uint8_t grant[STN_AUTHORITY_GRANT_SIZE];
    uint8_t action_bytes[STN_CONTRACT_TX_MAX_SIZE];
    uint8_t tx_bytes[STN_TX_MAX_SIZE];
    uint8_t body[STN_BLOCK_MAX_BODY];
    uint8_t genesis_bytes[STN_BLOCK_MAX_SIZE];
    uint8_t child_bytes[STN_BLOCK_MAX_SIZE];
    size_t draft_length=0,evidence_length=0,grant_length=0,action_length=0;
    size_t tx_length=0,body_length=0,genesis_length=0,child_length=0;

    memcpy(participants[0].identity,chain_root,sizeof(chain_root));
    participants[0].role=STN_CONTRACT_ROLE_APPROVER;
    memcpy(participants[1].identity,approver_two,sizeof(approver_two));
    participants[1].role=STN_CONTRACT_ROLE_APPROVER;
    memcpy(participants[2].identity,approver_three,sizeof(approver_three));
    participants[2].role=STN_CONTRACT_ROLE_APPROVER;
    draft.version=STN_CONTRACT_VERSION;draft.type=STN_CONTRACT_GENERIC;
    draft.sequence=0u;draft.created_at=1u;draft.state=STN_CONTRACT_STATE_DRAFT;
    draft.participants=participants;draft.participant_count=3u;
    draft.terms=terms;draft.terms_length=(uint32_t)sizeof(terms);
    CHECK(stn_contract_encode(&draft,draft_bytes,sizeof(draft_bytes),&draft_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_address(draft_bytes,draft_length,&address)==STN_CONTRACT_OK);
    CHECK(stn_contract_authority_action(STN_CONTRACT_ACTION_CREATE,authority_action)==STN_CONTRACT_OK);
    CHECK(stn_contract_authority_context(draft_bytes,draft_length,authority_context)==STN_CONTRACT_OK);
    CHECK(stn_authority_evidence_encode(chain_root,authority_action,authority_context,
        evidence,sizeof(evidence),&evidence_length)==STN_AUTHORITY_AUTHORIZED);
    CHECK(stn_authority_grant_encode(chain_root,evidence,create_grant_signature,
        grant,sizeof(grant),&grant_length)==STN_AUTHORITY_VALID_GRANT);

    grant_tx.version=1u;grant_tx.type=STN_TX_AUTHORITY_GRANT;
    grant_tx.record_bytes=grant;grant_tx.record_length=(uint32_t)grant_length;
    CHECK(stn_transaction_encode(&grant_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
    span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
    CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
    block.header.version=1u;block.header.network_id[0]=1u;
    block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
    CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
        block.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&block,genesis_bytes,sizeof(genesis_bytes),&genesis_length)==STN_DATA_OK);

    context.network_id[0]=1u;context.genesis_bytes=genesis_bytes;context.genesis_length=genesis_length;
    context.genesis_authority_roots=chain_root;context.genesis_authority_root_count=1u;
    context.genesis_initial_identities=chain_root;context.genesis_initial_identity_count=1u;
    context.hash_provider=provider;
    CHECK(stn_chain_initialize(&context,&empty)==STN_DATA_OK);
    report=stn_chain_validate_candidate(&context,&empty,genesis_bytes,genesis_length,&genesis_state);
    CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(genesis_state.lifecycle!=NULL && genesis_state.lifecycle->grant_count==1u);
    CHECK(stn_contract_snapshot_const_state(genesis_state.contracts)->entry_count==0u);

    action.version=STN_CONTRACT_TX_VERSION;action.action=STN_CONTRACT_ACTION_CREATE;
    action.sequence=1u;action.canonical_contract=draft_bytes;
    action.canonical_contract_length=(uint32_t)draft_length;
    memcpy(action.actor,chain_root,sizeof(chain_root));
    memcpy(action.signature,create_action_signature,sizeof(create_action_signature));
    action.authority_evidence=evidence;action.authority_evidence_length=(uint32_t)evidence_length;
    CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)==STN_CONTRACT_OK);
    contract_tx.version=1u;contract_tx.type=STN_TX_CONTRACT_ACTION;
    contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
    CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
    span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
    CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
    memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
    memcpy(block.header.previous_hash,genesis_state.tip_id,32);block.header.height=1u;block.header.timestamp=1u;
    block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
    CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
        block.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);

    report=stn_chain_validate_candidate(&context,&genesis_state,child_bytes,child_length,&accepted);
    CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    state=stn_contract_snapshot_state(accepted.contracts);
    CHECK(state!=NULL && state->entry_count==1u);
    CHECK(state->entries[0].current.state==STN_CONTRACT_STATE_ISSUED);
    CHECK(state->entries[0].current.sequence==1u);
    CHECK(memcmp(state->entries[0].contract_id,address.identifier,STN_ADDRESS_ID_SIZE)==0);
    CHECK(stn_contract_snapshot_const_state(genesis_state.contracts)->entry_count==0u);

    action.signature[0]^=1u;
    CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)==STN_CONTRACT_OK);
    contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
    CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
    span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
    CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
    block.header.body_length=(uint32_t)body_length;block.body=body;
    CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
        block.header.transaction_commitment)==STN_DATA_OK);
    CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
    report=stn_chain_validate_candidate(&context,&genesis_state,child_bytes,child_length,&rejected);
    CHECK(report.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(stn_contract_snapshot_const_state(genesis_state.contracts)->entry_count==0u);


    {
        stn_contract issued=state->entries[0].current;
        stn_chain_state grant_state={0},review_state={0};
        uint8_t issued_bytes[STN_CONTRACT_MAX_SIZE],amend_evidence[STN_AUTHORITY_EVIDENCE_SIZE];
        uint8_t amend_grant[STN_AUTHORITY_GRANT_SIZE],amend_grant_tx[STN_TX_MAX_SIZE];
        size_t issued_length=0,amend_evidence_length=0,amend_grant_length=0,amend_grant_tx_length=0;

        issued.participants=participants;issued.participant_bytes=NULL;
        CHECK(stn_contract_encode(&issued,issued_bytes,sizeof(issued_bytes),&issued_length)==STN_CONTRACT_OK);
        CHECK(stn_contract_authority_action(STN_CONTRACT_ACTION_AMEND,authority_action)==STN_CONTRACT_OK);
        CHECK(stn_contract_authority_context(draft_bytes,draft_length,authority_context)==STN_CONTRACT_OK);
        CHECK(stn_authority_evidence_encode(chain_root,authority_action,authority_context,
            amend_evidence,sizeof(amend_evidence),&amend_evidence_length)==STN_AUTHORITY_AUTHORIZED);
        CHECK(stn_authority_grant_encode(chain_root,amend_evidence,amend_grant_signature,
            amend_grant,sizeof(amend_grant),&amend_grant_length)==STN_AUTHORITY_VALID_GRANT);
        grant_tx.record_bytes=amend_grant;grant_tx.record_length=(uint32_t)amend_grant_length;
        CHECK(stn_transaction_encode(&grant_tx,amend_grant_tx,sizeof(amend_grant_tx),
            &amend_grant_tx_length)==STN_DATA_OK);
        span.bytes=amend_grant_tx;span.length=(uint32_t)amend_grant_tx_length;
        CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
        memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
        memcpy(block.header.previous_hash,accepted.tip_id,32);block.header.height=2u;block.header.timestamp=2u;
        block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
        CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
            block.header.transaction_commitment)==STN_DATA_OK);
        CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
        report=stn_chain_validate_candidate(&context,&accepted,child_bytes,child_length,&grant_state);
        CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        CHECK(grant_state.lifecycle!=NULL && grant_state.lifecycle->grant_count==2u);

        action.action=STN_CONTRACT_ACTION_AMEND;action.sequence=2u;
        action.canonical_contract=issued_bytes;action.canonical_contract_length=(uint32_t)issued_length;
        memcpy(action.signature,amend_action_signature,sizeof(amend_action_signature));
        action.authority_evidence=amend_evidence;action.authority_evidence_length=(uint32_t)amend_evidence_length;
        CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)==STN_CONTRACT_OK);
        contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
        CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
        span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
        CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
        memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
        memcpy(block.header.previous_hash,grant_state.tip_id,32);block.header.height=3u;block.header.timestamp=3u;
        block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
        CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
            block.header.transaction_commitment)==STN_DATA_OK);
        CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
        report=stn_chain_validate_candidate(&context,&grant_state,child_bytes,child_length,&review_state);
        CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
        state=stn_contract_snapshot_state(review_state.contracts);
        CHECK(state!=NULL && state->entry_count==1u);
        CHECK(state->entries[0].current.state==STN_CONTRACT_STATE_REVIEW);
        CHECK(state->entries[0].current.sequence==2u);
        CHECK(memcmp(state->entries[0].contract_id,address.identifier,STN_ADDRESS_ID_SIZE)==0);
        CHECK(stn_contract_snapshot_const_state(grant_state.contracts)->entries[0].current.state==
            STN_CONTRACT_STATE_ISSUED);
        CHECK(stn_contract_snapshot_const_state(grant_state.contracts)->entries[0].current.sequence==1u);


        {
            stn_contract review=state->entries[0].current,approvals;
            stn_chain_state approve_grant_one={0},approval_one={0},approve_grant_two={0},approval_two={0},duplicate={0};
            uint8_t review_bytes[STN_CONTRACT_MAX_SIZE],approvals_bytes[STN_CONTRACT_MAX_SIZE];
            uint8_t approve_evidence_one[STN_AUTHORITY_EVIDENCE_SIZE],approve_evidence_two[STN_AUTHORITY_EVIDENCE_SIZE];
            uint8_t approve_grant_one_bytes[STN_AUTHORITY_GRANT_SIZE],approve_grant_two_bytes[STN_AUTHORITY_GRANT_SIZE];
            size_t review_length=0,approvals_length=0,approve_evidence_one_length=0,approve_evidence_two_length=0;
            size_t approve_grant_one_length=0,approve_grant_two_length=0;

            review.participants=participants;review.participant_bytes=NULL;
            CHECK(stn_contract_encode(&review,review_bytes,sizeof(review_bytes),&review_length)==STN_CONTRACT_OK);
            CHECK(stn_contract_authority_action(STN_CONTRACT_ACTION_APPROVE,authority_action)==STN_CONTRACT_OK);
            CHECK(stn_contract_authority_context(draft_bytes,draft_length,authority_context)==STN_CONTRACT_OK);
            CHECK(stn_authority_evidence_encode(chain_root,authority_action,authority_context,
                approve_evidence_one,sizeof(approve_evidence_one),&approve_evidence_one_length)==STN_AUTHORITY_AUTHORIZED);
            CHECK(stn_authority_grant_encode(chain_root,approve_evidence_one,approve_one_grant_signature,
                approve_grant_one_bytes,sizeof(approve_grant_one_bytes),&approve_grant_one_length)==STN_AUTHORITY_VALID_GRANT);
            grant_tx.record_bytes=approve_grant_one_bytes;grant_tx.record_length=(uint32_t)approve_grant_one_length;
            CHECK(stn_transaction_encode(&grant_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
            span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
            CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
            memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
            memcpy(block.header.previous_hash,review_state.tip_id,32);block.header.height=4u;block.header.timestamp=4u;
            block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
            CHECK(stn_block_body_commitment(body,body_length,1u,&provider,block.header.transaction_commitment)==STN_DATA_OK);
            CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
            report=stn_chain_validate_candidate(&context,&review_state,child_bytes,child_length,&approve_grant_one);
            CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);

            action.action=STN_CONTRACT_ACTION_APPROVE;action.sequence=3u;
            action.canonical_contract=review_bytes;action.canonical_contract_length=(uint32_t)review_length;
            memcpy(action.actor,chain_root,sizeof(chain_root));
            memcpy(action.signature,approve_one_action_signature,sizeof(approve_one_action_signature));
            action.authority_evidence=approve_evidence_one;action.authority_evidence_length=(uint32_t)approve_evidence_one_length;
            CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)==STN_CONTRACT_OK);
            contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
            CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
            span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
            CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
            memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
            memcpy(block.header.previous_hash,approve_grant_one.tip_id,32);block.header.height=5u;block.header.timestamp=5u;
            block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
            CHECK(stn_block_body_commitment(body,body_length,1u,&provider,block.header.transaction_commitment)==STN_DATA_OK);
            CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
            report=stn_chain_validate_candidate(&context,&approve_grant_one,child_bytes,child_length,&approval_one);
            CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            state=stn_contract_snapshot_state(approval_one.contracts);
            CHECK(state->entries[0].eligible_count==3u && state->entries[0].required_count==2u);
            CHECK(state->entries[0].vote_count==1u);
            CHECK(state->entries[0].current.state==STN_CONTRACT_STATE_APPROVALS);
            CHECK(state->entries[0].current.sequence==3u);

            approvals=state->entries[0].current;approvals.participants=participants;approvals.participant_bytes=NULL;
            CHECK(stn_contract_encode(&approvals,approvals_bytes,sizeof(approvals_bytes),&approvals_length)==STN_CONTRACT_OK);
            CHECK(stn_authority_evidence_encode(approver_two,authority_action,authority_context,
                approve_evidence_two,sizeof(approve_evidence_two),&approve_evidence_two_length)==STN_AUTHORITY_AUTHORIZED);
            CHECK(stn_authority_grant_encode(chain_root,approve_evidence_two,approve_two_grant_signature,
                approve_grant_two_bytes,sizeof(approve_grant_two_bytes),&approve_grant_two_length)==STN_AUTHORITY_VALID_GRANT);
            grant_tx.record_bytes=approve_grant_two_bytes;grant_tx.record_length=(uint32_t)approve_grant_two_length;
            CHECK(stn_transaction_encode(&grant_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
            span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
            CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
            memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
            memcpy(block.header.previous_hash,approval_one.tip_id,32);block.header.height=6u;block.header.timestamp=6u;
            block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
            CHECK(stn_block_body_commitment(body,body_length,1u,&provider,block.header.transaction_commitment)==STN_DATA_OK);
            CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
            report=stn_chain_validate_candidate(&context,&approval_one,child_bytes,child_length,&approve_grant_two);
            CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);

            action.sequence=4u;action.canonical_contract=approvals_bytes;
            action.canonical_contract_length=(uint32_t)approvals_length;
            memcpy(action.actor,approver_two,sizeof(approver_two));
            memcpy(action.signature,approve_two_action_signature,sizeof(approve_two_action_signature));
            action.authority_evidence=approve_evidence_two;action.authority_evidence_length=(uint32_t)approve_evidence_two_length;
            CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)==STN_CONTRACT_OK);
            contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
            CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
            span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
            CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
            memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
            memcpy(block.header.previous_hash,approve_grant_two.tip_id,32);block.header.height=7u;block.header.timestamp=7u;
            block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
            CHECK(stn_block_body_commitment(body,body_length,1u,&provider,block.header.transaction_commitment)==STN_DATA_OK);
            CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
            report=stn_chain_validate_candidate(&context,&approve_grant_two,child_bytes,child_length,&approval_two);
            CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
            state=stn_contract_snapshot_state(approval_two.contracts);
            CHECK(state->entries[0].vote_count==2u);
            CHECK(state->entries[0].current.state==STN_CONTRACT_STATE_ATTESTATION);
            CHECK(state->entries[0].current.sequence==4u);
            CHECK(memcmp(state->entries[0].contract_id,address.identifier,STN_ADDRESS_ID_SIZE)==0);
            CHECK(stn_contract_snapshot_const_state(approval_one.contracts)->entries[0].current.state==
                STN_CONTRACT_STATE_APPROVALS);

            action.sequence=4u;action.canonical_contract=approvals_bytes;
            action.canonical_contract_length=(uint32_t)approvals_length;
            memcpy(action.actor,chain_root,sizeof(chain_root));
            memcpy(action.signature,approve_one_action_signature,sizeof(approve_one_action_signature));
            action.authority_evidence=approve_evidence_one;
            action.authority_evidence_length=(uint32_t)approve_evidence_one_length;
            CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),&action_length)==STN_CONTRACT_OK);
            contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
            CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
            span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
            CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
            memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
            memcpy(block.header.previous_hash,approve_grant_two.tip_id,32);block.header.height=7u;block.header.timestamp=7u;
            block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
            CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
                block.header.transaction_commitment)==STN_DATA_OK);
            CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
            report=stn_chain_validate_candidate(&context,&approve_grant_two,child_bytes,child_length,&duplicate);
            CHECK(report.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT);
            CHECK(stn_contract_snapshot_const_state(approve_grant_two.contracts)->entries[0].vote_count==1u);


            {
                stn_contract attestation=state->entries[0].current;
                stn_chain_state execute_grant_state={0},executed_state={0};
                stn_chain_state_release(&duplicate);
                uint8_t attestation_bytes[STN_CONTRACT_MAX_SIZE];
                uint8_t execute_evidence[STN_AUTHORITY_EVIDENCE_SIZE];
                uint8_t execute_grant[STN_AUTHORITY_GRANT_SIZE];
                size_t attestation_length=0,execute_evidence_length=0,execute_grant_length=0;

                attestation.participants=participants;attestation.participant_bytes=NULL;
                CHECK(stn_contract_encode(&attestation,attestation_bytes,sizeof(attestation_bytes),
                    &attestation_length)==STN_CONTRACT_OK);
                CHECK(stn_contract_authority_action(STN_CONTRACT_ACTION_EXECUTE,authority_action)==STN_CONTRACT_OK);
                CHECK(stn_contract_authority_context(draft_bytes,draft_length,authority_context)==STN_CONTRACT_OK);
                CHECK(stn_authority_evidence_encode(chain_root,authority_action,authority_context,
                    execute_evidence,sizeof(execute_evidence),&execute_evidence_length)==STN_AUTHORITY_AUTHORIZED);
                CHECK(stn_authority_grant_encode(chain_root,execute_evidence,execute_grant_signature,
                    execute_grant,sizeof(execute_grant),&execute_grant_length)==STN_AUTHORITY_VALID_GRANT);
                grant_tx.record_bytes=execute_grant;grant_tx.record_length=(uint32_t)execute_grant_length;
                CHECK(stn_transaction_encode(&grant_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
                span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
                CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
                memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
                memcpy(block.header.previous_hash,approval_two.tip_id,32);block.header.height=8u;block.header.timestamp=8u;
                block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
                CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
                    block.header.transaction_commitment)==STN_DATA_OK);
                CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
                report=stn_chain_validate_candidate(&context,&approval_two,child_bytes,child_length,&execute_grant_state);
                CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);

                action.action=STN_CONTRACT_ACTION_EXECUTE;action.sequence=5u;
                action.canonical_contract=attestation_bytes;
                action.canonical_contract_length=(uint32_t)attestation_length;
                memcpy(action.actor,chain_root,sizeof(chain_root));
                memcpy(action.signature,execute_action_signature,sizeof(execute_action_signature));
                action.authority_evidence=execute_evidence;
                action.authority_evidence_length=(uint32_t)execute_evidence_length;
                CHECK(stn_contract_transaction_encode(&action,action_bytes,sizeof(action_bytes),
                    &action_length)==STN_CONTRACT_OK);
                contract_tx.record_bytes=action_bytes;contract_tx.record_length=(uint32_t)action_length;
                CHECK(stn_transaction_encode(&contract_tx,tx_bytes,sizeof(tx_bytes),&tx_length)==STN_DATA_OK);
                span.bytes=tx_bytes;span.length=(uint32_t)tx_length;
                CHECK(stn_block_body_encode(&span,1u,body,sizeof(body),&body_length)==STN_DATA_OK);
                memset(&block,0,sizeof(block));block.header.version=1u;block.header.network_id[0]=1u;
                memcpy(block.header.previous_hash,execute_grant_state.tip_id,32);block.header.height=9u;block.header.timestamp=9u;
                block.header.transaction_count=1u;block.header.body_length=(uint32_t)body_length;block.body=body;
                CHECK(stn_block_body_commitment(body,body_length,1u,&provider,
                    block.header.transaction_commitment)==STN_DATA_OK);
                CHECK(stn_block_encode(&block,child_bytes,sizeof(child_bytes),&child_length)==STN_DATA_OK);
                report=stn_chain_validate_candidate(&context,&execute_grant_state,child_bytes,child_length,&executed_state);
                CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
                state=stn_contract_snapshot_state(executed_state.contracts);
                CHECK(state->entries[0].current.state==STN_CONTRACT_STATE_EXECUTED);
                CHECK(state->entries[0].current.sequence==5u);
                CHECK(state->entries[0].vote_count==2u);
                CHECK(memcmp(state->entries[0].contract_id,address.identifier,STN_ADDRESS_ID_SIZE)==0);
                CHECK(stn_contract_snapshot_const_state(approval_two.contracts)->entries[0].current.state==
                    STN_CONTRACT_STATE_ATTESTATION);
                CHECK(stn_contract_snapshot_const_state(approval_two.contracts)->entries[0].current.sequence==4u);

                stn_chain_state_release(&execute_grant_state);stn_chain_state_release(&executed_state);
            }

            stn_chain_state_release(&approve_grant_one);stn_chain_state_release(&approval_one);
            stn_chain_state_release(&approve_grant_two);stn_chain_state_release(&approval_two);
        }

        stn_chain_state_release(&grant_state);stn_chain_state_release(&review_state);
    }

    stn_chain_state_release(&empty);stn_chain_state_release(&genesis_state);
    stn_chain_state_release(&accepted);stn_chain_state_release(&rejected);
}

int test_contract_snapshot(void)
{
    stn_contract_snapshot *a,*b,*shared;
    stn_contract_state_store *sa,*sb;
    stn_contract_participant p[1];
    stn_contract c={0};
    uint8_t draft[STN_CONTRACT_MAX_SIZE];
    size_t draft_length=0,index=99;

    memset(p,0,sizeof(p));memset(p[0].identity,0x44,32);
    p[0].role=STN_CONTRACT_ROLE_APPROVER;
    c.version=STN_CONTRACT_VERSION;c.type=STN_CONTRACT_GENERIC;
    c.state=STN_CONTRACT_STATE_DRAFT;c.participants=p;c.participant_count=1u;
    CHECK(stn_contract_encode(&c,draft,sizeof(draft),&draft_length)==STN_CONTRACT_OK);

    a=stn_contract_snapshot_create();CHECK(a!=NULL);
    sa=stn_contract_snapshot_state(a);CHECK(sa!=NULL);
    CHECK(stn_contract_snapshot_register(a,draft,draft_length,&index)==STN_CONTRACT_OK);
    CHECK(sa->entry_count==1u);
    CHECK(sa->entries[0].canonical_draft!=draft);
    {
        uint8_t saved=draft[0];
        draft[0]^=0xffu;
        CHECK(sa->entries[0].canonical_draft[0]!=draft[0]);
        draft[0]=saved;
    }

    b=stn_contract_snapshot_clone(a);CHECK(b!=NULL);
    sb=stn_contract_snapshot_state(b);CHECK(sb!=NULL);
    CHECK(sb!=sa && sb->entries!=sa->entries && sb->votes!=sa->votes);
    CHECK(sb->entry_count==1u);
    CHECK(memcmp(sb->entries[0].contract_id,sa->entries[0].contract_id,
        STN_ADDRESS_ID_SIZE)==0);
    CHECK(sb->entries[0].canonical_draft!=sa->entries[0].canonical_draft);
    CHECK(memcmp(sb->entries[0].canonical_draft,sa->entries[0].canonical_draft,
        sa->entries[0].canonical_draft_length)==0);

    sb->entries[0].current.sequence=7u;
    CHECK(sa->entries[0].current.sequence==0u);

    shared=stn_contract_snapshot_share(a);CHECK(shared==a);
    stn_contract_snapshot_release(shared);
    CHECK(stn_contract_snapshot_const_state(a)->entry_count==1u);

    stn_contract_snapshot_release(b);
    stn_contract_snapshot_release(a);

    {
        stn_chain_state owner={0},copy={0},moved={0};
        owner.contracts=stn_contract_snapshot_create();
        CHECK(owner.contracts!=NULL);
        CHECK(stn_chain_state_share(&owner,&copy)==STN_DATA_OK);
        CHECK(copy.contracts==owner.contracts);
        stn_chain_state_move(&moved,&copy);
        CHECK(copy.contracts==NULL && moved.contracts==owner.contracts);
        stn_chain_state_release(&owner);
        CHECK(moved.contracts!=NULL);
        stn_chain_state_release(&moved);
        CHECK(moved.contracts==NULL);
    }
    contract_chain_create();
    printf("Contract snapshot: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}
#ifdef STN_CONTRACT_SNAPSHOT_TEST_MAIN
int main(void){return test_contract_snapshot();}
#endif
