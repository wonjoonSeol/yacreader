# YACReader third party source dependencies

This directory contains third party source dependencies
needed to build YACReader.

| Name      | Purpose  | Upstream | Version | License |
|---------- |--------- |--------- |-------- |-------- |
| QsLog     | Logging  | https://bitbucket.org/codeimproved/qslog | 2.1 46b643d5bcbc| 3-clause BSD|
| QtWebApp  | Server   | http://stefanfrings.de/qtwebapp/ | 1.9.1 | LGPL3 |
| QR-Code-generator | | https://github.com/nayuki/QR-Code-generator/tree/master/cpp | 1.8.0 | MIT|
| Anime4KCPP, trimmed (`anime4kcpp`) | ACNet upscaling in the reader | https://github.com/TianZerL/Anime4KCPP | fe187e0, changes in `anime4kcpp/yacreader.patch` | MIT |
| stb_image_resize2 (`anime4kcpp/deps/stb`) | Resizing inside Anime4KCPP | https://github.com/nothings/stb_image_resize | 2.18 (stb 2b2e64b) | MIT or public domain |
| ruapu (`anime4kcpp/deps/ruapu`) | CPU feature detection for Anime4KCPP | https://github.com/nihui/ruapu | 62e6850 | MIT |
| half (`anime4kcpp/deps/half`) | Half floats for Anime4KCPP | https://half.sourceforge.net | 2.2.1 | MIT |
