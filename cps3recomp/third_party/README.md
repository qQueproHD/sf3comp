# third_party

| File | From | Licence |
| --- | --- | --- |
| `librashader.h` | [librashader](https://github.com/SnowflakePowered/librashader)'s C header (loaded at run time when present) | MPL-2.0 or GPL-3.0 |
| `stb_vorbis.c` | [stb](https://github.com/nothings/stb) `stb_vorbis` 1.22 (commit 2c980bb), by Sean Barrett | Public domain, or MIT |
| `dr_flac.h` | [dr_libs](https://github.com/mackron/dr_libs) `dr_flac` 0.13.4 (commit dfe8377), by David Reid | Public domain, or MIT No Attribution |
| `dr_mp3.h` | dr_libs `dr_mp3` 0.7.4 (the same commit), by David Reid, after minimp3 by lieff | Public domain, or MIT No Attribution |
| `dr_wav.h` | dr_libs `dr_wav` 0.14.6 (the same commit), by David Reid | Public domain, or MIT No Attribution |
| `minih264e.h` | [minih264](https://github.com/lieff/minih264) (commit 207dee3), by Lieff, with three changes marked in it (macOS byte order, an ARM64 intrinsic, a BT.709 colour description) | CC0 1.0 |
| `minimp4.h` | [minimp4](https://github.com/lieff/minimp4) (commit 65fab0a), by Lieff, unchanged | CC0 1.0 |

The four sound decoders are cps2recomp's copies, unchanged, for the custom soundtrack (port/decoders.cpp). The two video encoders are cps2recomp's copies too, for the video recording (port/video_record.cpp).
