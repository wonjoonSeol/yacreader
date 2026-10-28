#ifdef AC_CORE_WITH_NEON
#include "AC/Core/Internal/Processor/CPU/ARM/NEON.hpp"

#define BACKEND_NAME NEON
#define LAYER_SUFFIX neon
#include "AC/Core/Internal/Processor/CPU/Backend.hpp"
#endif
