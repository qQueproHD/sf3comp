// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of sf3comp.
// The SOCD cleaner's choices (port/socd.h), held direction by held direction.
#include "../port/socd.h"

#include <cstdio>
#include <cstdlib>

using namespace cps3recomp;
using port::Socd;
using port::SocdCleaner;

namespace {
int failures = 0;
void Expect(Socd mode, SocdCleaner &cleaner, uint16_t held, uint16_t want, const char *what) {
    const uint16_t got = cleaner.Clean(mode, held);
    if (got != want) {
        fprintf(stderr, "socd: %s: held %04X gave %04X, wanted %04X\n", what, held, got, want);
        failures++;
    }
}
void Once(Socd mode, uint16_t held, uint16_t want, const char *what) {
    SocdCleaner cleaner;
    Expect(mode, cleaner, held, want, what);
}
} // namespace

int main() {
    const uint16_t ud = kUp | kDown, lr = kLeft | kRight, button = kPunch1 | kStart;

    Once(Socd::Off, ud | lr | button, ud | lr | button, "off passes everything");
    Once(Socd::Neutral, ud | button, button, "neutral: up+down");
    Once(Socd::Neutral, lr | kDown, kDown, "neutral: left+right");
    Once(Socd::Neutral, kUp | kLeft, kUp | kLeft, "neutral: a diagonal stays");
    Once(Socd::HitBox, ud | kRight, kUp | kRight, "hit box: up+down is up");
    Once(Socd::HitBox, lr | kUp, kUp, "hit box: left+right is neither");
    Once(Socd::HitBox, ud | lr, kUp, "hit box: all four is up");

    {
        SocdCleaner c; // 4-way: the axis pressed last wins
        Expect(Socd::FourWay, c, kRight, kRight, "4-way: right");
        Expect(Socd::FourWay, c, kRight | kDown, kDown, "4-way: then down wins");
        Expect(Socd::FourWay, c, kRight | kDown, kDown, "4-way: still down");
        Expect(Socd::FourWay, c, kDown, kDown, "4-way: right let go");
        Expect(Socd::FourWay, c, kDown | kLeft, kLeft, "4-way: then left wins");
        Expect(Socd::FourWay, c, kDown | kRight, kRight, "4-way: left to right, still across");
        Expect(Socd::FourWay, c, kUp | kRight, kUp, "4-way: down to up, vertical now");
        Expect(Socd::FourWay, c, kUp | lr | kPunch1, kUp | kPunch1, "4-way: left+right neutral, buttons kept");
        Expect(Socd::FourWay, c, 0, 0, "4-way: nothing");
        Expect(Socd::FourWay, c, kUp | kLeft, kUp, "4-way: both at once keep the last axis");
    }
    {
        SocdCleaner c; // a fresh cleaner with both at once: across
        Expect(Socd::FourWay, c, kDown | kLeft, kLeft, "4-way: fresh diagonal is across");
        Expect(Socd::Neutral, c, kDown | kLeft, kDown | kLeft, "switching off 4-way keeps diagonals");
    }

    if (failures) {
        return EXIT_FAILURE;
    }
    printf("socd: all choices clean as they should\n");
    return EXIT_SUCCESS;
}
