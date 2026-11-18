// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
//
// FAAC's config.h (the video recording's AAC encoder, video_record.h), as its meson.build would write it for this
// build: two channels, and its plain C quantizer only (the SSE2 one left out, so it is the same everywhere)
#define PACKAGE "faac"
#define PACKAGE_VERSION "2.2"
#define FAAC_SBR_DECIMATION 1
#define MAX_CHANNELS 2
