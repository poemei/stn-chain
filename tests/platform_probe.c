/* Compile-only detection probes. Simulated macros do not qualify a target. */
#include "../platforms/stn_build_config.h"
#ifdef STN_PROBE_BACKEND
#include "../platforms/stn_backend.h"
#endif
_Static_assert(STN_HOST_OS == STN_EXPECT_OS,"Wrong OS selection");
_Static_assert(STN_HOST_ARCH == STN_EXPECT_ARCH,"Wrong architecture selection");
