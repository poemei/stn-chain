/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_platform.h"
#include "../platforms/stn_backend.h" /* Application composition, not consensus. */

#include <stdio.h>
#include <stdlib.h>

int stn_windows_app(int argc,char **argv);
int main(int argc,char **argv) { return stn_windows_app(argc,argv); }
