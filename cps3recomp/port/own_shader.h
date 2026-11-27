// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp).
//
// The Display page's Shader (display_shader): its choice Your own, which draws through a .slangp preset of the player's
// (recomp-common/own_shaders.h). The game (port/main.cpp, after kShaderPresets: it checks this is its last choice) and
// the launcher (launcher/src/description.cpp: Your own preset and the shaders folder's rows under the Shader) both
// name it. A choice of the Shader's taken out or put in before it changes this number.
#pragma once

namespace port {

constexpr int kYourOwnShaderChoice = 11;

} // namespace port
