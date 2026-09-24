/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_transaction.h"
#include "stn_transfer_envelope.h"
#include <stdio.h>
#include <string.h>
static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
int test_transfer_transaction(void){
 stn_transfer_envelope e={0};stn_transaction tx={0},decoded={0};uint8_t envelope[STN_TX_TRANSFER_SIZE];
 uint8_t wire[STN_TX_HEADER_SIZE+STN_TX_TRANSFER_SIZE],saved[sizeof(wire)];size_t written=99;
 memset(e.controller,0x11,sizeof(e.controller));e.nonce[31]=1;e.transfer.source.type=STN_ADDRESS_WALLET;
 e.transfer.destination.type=STN_ADDRESS_WALLET;memset(e.transfer.source.identifier,0x22,32);
 memset(e.transfer.destination.identifier,0x33,32);e.transfer.units=25;memset(e.signature,0x44,sizeof(e.signature));
 CHECK(stn_transfer_envelope_encode(&e,envelope)==STN_DATA_OK);
 tx.version=1;tx.type=STN_TX_TRANSFER;tx.record_bytes=envelope;tx.record_length=sizeof(envelope);
 CHECK(stn_transaction_encode(&tx,wire,sizeof(wire),&written)==STN_DATA_OK);
 CHECK(written==sizeof(wire));
 CHECK(wire[0]=='S'&&wire[1]=='T'&&wire[2]=='N'&&wire[3]=='T'&&wire[7]==STN_TX_TRANSFER);
 CHECK(stn_transaction_decode(wire,sizeof(wire),&decoded)==STN_DATA_OK);
 CHECK(decoded.version==1&&decoded.type==STN_TX_TRANSFER&&decoded.record_length==STN_TX_TRANSFER_SIZE);
 CHECK(decoded.record_bytes==wire+STN_TX_HEADER_SIZE);
 CHECK(stn_transaction_validate_structure(wire,sizeof(wire))==STN_DATA_OK);
 memcpy(saved,wire,sizeof(wire));saved[STN_TX_HEADER_SIZE]=2;
 CHECK(stn_transaction_validate_structure(saved,sizeof(saved))==STN_DATA_CONTENT);
 memcpy(saved,wire,sizeof(wire));memset(saved+STN_TX_HEADER_SIZE+1+32,0,32);
 CHECK(stn_transaction_validate_structure(saved,sizeof(saved))==STN_DATA_CONTENT);
 CHECK(stn_transaction_validate_structure(wire,sizeof(wire)-1)==STN_DATA_LENGTH);
 tx.record_length=STN_TX_TRANSFER_SIZE-1;written=99;memset(saved,0xa5,sizeof(saved));
 CHECK(stn_transaction_encode(&tx,saved,sizeof(saved),&written)==STN_DATA_CONTENT);
 CHECK(written==0);
 tx.record_length=STN_TX_TRANSFER_SIZE;written=99;
 CHECK(stn_transaction_encode(&tx,saved,sizeof(saved)-1,&written)==STN_DATA_CAPACITY);
 CHECK(written==0);
 CHECK(stn_transaction_decode(NULL,0,&decoded)==STN_DATA_ARGUMENT);
 CHECK(stn_transaction_decode(wire,sizeof(wire),NULL)==STN_DATA_ARGUMENT);
 printf("Transfer transaction: %u checks, %u failures.\n",checks,failures);return failures?1:0;
}
#ifdef STN_TRANSFER_TRANSACTION_TEST_MAIN
int main(void){return test_transfer_transaction();}
#endif
