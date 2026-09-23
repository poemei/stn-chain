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
static const uint8_t create_grant_signature[64]={
    0x7a,0x5a,0xbc,0xca,0xd8,0x81,0x30,0x1c,0x6c,0x89,0x46,0xda,0x35,0x40,0x19,0x16,
    0x5e,0x1a,0x74,0xd3,0xe9,0x11,0x76,0x79,0x22,0x2f,0x99,0xee,0x5f,0x94,0xd9,0xac,
    0x0c,0x24,0x43,0x08,0xf2,0x41,0xe4,0xb1,0x31,0x72,0x27,0xaa,0xbe,0x8a,0x71,0x65,
    0x34,0x6b,0x0f,0x7b,0xce,0x0c,0xa1,0x4f,0x23,0xdc,0xf8,0xb0,0x5f,0x44,0x19,0x06
};
static const uint8_t create_action_signature[64]={
    0xe3,0x71,0xeb,0x9c,0xbe,0x9c,0x46,0x51,0x52,0x3a,0xa8,0x4f,0xa6,0xe7,0x80,0xfe,
    0x06,0xde,0x93,0x96,0xab,0x21,0xf4,0xc1,0x34,0x2b,0xfc,0x9b,0xc8,0x06,0x9a,0x0b,
    0xfe,0x1a,0x18,0x01,0x96,0x82,0x73,0xc4,0x8e,0xf2,0x22,0x5a,0xb6,0x7d,0x23,0xd8,
    0xe3,0x83,0x0a,0x3e,0x42,0xb6,0xe2,0x56,0x8d,0xf2,0xff,0x55,0x5b,0xc6,0x68,0x04
};

static const uint8_t amend_grant_signature[64]={
    0x5c,0x48,0x54,0x27,0x5a,0x7b,0x72,0x26,0xcc,0x05,0x46,0xba,0x25,0x8f,0x4a,0x7d,
    0x8b,0xa9,0xff,0xe9,0xb9,0x09,0xec,0x28,0xcb,0x73,0x78,0x42,0x10,0xd1,0xcc,0x23,
    0x83,0xeb,0x4f,0xd5,0x7f,0xb1,0xa4,0x48,0x61,0xe1,0xdd,0x8d,0x5e,0x31,0xfb,0xea,
    0x20,0x37,0xbf,0x8f,0xb1,0x35,0x31,0x48,0x1e,0xe2,0xfa,0x26,0x84,0xb4,0x8d,0x09
};
static const uint8_t amend_action_signature[64]={
    0xa4,0x86,0x8d,0x11,0x69,0x29,0x44,0xa0,0x73,0xbe,0x4f,0xdd,0x41,0x3e,0x97,0x75,
    0x57,0x88,0xb5,0xf6,0x23,0x73,0x26,0xba,0x0b,0x74,0xd9,0x8f,0xea,0xa0,0x93,0xe0,
    0x7a,0x05,0x75,0xe0,0x23,0x9c,0xeb,0x04,0xeb,0x78,0xc2,0x96,0x18,0xe0,0x46,0x35,
    0x9f,0x9d,0x32,0xb2,0x1a,0x29,0xb8,0x46,0x9a,0x53,0x67,0x92,0xdc,0x7d,0xf1,0x07
};

static void contract_chain_create(void)
{
    static const uint8_t terms[]={'T'};
    stn_contract_participant participant={0};
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

    memcpy(participant.identity,chain_root,sizeof(chain_root));
    participant.role=STN_CONTRACT_ROLE_APPROVER;
    draft.version=STN_CONTRACT_VERSION;draft.type=STN_CONTRACT_GENERIC;
    draft.sequence=0u;draft.created_at=1u;draft.state=STN_CONTRACT_STATE_DRAFT;
    draft.participants=&participant;draft.participant_count=1u;
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

        issued.participants=&participant;issued.participant_bytes=NULL;
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
