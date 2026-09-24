/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_chain.h"
#include "stn_transfer_envelope_authorization.h"
#include "stn_wallet.h"
#include "stn_sha256.h"
#include <stdio.h>
#include <string.h>

int stn_ed25519_sign(const uint8_t *,size_t,const uint8_t[32],const uint8_t[32],uint8_t[64]);
static unsigned checks,failures;
#define CHECK(e) do{++checks;if(!(e)){++failures;fprintf(stderr,"transfer chain line %d: %s\n",__LINE__,#e);}}while(0)

static int sign_envelope(stn_transfer_envelope *e,const uint8_t pk[32],const uint8_t sk[32]){
    uint8_t statement[STN_TRANSFER_ENVELOPE_AUTHORIZATION_STATEMENT_SIZE];
    if(stn_transfer_envelope_authorization_statement(e,statement)!=STN_DATA_OK)return -1;
    return stn_ed25519_sign(statement,sizeof(statement),pk,sk,e->signature);
}
static int make_block(stn_block *b,const stn_hash_provider *p,uint8_t *body,size_t body_capacity,
    uint8_t *wire,size_t wire_capacity,size_t *written,const stn_transaction_span *spans,size_t count){
    size_t n;
    memset(b,0,sizeof(*b));b->header.version=1;if(count>UINT32_MAX)return -1;b->header.transaction_count=(uint32_t)count;
    if(count>UINT32_MAX)return -1;\n    if(stn_block_body_encode(spans,(uint32_t)count,body,body_capacity,&n)!=STN_DATA_OK)return -1;
    if(n>UINT32_MAX)return -1;\n    b->header.body_length=(uint32_t)n;\n    b->body=body;
    if(stn_block_body_commitment(body,n,(uint32_t)count,p,b->header.transaction_commitment)!=STN_DATA_OK)return -1;
    return stn_block_encode(b,wire,wire_capacity,written)==STN_DATA_OK?0:-1;
}
static void qualification(void){
    static const uint8_t sk[32]={0x9d,0x61,0xb1,0x9d,0xef,0xfd,0x5a,0x60,0xba,0x84,0x4a,0xf4,0x92,0xec,0x2c,0xc4,0x44,0x49,0xc5,0x69,0x7b,0x32,0x69,0x19,0x70,0x3b,0xac,0x03,0x1c,0xae,0x7f,0x60};
    static const uint8_t pk[32]={0xd7,0x5a,0x98,0x01,0x82,0xb1,0x0a,0xb7,0xd5,0x4b,0xfe,0xd3,0xc9,0x64,0x07,0x3a,0x0e,0xe1,0x72,0xf3,0xda,0xa6,0x23,0x25,0xaf,0x02,0x1a,0x68,0xf7,0x07,0x51,0x1a};
    stn_hash_provider hp={stn_sha256,NULL};stn_chain_context ctx={0};stn_chain_state empty={0},funded={0},accepted={0},rebuilt={0},rejected={0};
    stn_block genesis={0},transfer_block={0};stn_issuance_record issue={0};stn_transfer_envelope envelope={0};
    stn_transaction tx={0};stn_transaction_span span;stn_address identity={0};
    uint8_t issue_bytes[STN_ISSUANCE_CANONICAL_SIZE],env_bytes[STN_TRANSFER_ENVELOPE_CANONICAL_SIZE];
    uint8_t tx1[256],tx2[256],body1[512],body2[512],wire1[1024],wire2[1024],genesis_id[32];
    size_t header_written=0,n1=0,n2=0,w1=0,w2=0;uint64_t units=0;stn_chain_report report;stn_block_span history[2];

    identity.type=STN_ADDRESS_IDENTITY;memcpy(identity.identifier,pk,32);
    CHECK(stn_wallet_derive(&identity,&envelope.transfer.source)==STN_DATA_OK);
    envelope.transfer.destination.type=STN_ADDRESS_WALLET;memset(envelope.transfer.destination.identifier,0x22,32);envelope.transfer.units=25;memcpy(envelope.controller,pk,32);envelope.nonce[31]=1;
    CHECK(sign_envelope(&envelope,pk,sk)==0);

    issue.reason=STN_ISSUANCE_REASON_BLOCK;issue.units=STN_ISSUANCE_BLOCK_UNITS;issue.destination.mining_identity=identity;issue.destination.wallet=envelope.transfer.source;
    CHECK(stn_issuance_encode(&issue,issue_bytes)==STN_DATA_OK);
    tx.version=1;tx.type=STN_TX_ISSUANCE;tx.record_bytes=issue_bytes;tx.record_length=sizeof(issue_bytes);
    CHECK(stn_transaction_encode(&tx,tx1,sizeof(tx1),&n1)==STN_DATA_OK);span.bytes=tx1;span.length=(uint32_t)n1;
    CHECK(make_block(&genesis,&hp,body1,sizeof(body1),wire1,sizeof(wire1),&w1,&span,1)==0);

    memcpy(ctx.network_id,genesis.header.network_id,32);ctx.genesis_bytes=wire1;ctx.genesis_length=w1;ctx.genesis_authority_roots=pk;ctx.genesis_authority_root_count=1;ctx.genesis_initial_identities=pk;ctx.genesis_initial_identity_count=1;ctx.hash_provider=hp;
    CHECK(stn_chain_initialize(&ctx,&empty)==STN_DATA_OK);
    report=stn_chain_validate_candidate(&ctx,&empty,wire1,w1,&funded);
    CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(stn_economic_state_balance(funded.economy,&envelope.transfer.source,&units)==STN_DATA_OK && units==STN_ISSUANCE_BLOCK_UNITS);

    CHECK(stn_transfer_envelope_encode(&envelope,env_bytes)==STN_DATA_OK);
    tx.type=STN_TX_TRANSFER;tx.record_bytes=env_bytes;tx.record_length=sizeof(env_bytes);
    CHECK(stn_transaction_encode(&tx,tx2,sizeof(tx2),&n2)==STN_DATA_OK);span.bytes=tx2;span.length=(uint32_t)n2;
    CHECK(stn_chain_block_id(wire1,w1,&hp,genesis_id)==STN_DATA_OK);
    CHECK(make_block(&transfer_block,&hp,body2,sizeof(body2),wire2,sizeof(wire2),&w2,&span,1)==0);
    transfer_block.header.height=1;transfer_block.header.timestamp=1;memcpy(transfer_block.header.previous_hash,genesis_id,32);
    CHECK(stn_block_header_encode(&transfer_block.header,wire2,STN_BLOCK_HEADER_SIZE,&header_written)==STN_DATA_OK && header_written==STN_BLOCK_HEADER_SIZE);

    report=stn_chain_validate_candidate(&ctx,&funded,wire2,w2,&accepted);
    CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(stn_economic_state_balance(accepted.economy,&envelope.transfer.source,&units)==STN_DATA_OK && units==75);
    CHECK(stn_economic_state_balance(accepted.economy,&envelope.transfer.destination,&units)==STN_DATA_OK && units==25);
    CHECK(accepted.economy->total_supply==STN_ISSUANCE_BLOCK_UNITS);

    report=stn_chain_validate_candidate(&ctx,&accepted,wire2,w2,&rejected);
    CHECK(report.acceptance!=STN_ACCEPTANCE_UNDER_CONTEXT);

    history[0].bytes=wire1;history[0].length=w1;history[1].bytes=wire2;history[1].length=w2;
    report=stn_chain_reconstruct_history(&ctx,history,2,&rebuilt);
    CHECK(report.acceptance==STN_ACCEPTANCE_UNDER_CONTEXT);
    CHECK(stn_economic_state_balance(rebuilt.economy,&envelope.transfer.source,&units)==STN_DATA_OK && units==75);
    CHECK(stn_economic_state_balance(rebuilt.economy,&envelope.transfer.destination,&units)==STN_DATA_OK && units==25);
    CHECK(rebuilt.economy->total_supply==accepted.economy->total_supply);

    stn_chain_state_release(&rebuilt);stn_chain_state_release(&accepted);stn_chain_state_release(&funded);stn_chain_state_release(&empty);
}
int test_transfer_chain(void){qualification();printf("Transfer Chain integration: %u checks, %u failures.\n",checks,failures);return failures?1:0;}
#ifdef STN_TRANSFER_CHAIN_TEST_MAIN
int main(void){return test_transfer_chain();}
#endif
