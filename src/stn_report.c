/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_report.h"

#include <stdio.h>

const char *stn_report_event_name(stn_report_event event)
{
    static const char *names[] = {
        "START", "PEER", "SYNC", "BLOCK", "REORG", "STOP", "ERROR"
    };

    if(event < STN_REPORT_START || event > STN_REPORT_ERROR) {
        return "ERROR";
    }

    return names[(size_t)event];
}

int stn_report_format(
    stn_report_event event,
    const char *message,
    char *output,
    size_t capacity)
{
    int written;

    if(message == NULL || output == NULL || capacity == 0) {
        return 0;
    }

    written = snprintf(
        output,
        capacity,
        "[%s] %s",
        stn_report_event_name(event),
        message);

    return written >= 0 && (size_t)written < capacity;
}
