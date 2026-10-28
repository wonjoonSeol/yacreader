#ifdef AC_CORE_WITH_NEON
#define STBIR_NEON
#define STB_IMAGE_RESIZE2_IMPLEMENTATION
#define STB_IMAGE_RESIZE_STATIC
#include <stb_image_resize2.h>

extern const auto stbir_resize_extended_neon = stbir_resize_extended;
#endif
