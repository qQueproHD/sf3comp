// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 cps2recomp contributors. Part of cps3recomp (sf3comp); from cps2recomp.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace port {

// Only confirmed frames enter this counter. Predictions (and their replacements during rollback) stay in
// Netplay::Kept until both players' controls are known. This state never changes the board or its replay.
class FirstTo {
public:
    struct Frame {
        int result = -1;
        int rounds = 1;
        bool versus = false;
    };
    // `choice`: online_first_to's (0 Casual, 1 FT3, 2 FT5, 3 FT10). `scored`: the games won are counted (a fighting
    // game's: Casual's too, for the score on the screen, which then never ends the match)
    void Begin(int choice, bool scored = true) {
        *this = FirstTo{};
        target = choice == 1 ? 3 : choice == 2 ? 5 : choice == 3 ? 10 : 0;
        counting = scored || target;
    }
    // The set from 0-0 again, as it is (its target, whether it counts): the players have swapped sides (Netplay: the host's Swap sides)
    void Restart() {
        const int keptTarget = target;
        const bool keptCounting = counting;
        *this = FirstTo{};
        target = keptTarget;
        counting = keptCounting;
    }
    int Target() const { return target; }
    bool Counting() const { return counting; }
    std::string Format() const { return target ? "FT" + std::to_string(target) : "Casual"; }
    int Score(int side) const { return score[side]; }
    int Winner() const { return winner; } // -1 until the set is complete

    // Returns the player awarded a game, or -1. The frame number also makes duplicate confirmations harmless.
    int Confirm(uint64_t frame, const Frame &now) {
        if (!counting || winner >= 0 || (haveFrame && frame <= lastFrame)) {
            return -1;
        }
        haveFrame = true;
        lastFrame = frame;
        if (!now.versus || now.result < 0 || now.result > 3 || now.rounds < 1) {
            quiet = 0;
            armed = false;
            // (the rounds won so far are let go only after a while out of versus play: between a game's rounds its
            // players' records are put back for a while: Super Street Fighter II Turbo's for six frames, Vampire Savior's for six
            // seconds)
            if (apart < kApart && ++apart == kApart) {
                rounds = {};
            }
            return -1;
        }
        apart = 0;
        if (now.result == 0) {
            // A persistent result, or a short flicker of it, must count only once. Also ignore a result already
            // on screen when the online session starts: first observe a live two-player round.
            if (quiet < 60) {
                ++quiet;
            }
            armed = quiet >= 60;
            return -1;
        }
        quiet = 0;
        if (!armed) {
            return -1;
        }
        armed = false;
        if (now.result == 3) {
            return -1;
        }
        const int side = now.result - 1;
        if (++rounds[side] < now.rounds) {
            return -1;
        }
        rounds = {};
        if (++score[side] == target && target) {
            winner = side;
        }
        return side;
    }

    // The whole counter as numbers, and back (cps2recomp's viewers start from the host's: kept as its copy has it)
    static constexpr int kApart = 1800; // frames out of versus play (30 seconds) that let a game's rounds won so far go
    static constexpr int kSaved = 12;
    std::array<uint32_t, kSaved> Saved() const {
        return {uint32_t(target), uint32_t(winner), uint32_t(score[0]), uint32_t(score[1]), uint32_t(rounds[0]),
                uint32_t(rounds[1]), uint32_t(quiet), armed, haveFrame, uint32_t(lastFrame), counting, uint32_t(apart)};
    }
    void Load(const std::array<uint32_t, kSaved> &v) {
        target = int(v[0]), winner = int(v[1]), score = {int(v[2]), int(v[3])}, rounds = {int(v[4]), int(v[5])};
        quiet = int(v[6]), armed = v[7] != 0, haveFrame = v[8] != 0, lastFrame = v[9], counting = v[10] != 0, apart = int(v[11]);
    }

private:
    int target = 0, winner = -1;
    bool counting = false;
    std::array<int, 2> score{}, rounds{};
    int quiet = 0, apart = 0;
    bool armed = false, haveFrame = false;
    uint64_t lastFrame = 0;
};

} // namespace port
