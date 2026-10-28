## About this fork

A personal fork of [YACReader](https://github.com/YACReader/yacreader) that changes the comic reader. Each change has a PR. The closed ones are previews that show the change as a diff against upstream `develop`, ready to send upstream if the maintainer wants it. The upstream README starts after the line.

| Change | Kind | PR |
|---|---|---|
| [Choose the page comics open at](#choose-the-page-comics-open-at) | Option | [#3](https://github.com/wonjoonSeol/yacreader/pull/3) |
| [Fit pages to height in continuous scroll](#fit-pages-to-height-in-continuous-scroll) | Option | [#5](https://github.com/wonjoonSeol/yacreader/pull/5) |
| [Image adjustment menu](#image-adjustment-menu) | Feature | [#12](https://github.com/wonjoonSeol/yacreader/pull/12) |
| [Go 5 or 10 pages](#go-5-or-10-pages) | Shortcuts | [#4](https://github.com/wonjoonSeol/yacreader/pull/4) |
| [Page number when the page changes](#page-number-when-the-page-changes) | Display | [#11](https://github.com/wonjoonSeol/yacreader/pull/11) |
| [Toolbar in full screen](#toolbar-in-full-screen) | Feature | [#8](https://github.com/wonjoonSeol/yacreader/pull/8) |
| [Next file after an unreadable one](#next-file-after-an-unreadable-one) | Bug fix | [#1](https://github.com/wonjoonSeol/yacreader/pull/1) |
| [Comics removed from the Reading list](#comics-removed-from-the-reading-list) | Bug fix | [#2](https://github.com/wonjoonSeol/yacreader/pull/2) |
| [Lanczos3 and Lanczos2 scaling](#lanczos3-and-lanczos2-scaling) | Option | [#6](https://github.com/wonjoonSeol/yacreader/pull/6) |
| [Page turns without scaling](#page-turns-without-scaling) | Speed | [#13](https://github.com/wonjoonSeol/yacreader/pull/13) |
| [Sharper enlarged pages with Anime4K](#sharper-enlarged-pages-with-anime4k) | Option | [#14](https://github.com/wonjoonSeol/yacreader/pull/14) |

**Builds:** open a successful run of [Build](https://github.com/wonjoonSeol/yacreader/actions/workflows/build.yml) and download the `windows-x64-qt6-…` installer or the macOS `.dmg`. They are unsigned, need a GitHub sign-in, and expire after 90 days.

**Screenshots** come from the fork's Linux build with generated sample pages. "Upstream" shots run the same steps on upstream `develop` (268288b).

### Choose the page comics open at

Options > General > **Open comics at**

<img src="docs/fork/open-comics-at-options.png" width="600" alt="The Open comics at group in Options, General">

| Setting | Finished comic | Unfinished comic |
|---|---|---|
| The last page read (default) | last page | last page read |
| The first page if the comic is finished, otherwise the last page read | **first page** | last page read |
| The first page | **first page** | **first page** |

- **Finished:** the reading progress is on the last page, the same rule the library uses to mark a comic as read. Closing a comic on its final double page spread counts too.
- **Where it applies:** comics opened from the library and comics opened as files.
- **Requested on the forum:** [Read comic are opened from the last page](https://www.yacreader.com/forum/support-ios/2096-read-comic-are-opened-from-the-last-page).

### Fit pages to height in continuous scroll

Options > Image adjustment > **Continuous scroll**

<img src="docs/fork/fit-to-height-options.png" width="600" alt="The Continuous scroll group in Options, Image adjustment">

| Fit pages to width (default) | Fit pages to height |
|---|---|
| <img src="docs/fork/continuous-fit-width.png" width="400" alt="Continuous scroll with the page as wide as the window"> | <img src="docs/fork/continuous-fit-height.png" width="400" alt="Continuous scroll with the whole page as tall as the window"> |
| Pages are as wide as the window, so you scroll through each one | Pages are as tall as the window, so you see each one whole |

- A page that would end up wider than the window, such as a double page spread, fits the width instead.
- Comics marked as web comics in the library always fit the width.

### Image adjustment menu

Right-click the page > **Image adjustment**, or click the arrow next to the Options button on the toolbar. It changes the options of Options > Image adjustment while you read:

<img src="docs/fork/image-adjustment-context-menu.png" width="600" alt="The right-click menu on a page with its Image adjustment submenu open">

| In the menu | Options > Image adjustment |
|---|---|
| Brightness, Contrast, Gamma, Reset | Image options |
| Enlarge images to fit width/height | Fit options |
| Continuous scroll > Fit pages to width / height | Continuous scroll |
| Scaling method > Nearest / Bilinear / Lanczos4 / Lanczos3 / Lanczos2 | Scaling |

| Moving a slider | The arrow next to Options |
|---|---|
| <img src="docs/fork/image-adjustment-brightness.png" width="400" alt="The page brighter with Brightness at 15 in the open menu"> | <img src="docs/fork/image-adjustment-toolbar-menu.png" width="400" alt="The same menu under the Options button with the scaling methods open"> |
| The menu stays open and the page changes when you let go | Works in every mouse mode |

- **Same settings as Options:** a change in either one shows in the other at once.
- **Brightness, Contrast, Gamma:** the page changes when you let go of a slider, or 150 ms after the last spin box step, so holding an arrow key or turning the wheel filters the pages once. The same goes for the sliders in Options.
- **Right-click** opens the menu unless Options > General > Mouse mode is "Use the Left/Right buttons to turn pages", where the right button turns the page. The arrow next to Options works in every mode.
- **Full screen:** the toolbar is hidden. Right-click still opens the menu, except in the Left/Right buttons mouse mode; there, show the toolbar with Hide/show toolbar (H) to reach the arrow.
- **macOS:** right-click only. The native toolbar has no arrow.

### Go 5 or 10 pages

<img src="docs/fork/page-jumps-shortcuts.png" width="600" alt="The four new shortcuts in Edit shortcuts, Reading">

| Keys | Action |
|---|---|
| Shift+← / Shift+→ | Go 5 pages left / right |
| Alt+← / Alt+→ | Go 10 pages left / right |

- **Manga mode:** the directions flip, as they do for ← and →.
- **Double page mode:** the 5 page jump moves 4 pages, so spreads stay paired.
- **Changing the keys:** Options > Shortcuts > Edit shortcuts > Reading. If one of these keys is already bound to another action, that jump starts unassigned.

### Page number when the page changes

Options > General > Display > **Briefly show the page number when the page changes**, off by default. With it on, turning a page shows the page number in the top left corner for a moment, then it fades out.

<img src="docs/fork/page-counter-option.png" width="600" alt="The Display group in Options, General, with the option turned on">

| Single page | Double page |
|---|---|
| <img src="docs/fork/page-counter-single-page.png" width="400" alt="10/100 in the top left corner after turning to page 10"> | <img src="docs/fork/page-counter-double-page.png" width="400" alt="12-13/100 in the top left corner after turning to pages 12 and 13"> |

- **When:** next and previous page, Go 5 or 10 pages, Go to page, the page flow, bookmarks, first and last page, opening a comic, and scrolling onto another page in continuous scroll. It stays hidden when the page doesn't change, such as Next on the last page.
- **Double page mode:** a spread shows both pages, such as 12-13/100. In manga mode they read right to left, 13-12/100, as in the page information label.
- **Show Info (I):** while the page information label is on, the counter stays hidden because the label already shows the page.
- **Look:** the colors of the page information label in the current theme. It stays for 1 second and fades out in 0.4 seconds; on macOS it hides after the second without fading.

### Toolbar in full screen

Full screen (F) has no title bar and hides the toolbar. Move the mouse to the top of the screen to show the toolbar over the page, where it covers the page number in the top right corner. It hides again when the mouse moves back down to the page or leaves the screen, and when another window comes to the front (Alt+Tab).

| Full screen | Mouse at the top of the screen |
|---|---|
| <img src="docs/fork/fullscreen-toolbar-hidden.png" width="400" alt="A page in full screen with no toolbar"> | <img src="docs/fork/fullscreen-toolbar-shown.png" width="400" alt="The toolbar over the top of the same page"> |

<img src="docs/fork/fullscreen-toolbar-buttons.png" width="480" alt="Exit fullscreen mode and Close at the right end of the toolbar">

- **Exit fullscreen and close:** two buttons at the right end of the toolbar, shown only in full screen.
- **The page stays still:** the toolbar covers the top of the page instead of pushing it down.
- **Not while** the magnifying glass is on or a mouse button is held, so dragging a page up to the top does not open it.
- **macOS:** unchanged. In full screen macOS shows its own menu bar and window buttons at the top of the screen.

### Next file after an unreadable one

In a folder with `Comic 1.pdf`, an unreadable `Comic 2.cbz` and `Comic 3.pdf`, going from Comic 1 to the next comic shows this error:

<img src="docs/fork/prev-next-error.png" width="400" alt="Format not supported error">

After closing it:

| | Previous / next comic buttons |
|---|---|
| Upstream | <img src="docs/fork/prev-next-toolbar-before.png" width="560" alt="Both buttons greyed out"> |
| This fork | <img src="docs/fork/prev-next-toolbar-after.png" width="560" alt="Both buttons available"> |

Upstream disables both buttons and their shortcuts (Ctrl+← and Ctrl+→), so the next file has to be opened by hand. In the fork, Ctrl+→ opens Comic 3.

### Comics removed from the Reading list

Removing a comic from the library's Reading list resets its progress to 0. Opening it again:

| Upstream | This fork |
|---|---|
| <img src="docs/fork/reading-list-before.png" width="400" alt="0/10 and Loading, please wait"> | <img src="docs/fork/reading-list-after.png" width="400" alt="Page 1 of 10"> |
| Shows 0/10, stays on "Loading...please wait!", then crashes | Opens at page 1 |

### Lanczos3 and Lanczos2 scaling

Options > Image adjustment > **Scaling** > Scaling method

<img src="docs/fork/scaling-options.png" width="600" alt="The Scaling group in Options, Image adjustment, with Lanczos3 selected">

| Method | Lobes | What you get |
|---|---|---|
| Lanczos4 (default) | 4 | Upstream's Lanczos under a new name. The most fine detail and the most halos around lines |
| Lanczos3 | 3 | In between |
| Lanczos2 | 2 | The fewest halos, a little less fine detail |

The same line art enlarged 3x, with the boxed part shown 3x larger again below it. The light bands next to the strokes are the halos:

<img src="docs/fork/scaling-comparison.png" width="900" alt="The same line art enlarged 3x with Lanczos4, Lanczos3 and Lanczos2, with a close-up of the letter A">

Measured on a grey edge (64 to 192) and on fine stripes, both enlarged 4x:

| | Lanczos4 | Lanczos3 | Lanczos2 | Bilinear |
|---|---|---|---|---|
| Overshoot past the edge (grey levels) | 16 | 15 | 10 | 0 |
| Pixels from the edge that change | 13 | 9 | 6 | 2 |
| Edge rise from 10% to 90% (px) | 3.0 | 3.0 | 3.0 | 3.4 |
| Contrast kept on stripes with a 3 px period | 98% | 96% | 90% | 77% |

- **Edges** are equally steep with all three. The difference is in the halos and the finest detail.
- **Shrinking** 4x, the overshoot is at most 2 grey levels with any of them.
- **Switching back:** older builds of this fork and upstream YACReader don't have Lanczos3 and Lanczos2. With one of them chosen they skip their own scaling, so pages are scaled at a lower quality, and on HiDPI screens single pages show at their own size instead of fitting the window. Pick Lanczos4 before switching back.
- **The other way round,** a method this build doesn't know shows as Lanczos4 and stays stored until you pick another one.
- **Requested upstream:** [Algorithms for image scaling](https://github.com/YACReader/yacreader/issues/529) asks for Lanczos3.

### Page turns without scaling

In single and double page mode the pages next to the one you read are scaled to the window in the background, so a page turn shows a page that is ready instead of scaling it first.

<img src="docs/fork/prescale-page-turn-time.png" width="700" alt="Time a page turn keeps the reader busy before and after this change, for single and double pages">

| Single page mode | Double page mode |
|---|---|
| <img src="docs/fork/prescale-single-page.png" width="400" alt="Page 2 of a sample comic in single page mode"> | <img src="docs/fork/prescale-double-page.png" width="400" alt="Pages 4 and 5 of a sample comic in double page mode"> |

- **What gets ready:** the page shown, the next two and the previous one, in that order. In double page mode the pages are paired the way the next page turns will pair them.
- **Looks the same:** the pages are pixel for pixel what the reader showed before, in single, double, manga and rotated mode, and in the magnifier.
- **When a page is not ready yet,** for example right after jumping far or when flipping through double pages faster than they decode, it is scaled when it is shown, as before.
- **Memory:** at most 256 MiB of scaled views, always including the one shown. Fitted to the window a 1440 x 2160 view takes about 12 MB, so all four fit; zoomed in far, or fitted to the width of a large screen, the views at the end of that order are left out.
- **Continuous scroll** already scaled its pages in the background and is unchanged.

### Sharper enlarged pages with Anime4K

Options > Image adjustment > Scaling > **Upscale small pages with Anime4K (ACNet)**, off by default, with a **Model** choice: F8B4 (default) or F8B8-HDN.

In single page, double page and continuous scroll mode, a page shown at 1.5 times its own size or more is first shown scaled as usual, then replaced by a sharper one: [Anime4KCPP](https://github.com/TianZerL/Anime4KCPP)'s ACNet, a small neural network for line art, doubles it on the CPU in the background, and Lanczos takes it to the window size. The network sharpens the luma; colour and transparency are scaled with Catmull-Rom. The code is vendored in `third_party/anime4kcpp` (MIT) with the revisions it came from and a patch of the changes made to it.

<img src="docs/fork/anime4k-crop.png" width="700" alt="The same lettering doubled with Lanczos4 and with ACNet F8B4, shown at 4x">

Quality: the page shrunk to half with a 2x2 box filter, doubled again, and compared with the original (whole page, PSNR / SSIM on luma).

| Method | Krazy Kat page | Synthetic gray page | Synthetic colour page |
|---|---|---|---|
| Lanczos4 (the default scaling) | 25.8 dB / 0.931 | 22.5 / 0.946 | 27.7 / 0.969 |
| **ACNet F8B4 (default model)** | **29.5 / 0.970** | **26.4 / 0.980** | **31.5 / 0.990** |
| ACNet F8B8-HDN | 29.9 / 0.969 | 26.6 / 0.981 | 31.7 / 0.990 |
| Anime4K 0.9, CPU port | 21.8 / 0.888 | 18.1 / 0.892 | 24.0 / 0.957 |
| ArtCNN C4F16, CPU (not included) | 30.2 / 0.973 | 28.2 / 0.987 | 32.6 / 0.992 |
| Anime4K v4 CNN M, GPU shader | 29.4 / 0.966 | 25.9 / 0.979 | 32.0 / 0.989 |
| Anime4K v4 CNN UL, GPU shader | 30.5 / 0.970 | 28.4 / 0.987 | 33.0 / 0.992 |

The Krazy Kat page is George Herriman's of 1922-04-02, public domain ([Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Krazy_Kat_1922-04-02_original.jpg)), in gray at 2482 x 3000. The synthetic pages are generated 2000 x 3000 manga-style pages.

| Hatching | Lettering |
|---|---|
| <img src="docs/fork/anime4k-krazykat-hatching.png" width="400" alt="Hatching from the Krazy Kat page doubled by each method"> | <img src="docs/fork/anime4k-krazykat-lettering.png" width="400" alt="Lettering from the Krazy Kat page doubled by each method"> |

Speed of one 2x upscale of a 1000 x 1500 page, in ms:

| | Gray | Colour |
|---|---|---|
| Lanczos4, one thread (WSL) | 37 | 39 |
| ACNet F8B4, 8 threads (Windows) | 46 | 66 |
| ACNet F8B8-HDN, 8 threads (Windows) | 82 | 99 |
| ACNet F8B4, 1 / 4 / 16 threads (WSL) | 229 / 71 / 43 | 253 / 87 / 61 |
| ACNet F8B8-HDN, 1 / 4 / 16 threads (WSL) | 345 / 132 / 72 | not measured cleanly |

Ryzen 7 5700X3D with the FMA backend, the one Windows x64 builds pick on this CPU. The Windows row is the MSVC build's test; the WSL rows are the best of 5 to 7 runs on a machine shared with other builds, so allow about ±40%. Linux x86_64 builds use the SSE backend, about 1.7 times slower.

- **CPU:** the network runs on one pool of half the CPU's threads, shared by all upscales, with the fastest backend the CPU supports: FMA or AVX on Windows x64, AVX on Intel Macs, NEON on arm64, SSE on Linux x86_64.
- **Memory:** the network works on strips of 256 rows, so its scratch memory stays near 28 MB for a 1600 pixel wide page. A 1600 x 2400 colour page peaks at about 157 MB more than before the upscale, Lanczos4 at 78 MB. That is about 41 bytes per page pixel, so the 4 megapixel limit below keeps an upscale near 165 MB, and each mode upscales one page at a time.
- **When it runs:** only for pages shown at least 1.5 times their own size, on either side, and of at most 4 megapixels (both pages together for a double page), which also leaves out long webtoon strips; both limits are in `image_processing/acnet_upscale.h`. Up to 2x, Lanczos takes the doubled page to its size whatever the scaling method, since Nearest would alias the lines ACNet just sharpened; past 2x the scaling method does. In single and double page mode the page shown goes first, once the pages around it have their plain scale; continuous scroll starts with the visible pages.

---

| Build | Master | Develop |
|---|---|---|
|Code Validation |[![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=master&jobName=CodeFormatValidation)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=master)|[![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=develop&jobName=CodeFormatValidation)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=develop)|
|Linux | [![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=master&jobName=Linux)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=master) | [![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=develop&jobName=Linux)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=develop)|
|Windows x86| [![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=master&jobName=Windows_x86)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=master)|[![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=develop&jobName=Windows_x86)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=develop)|
|Windows x64| [![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=master&jobName=Windows_x64)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=master)|[![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=develop&jobName=Windows_x64)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=develop)|
|MacOS | [![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=master&jobName=MacOS)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=master) | [![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=develop&jobName=MacOS)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=develop)|
|Publish Build||[![Build Status](https://dev.azure.com/luisangelsm/YACReader/_apis/build/status/YACReader.yacreader?branchName=develop&jobName=PublishDevBuilds)](https://dev.azure.com/luisangelsm/YACReader/_build/latest?definitionId=1&branchName=develop)|

# YACReader

"Yet another comic reader"

## License
This software has been developed by Luis Ángel San Martín Rodríguez
(luisangelsm@gmail.com) under GPL v3 license
(see [COPYING.txt](./COPYING.txt)).

## Getting YACReader

### Official releases:
[https://github.com/YACReader/yacreader/releases](https://github.com/YACReader/yacreader/releases)

[<img src="https://flathub.org/assets/badges/flathub-badge-en.png" width="100"/>](https://flathub.org/apps/details/com.yacreader.YACReader)

### As a package:

[![Packaging status](https://repology.org/badge/vertical-allrepos/yacreader.svg)](https://repology.org/metapackage/yacreader)

### From OBS build service:

### Building from source:

See [INSTALL.md](./INSTALL.md)

## Contact
- Project home page : www.yacreader.com
- e-mail:
   - info@yacreader.com
   - support@yacreader.com
- Social:
   - Facebook  - http://www.facebook.com/YACReader
   - Twitter   - https://twitter.com/yacreader
   - YouTube   - https://www.youtube.com/user/yacreader
   - Instagram - https://www.instagram.com/yacreader/

If you need help or have any suggestion, please, send me an e-mail.

## Contributing
If you are interested in contributing to the project *the first step should be to contact me* so we can plan together the best approach, you can send an e-mail, joing the [Discord](https://discord.com/invite/dA5xABa85j) server or just open an issue in this repo. For tiny bug fixes it is usually ok to open a PR directly.

Contributions are not restricted to coding; you can help the project by bringing new UI/UX ideas, designing new assets, writing manuals or tutorials, translating the apps, etc. If you are interested in DevOps, YACReader uses Azure Pipelines for CI/CD, any improvements in that area are welcome. Testing pre-releases is also really appreciated.

### AI Usage
If you can't do it yourself please don't open PRs based on ideas developed using AI. If you are a software engineer it's ok to use AI as long as you know what you are doing, understanding and reviewing agent generated code before opening a PR is a must. Code that just works won't be enough to ensure I'll accept your contribution.

#### Dev Setup
YACReader is developed in *C++/Qt* and built with *CMake*. You need a *C++20* compiler and *Qt 6.9+*. In *Windows* I use *Visual Studio 2022* and in *macOS* I use Xcode, but I do all the coding using *QtCreator*.

The repo includes binaries for the dependencies needed for *Windows* (MSVC compiler) and *macOS* (clang). The *7zip* decompression backend source is downloaded automatically by CMake during configuration.

To build:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

See [INSTALL.md](./INSTALL.md) for detailed build options.

##### Running and debugging
YACReader needs to find its dependencies at runtime, make sure that *Qt* binaries are in your *PATH* and the third-party binaries are next to the executable. The best way to make sure you have all the third-party binaries in place is to check YACReader installation and copy the binaries in your output folder.

#### Code Format
YACReader uses `clang-format` to ensure a common style and avoid deviances from it. CI checks this and will fail if the correct format is not used. `clang-format` needs to be called recursively in all the folders because some of them have their own `.clang-format` file, mainly to exclude changing the format in third-party libraries which are included in the source code. I recommend configuring your development tools to use `clang-format`, you can try to use it manually, but please, do it always before committing changes. I recommend using QtCreator configured properly, you can find a tutorial [here]( https://www.vikingsoftware.com/using-clang-format-with-qtcreator/).

#### CI/CD
Any PR will be validated through CI, and it will not be merged if CI fails.

#### Pull Requests
The base and target branch for any PR should always be `develop`.

## Donations
YACReader is free but it needs money to keep being alive, so please, if you like YACReader, visit the home page and make a donation.

## Hacktoberfest
If you are interested in YACReader, please contact me so we can discuss your next steps.

## Sponsors 
Free code signing on Windows provided by [SignPath.io](https://signpath.io/), certificate by [SignPath Foundation](https://signpath.org/)
