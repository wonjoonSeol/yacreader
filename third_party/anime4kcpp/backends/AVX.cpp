#ifdef AC_CORE_WITH_AVX
#include "AC/Core/Internal/Processor/CPU/X86/AVX.hpp"

#define BACKEND_NAME AVX
#define LAYER_SUFFIX avx
#include "AC/Core/Internal/Processor/CPU/Backend.hpp"
#endif
