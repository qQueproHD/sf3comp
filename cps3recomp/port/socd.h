// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of sf3comp (the same as cps2recomp's port/socd.h).
//
// SOCD cleaning: what the game is shown when a player holds two opposite directions at once (simultaneous opposite
// cardinal directions), which a hit box or a keyboard can do and a stick cannot. The board passes both on, and a game
// does strange things with them. The choices are Fightcade's:
//
//   Off        both passed on, as the board does
//   Neutral    up + down is neither, left + right is neither
//   Hit box    up + down is up, left + right is neither
//   4-way      Neutral, and no diagonals: of a vertical and a horizontal held together, only the one pressed last
//
// Only the directions are changed. A player's cleaner keeps what that player held last time (for 4-way), so each
// player has their own; Clean is called once for every reading of the controls.
#pragma once

#include "../runtime/system.h"

#include <cstdint>

namespace port {

enum class Socd { Off, Neutral, HitBox, FourWay };

class SocdCleaner {
public:
    uint16_t Clean(Socd mode, uint16_t held) {
        using namespace cps3recomp;
        const uint16_t vertical = kUp | kDown, horizontal = kLeft | kRight;
        uint16_t directions = held & (vertical | horizontal);
        if (mode != Socd::Off) {
            if ((directions & vertical) == vertical) {
                directions &= static_cast<uint16_t>(mode == Socd::HitBox ? ~kDown : ~vertical);
            }
            if ((directions & horizontal) == horizontal) {
                directions &= static_cast<uint16_t>(~horizontal);
            }
        }
        if (mode == Socd::FourWay) {
            // (an axis is pressed when its direction differs from last time's; both at once, the one kept before)
            const uint16_t onAxis[2] = {static_cast<uint16_t>(directions & horizontal), static_cast<uint16_t>(directions & vertical)};
            const bool pressed[2] = {onAxis[0] && onAxis[0] != (was & horizontal), onAxis[1] && onAxis[1] != (was & vertical)};
            if (pressed[0] != pressed[1]) {
                lastVertical = pressed[1];
            } else if (!onAxis[0] != !onAxis[1]) {
                lastVertical = onAxis[1];
            }
            was = directions;
            if (onAxis[0] && onAxis[1]) {
                directions = onAxis[lastVertical];
            }
        } else {
            was = 0;
        }
        return static_cast<uint16_t>((held & ~(vertical | horizontal)) | directions);
    }

private:
    uint16_t was = 0;          // the directions held last time (opposites taken out), for 4-way
    bool lastVertical = false; // of the two axes, the one pressed last
};

} // namespace port
