// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp).
//
// The Display page's Bezels (display_bezel, issue #8): what fills the window beside a picture narrower than it (the
// board's 4:3 in a 16:9 window or on an ultrawide screen). Black and white, patterns drawn here (no one's artwork),
// or the player's own picture. Made at the window's shape, so it fits any screen; the presenter draws it under the
// picture (present.h, SetBackdrop).
#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <vector>

namespace port {

// The choices, in the order of the feature's
enum class Bezel { Off, Black, White, Dusk, Carbon, NeonGrid, BrushedMetal, YourOwn };
inline constexpr const char *kBezelChoices[] = {"Off", "Black", "White", "Dusk", "Carbon", "Neon grid", "Brushed metal", "Your own"};

// The bezel for a `width` x `height` window with the picture at `picture` (in the window's pixels): its pixels (red in
// the low byte, opaque) in `out`, `outWidth` x `outHeight`, the window's shape though no larger than a 1080-line
// screen's (it is stretched to the window). `ownFolder`: where Your own's picture is (the first .png or .bmp there,
// by name). False: nothing to draw under the picture (Off, Black, or Your own with no picture found: black)
bool MakeBezel(Bezel bezel, int width, int height, const SDL_FRect &picture, const std::string &ownFolder, std::vector<uint32_t> &out,
               int &outWidth, int &outHeight);

} // namespace port
