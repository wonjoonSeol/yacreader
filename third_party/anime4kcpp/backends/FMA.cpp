#ifdef AC_CORE_WITH_FMA
#include "AC/Core/Internal/Processor/CPU/X86/AVX.hpp"

#define BACKEND_NAME FMA
#define LAYER_SUFFIX fma
#include "AC/Core/Internal/Processor/CPU/Backend.hpp"
#endif
