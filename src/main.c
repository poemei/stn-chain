/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_platform.h"
#include "../platforms/stn_backend.h" /* Application composition, not consensus. */

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    if (puts("STN Chain - development scaffold; node not implemented.") == EOF) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
