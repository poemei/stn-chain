/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_report.h"

#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;

static void check(int condition, const char *name)
{
    ++checks;
    if(!condition) {
        ++failures;
        printf("FAIL: %s\n", name);
    }
}

int main(void)
{
    char line[128];
    char tiny[4];

    check(strcmp(stn_report_event_name(STN_REPORT_START), "START") == 0, "start name");
    check(strcmp(stn_report_event_name(STN_REPORT_PEER), "PEER") == 0, "peer name");
    check(strcmp(stn_report_event_name(STN_REPORT_SYNC), "SYNC") == 0, "sync name");
    check(strcmp(stn_report_event_name(STN_REPORT_BLOCK), "BLOCK") == 0, "block name");
    check(strcmp(stn_report_event_name(STN_REPORT_REORG), "REORG") == 0, "reorg name");
    check(strcmp(stn_report_event_name(STN_REPORT_STOP), "STOP") == 0, "stop name");
    check(strcmp(stn_report_event_name(STN_REPORT_ERROR), "ERROR") == 0, "error name");
    check(strcmp(stn_report_event_name((stn_report_event)99), "ERROR") == 0, "invalid name");

    check(stn_report_format(STN_REPORT_SYNC, "Complete height=42", line, sizeof(line)), "format");
    check(strcmp(line, "[SYNC] Complete height=42") == 0, "formatted content");
    check(!stn_report_format(STN_REPORT_START, "x", tiny, sizeof(tiny)), "capacity");
    check(!stn_report_format(STN_REPORT_START, NULL, line, sizeof(line)), "null message");

    printf("Operational reporting: %u checks, %u failures.\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
