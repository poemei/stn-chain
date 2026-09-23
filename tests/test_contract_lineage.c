/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_contract_lineage.h"

#include <stdio.h>
#include <string.h>

static unsigned checks,failures;
#define CHECK(x) do{++checks;if(!(x)){++failures;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)

int test_contract_lineage(void)
{
    stn_contract draft={0},current;
    stn_contract_participant participants[2];
    uint8_t draft_bytes[STN_CONTRACT_MAX_SIZE];
    uint8_t current_bytes[STN_CONTRACT_MAX_SIZE];
    uint8_t id[STN_ADDRESS_ID_SIZE],before[STN_ADDRESS_ID_SIZE];
    static const uint8_t terms[]="deterministic contract";
    size_t draft_length=0,current_length=0;

    memset(participants,0,sizeof(participants));
    memset(participants[0].identity,0x11,32);
    memset(participants[1].identity,0x22,32);
    participants[0].role=STN_CONTRACT_ROLE_ISSUER;
    participants[1].role=STN_CONTRACT_ROLE_APPROVER;

    draft.version=STN_CONTRACT_VERSION;
    draft.type=STN_CONTRACT_GENERIC;
    draft.sequence=0u;
    draft.created_at=123456u;
    draft.state=STN_CONTRACT_STATE_DRAFT;
    draft.participants=participants;
    draft.participant_count=2u;
    draft.terms=terms;
    draft.terms_length=(uint32_t)(sizeof(terms)-1u);
    CHECK(stn_contract_encode(&draft,draft_bytes,sizeof(draft_bytes),
        &draft_length)==STN_CONTRACT_OK);

    current=draft;
    current.sequence=9u;
    current.state=STN_CONTRACT_STATE_ATTESTATION;
    CHECK(stn_contract_encode(&current,current_bytes,sizeof(current_bytes),
        &current_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_lineage_validate(draft_bytes,draft_length,
        current_bytes,current_length)==STN_CONTRACT_OK);
    memset(id,0,sizeof(id));
    CHECK(stn_contract_lineage_id(draft_bytes,draft_length,
        current_bytes,current_length,id)==STN_CONTRACT_OK);
    memcpy(before,id,sizeof(before));

    current.created_at++;
    CHECK(stn_contract_encode(&current,current_bytes,sizeof(current_bytes),
        &current_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_lineage_validate(draft_bytes,draft_length,
        current_bytes,current_length)==STN_CONTRACT_ADDRESS_ERROR);
    CHECK(stn_contract_lineage_id(draft_bytes,draft_length,
        current_bytes,current_length,id)==STN_CONTRACT_ADDRESS_ERROR);
    CHECK(memcmp(id,before,sizeof(id))==0);

    current=draft;
    participants[1].role=STN_CONTRACT_ROLE_ATTESTOR;
    CHECK(stn_contract_encode(&current,current_bytes,sizeof(current_bytes),
        &current_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_lineage_validate(draft_bytes,draft_length,
        current_bytes,current_length)==STN_CONTRACT_ADDRESS_ERROR);
    participants[1].role=STN_CONTRACT_ROLE_APPROVER;

    current=draft;
    current.terms=(const uint8_t *)"deterministic contracx";
    CHECK(stn_contract_encode(&current,current_bytes,sizeof(current_bytes),
        &current_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_lineage_validate(draft_bytes,draft_length,
        current_bytes,current_length)==STN_CONTRACT_ADDRESS_ERROR);

    current=draft;
    current.sequence=1u;
    CHECK(stn_contract_encode(&current,current_bytes,sizeof(current_bytes),
        &current_length)==STN_CONTRACT_OK);
    CHECK(stn_contract_lineage_validate(current_bytes,current_length,
        draft_bytes,draft_length)==STN_CONTRACT_STATE_ERROR);

    printf("Contract lineage: %u checks, %u failures.\n",checks,failures);
    return failures!=0u;
}
#ifdef STN_CONTRACT_LINEAGE_TEST_MAIN
int main(void){return test_contract_lineage();}
#endif
