# Anime4KCPP, trimmed

The CPU part of the Anime4KCPP core, enough to run its ACNet upscaler. YACReader uses it through `image_processing/acnet_upscale.h`.

| Files | Source | Revision | License |
|---|---|---|---|
| `core/`, `util/` | [TianZerL/Anime4KCPP](https://github.com/TianZerL/Anime4KCPP) | `fe187e020c8e57c6885511344a8705c292a92612` (2026-07-05) | MIT, `LICENSE-MIT` |
| `deps/stb/stb_image_resize2.h` | [nothings/stb](https://github.com/nothings/stb) | `2b2e64bc4778df6a06fbea3a5dbe61f7a76ecf19` (v2.18, the parent of `db69a22`, which moved the file to `deprecated/`) | MIT or public domain, `deps/stb/LICENSE` |
| `deps/ruapu/ruapu.h` | [nihui/ruapu](https://github.com/nihui/ruapu) | `62e6850aed73aeecd60317b7f28805194e0bb393` (2026-05-22, the newest before the Anime4KCPP revision; later ones also request AMX state from the OS) | MIT, `deps/ruapu/LICENSE` |
| `deps/half/half.hpp` | [half](https://half.sourceforge.net) | 2.2.1 | MIT, `deps/half/LICENSE.txt` |

Left out: image file I/O (`ImageIO.cpp`, stb_image, fpng), Eigen, OpenCL, CUDA, the video module, the CLI, GUI and bindings, and the ARNet, ArtCNN and FSRCNNX models. The FSRCNNX weights come from GPL-3.0 sources; the ACNet weights are Anime4KCPP's own.

Added for YACReader:

- `CMakeLists.txt` replaces the upstream build, which downloads its dependencies from their default branches.
- `backends/*.cpp` are the files upstream generates at configure time, each wrapped so it compiles to nothing unless its SIMD level is enabled.
- `.clang-format` keeps the repository's formatting off these files.

Changed upstream files:

- `Model.hpp`, `Param.hpp`, `Model.cpp`, `Processor.cpp`, `CPUProcessor.cpp`: ARNet, ArtCNN and FSRCNNX removed.
- `Parallel.hpp`, thread pool branch (the one built here): all loops share one pool instead of one pool per template instantiation, and `ac::util::parallelForThreads` caps the threads a loop uses.

To update, export the same files from a newer revision and reapply these changes; the commit after the one that added the unmodified files shows them.
