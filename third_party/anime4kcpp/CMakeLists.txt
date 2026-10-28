# Anime4KCPP core, CPU processor only. README.md lists what is vendored and what was changed.

add_library(anime4kcpp STATIC
    core/src/Alloc.cpp
    core/src/Image.cpp
    core/src/ImageProcess.cpp
    core/src/ImageResize.cpp
    core/src/Model.cpp
    core/src/SIMD.cpp
    core/src/processor/Processor.cpp
    core/src/processor/cpu/CPUProcessor.cpp
    backends/Generic.cpp
    backends/SSE.cpp
    backends/AVX.cpp
    backends/FMA.cpp
    backends/NEON.cpp
    backends/ImageResizeAVX.cpp
    backends/ImageResizeNEON.cpp
)
set_target_properties(anime4kcpp PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

include(GenerateExportHeader)
generate_export_header(anime4kcpp
    BASE_NAME AC_CORE
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/include/ACCoreExport.hpp
)

target_include_directories(anime4kcpp
    PUBLIC
        core/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
        util/misc/include
        util/parallel/include
        util/threads/include
    PRIVATE
        core/internal
        deps/half
        deps/ruapu
        deps/stb
)

target_compile_definitions(anime4kcpp
    PUBLIC
        AC_CORE_STATIC_DEFINE
        AC_CORE_DISABLE_IMAGE_IO
    PRIVATE
        AC_CORE_WITH_HALF
)
if(WIN32)
    target_compile_definitions(anime4kcpp PRIVATE AC_CORE_HAVE_WIN32_ALIGNED_MALLOC)
else()
    target_compile_definitions(anime4kcpp PRIVATE AC_CORE_HAVE_POSIX_MEMALIGN)
endif()

find_package(Threads REQUIRED)
target_link_libraries(anime4kcpp PUBLIC Threads::Threads)

# SIMD backends. The processor picks the fastest one the CPU supports at run time, so a backend
# may only use its instructions inside its own functions. With GCC and Clang that takes per-file
# flags such as -mavx, and then the linker may keep the AVX copy of an inline function that
# other files share, which crashes CPUs without AVX. So AVX and FMA are only built where that
# cannot happen: MSVC needs no flag for intrinsics, and every Mac that runs our deployment target
# (macOS 12) has AVX, though not always FMA. SSE and NEON are part of the x86_64 and arm64
# baselines and need no flag.
if(APPLE)
    foreach(arch IN LISTS CMAKE_OSX_ARCHITECTURES)
        if(arch STREQUAL "x86_64")
            target_compile_options(anime4kcpp PRIVATE
                "SHELL:-Xarch_x86_64 -DAC_CORE_WITH_SSE"
                "SHELL:-Xarch_x86_64 -DAC_CORE_WITH_AVX"
            )
            set_source_files_properties(backends/AVX.cpp backends/ImageResizeAVX.cpp
                PROPERTIES COMPILE_OPTIONS "SHELL:-Xarch_x86_64 -mavx")
            message(STATUS "Anime4KCPP backends on x86_64: Generic SSE AVX")
        elseif(arch STREQUAL "arm64")
            target_compile_options(anime4kcpp PRIVATE "SHELL:-Xarch_arm64 -DAC_CORE_WITH_NEON")
            message(STATUS "Anime4KCPP backends on arm64: Generic NEON")
        endif()
    endforeach()
else()
    set(anime4kcpp_simd "")
    # MSVC itself, not if(MSVC), which clang-cl matches too: clang-cl needs -mavx for AVX
    # intrinsics like Clang, so it takes the branches below
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
            set(anime4kcpp_simd SSE AVX FMA)
        elseif(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64")
            set(anime4kcpp_simd NEON)
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
        set(anime4kcpp_simd SSE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(anime4kcpp_simd NEON)
    endif()
    list(TRANSFORM anime4kcpp_simd PREPEND AC_CORE_WITH_ OUTPUT_VARIABLE anime4kcpp_simd_definitions)
    target_compile_definitions(anime4kcpp PRIVATE ${anime4kcpp_simd_definitions})
    list(JOIN anime4kcpp_simd " " anime4kcpp_simd_names)
    message(STATUS "Anime4KCPP backends: Generic ${anime4kcpp_simd_names}")
endif()
