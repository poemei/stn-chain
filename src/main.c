/* Copyright (c) 2026 STN-Labz. See docs/LICENSE.md. */
#include "stn_platform.h"
#include "../platforms/stn_backend.h" /* Application composition, not consensus. */

#if STN_HOST_OS == STN_OS_WINDOWS

int stn_windows_app(int argc, char **argv);

int main(int argc, char **argv)
{
    return stn_windows_app(argc, argv);
}

#elif STN_HOST_OS == STN_OS_LINUX

int stn_linux_app(int argc, char **argv);

int main(int argc, char **argv)
{
    return stn_linux_app(argc, argv);
}

#else

#error STN_APPLICATION_PLATFORM_NOT_IMPLEMENTED

#endif