# Anime4KCPP, trimmed

The CPU part of the Anime4KCPP core, enough to run its ACNet upscaler. YACReader uses it through `image_processing/acnet_upscale.h`.

| Files | Source | Revision | License |
|---|---|---|---|
| `core/`, `util/` | [TianZerL/Anime4KCPP](https://github.com/TianZerL/Anime4KCPP) | `fe187e020c8e57c6885511344a8705c292a92612` (2026-07-05) | MIT, `LICENSE-MIT` |
| `deps/stb/stb_image_resize2.h` | [nothings/stb](https://github.com/nothings/stb) | `2b2e64bc4778df6a06fbea3a5dbe61f7a76ecf19` (v2.18, the parent of `db69a22`, which moved the file to `deprecated/`) | MIT or public domain, `deps/stb/LICENSE` |
| `deps/ruapu/ruapu.h` | [nihui/ruapu](https://github.com/nihui/ruapu) | `62e6850aed73aeecd60317b7f28805194e0bb393` (2026-05-22, the newest before the Anime4KCPP revision; later ones also request AMX state from the OS) | MIT, `deps/ruapu/LICENSE` |
| `deps/half/half.hpp` | [half](https://half.sourceforge.net) | 2.2.1 | MIT, `deps/half/LICENSE.txt` |

stb_image_resize2.h has its own repository now, [nothings/stb_image_resize](https://github.com/nothings/stb_image_resize), which starts at v2.50 (`2768987`, 2026-10-19). Take newer versions from there.

Left out: image file I/O (`ImageIO.cpp`, stb_image, fpng), Eigen, OpenCL, CUDA, the video module, the CLI, GUI and bindings, and the ARNet, ArtCNN and FSRCNNX models. The FSRCNNX weights come from GPL-3.0 sources; the ACNet weights are Anime4KCPP's own.

Added for YACReader:

- `CMakeLists.txt` replaces the upstream build, which downloads its dependencies from their default branches.
- `backends/*.cpp` are the files upstream generates at configure time, each wrapped so it compiles to nothing unless its SIMD level is enabled.
- `.clang-format` keeps the repository's formatting off these files.
- `yacreader.patch` holds every change to the upstream files, listed below.

Changed upstream files:

- `Model.hpp`, `Param.hpp`, `Model.cpp`, `Processor.cpp`, `CPUProcessor.cpp`: ARNet, ArtCNN and FSRCNNX removed.
- `Parallel.hpp`, thread pool branch (the one built here): all loops share one pool instead of one pool per template instantiation, and `ac::util::parallelForThreads` caps the threads a loop uses. The pool takes its size from it on first use, so loops running at the same time share that cap.
- `CPUProcessor.cpp`: a scratch buffer that cannot be allocated throws `std::bad_alloc` instead of leaving a null pointer for the convolutions to write through.
- `ImageResize.cpp`: the SIMD resize function is chosen on first use instead of during static initialisation, so programs linking the library do not probe the CPU (`ruapu_init`) when they start.

To update, export the same files from a newer revision, then from the repository root run `git apply --directory=third_party/anime4kcpp third_party/anime4kcpp/yacreader.patch`, and regenerate the patch afterwards with `git diff --relative=third_party/anime4kcpp/ <commit that added the new upstream files> -- third_party/anime4kcpp/core third_party/anime4kcpp/util third_party/anime4kcpp/deps`.
