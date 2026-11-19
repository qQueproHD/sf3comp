// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); its shape and its files are cps2recomp's port/scene.h.
//
// The scene export (cps2recomp's issue #366): the picture shown, and each of its layers by itself as a PNG with nothing
// where the layer has nothing, so that the picture can be taken apart and put together again; and a scene.json that says
// what each file is and the layers' order, back to front: the same files and the same scene.json as cps2recomp's, so that
// its diorama (tools/diorama/diorama.html) shows them as they are.
//
// A CPS-3 has no fixed layers: its picture is the frame's object list drawn in its order, four tilemaps among the objects,
// and the text layer over it all. So a layer here is each run of the list that is one tilemap's lines ("tilemap1" to
// "tilemap4") or objects ("objects-0" on: how many tilemap runs are behind them), in the order they are drawn, and the
// text layer ("text"); a fight on New Generation's or 2nd Impact's stage has that stage's runs too ("stage-..."), behind
// the game's (and the stage's pieces in front of the fighters in front of them). Each layer is the frame drawn again from
// its records (native::Rasteriser::Split, the frame the picture was drawn from: Machine::ShownFrame): a shadow or a
// highlight (a blend, which darkens or lightens what is under it) is in the layer it falls on, so that the layers laid
// over each other on the backdrop make the picture again, pixel for pixel. That is checked as each scene is taken: a
// picture the layers do not make again (or one made of more than one picture: a stage fading or sliding in, the ring
// out's way down) is saved whole, as the one layer "whole".
//
// In a round, the picture by what is in it too (the stage behind the fighters, each player, the stage in front, the panel,
// the rest), each object by itself, and in training what the port draws over it (cps2recomp's parts, entries, overlays).
//
// A clip is the same for each frame of a few seconds, in a folder per layer, written by threads beside the game.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace cps3recomp {
class Machine;
}

namespace port {

class SceneExport {
public:
    ~SceneExport();
    // The last frame's scene, in a folder of its own under `under` named `name`; what to say of it
    std::string Save(cps3recomp::Machine &machine, const std::string &under, const std::string &name);
    // A clip of the next `frames` frames, in a folder of its own; what to say of it (it is begun, or why not)
    std::string StartClip(const std::string &under, const std::string &name, int frames);
    // Ended before its length, the frames so far kept
    void StopClip(bool cutShort = false);
    bool Recording() const {
        return m_clip.on;
    }
    // Each frame the game ran and will show (the clip's frame taken)
    void Frame(cps3recomp::Machine &machine);
    // What is to be said now (a clip finished writing, or one could not be): true once for each
    bool TakeSaid(std::string &said);
    // Not playing in real time (headless): a clip waits for its writers rather than being cut short when they fall behind
    void SetWaiting(bool waiting) {
        m_waiting = waiting;
    }

    // A layer of a scene, its pixels red in the low byte, then green, blue, and 0xFF in the top byte where it has
    // something (0: nothing)
    struct Layer {
        std::string name;  // ("tilemap1".."tilemap4", "tilemap1-ends" (widescreen's filled ends), "objects-0".., "text";
                           // "stage-" before them for another game's stage; "whole": the picture, not taken apart)
        std::string board; // ("game", or "stage")
        bool scroll = false; // (a tilemap's or the text layer's: scene.json's kind "scroll", else "objects")
        int tile = 16;       // (its tiles' size: 16, the text layer's 8)
        std::vector<uint32_t> pixels;
        bool empty = true;
    };
    struct Scene {
        int width = 0, height = 0;
        uint64_t frame = 0;
        uint32_t backdrop = 0;
        std::vector<uint32_t> picture; // (as shown, solid)
        std::vector<Layer> layers;     // (back to front)
        // (a fight's parts by what is in the picture, Game::scenePart: part-stage-behind, part-other, the players,
        // part-stage-front, part-panel; each object by itself, a head of the board's list; and what the port draws over
        // the picture, its hit boxes and its readout, the boxes listed with their players: cps2recomp's, the same files)
        std::vector<Layer> parts;
        struct Entry {
            std::string board;
            int entry = 0;
            uint32_t tile = 0;
            int colour = 0, level = 0;
            uint16_t attributes = 0;
            int x = 0, y = 0, w = 0, h = 0;
            std::string layer, part, what;
            std::vector<uint32_t> pixels;
        };
        std::vector<Entry> entries;
        std::vector<uint32_t> entryMap;
        std::vector<Layer> overlays;
        struct Box {
            int player = -1;
            uint8_t kind = 0;
            bool thrown = false;
            int x = 0, y = 0, width = 0, height = 0;
            uint32_t colour = 0;
            uint8_t fill = 0;
        };
        std::vector<Box> boxes;
        bool whole = false; // (not taken apart: the one layer "whole")
    };
    // The last frame's scene (false: no picture yet)
    static constexpr int kNoEntries = 0, kEntryMap = 1, kEntryPictures = 2;
    static bool Take(cps3recomp::Machine &machine, Scene &scene, int entries = kNoEntries);

private:
    struct Clip {
        std::atomic<bool> on{false};
        std::string folder;
        int wanted = 0, taken = 0, missed = 0;
        std::vector<std::string> names; // (the first frame's layers, in its order: the folders)
        std::string json;               // (each frame's line of scene.json)
        std::vector<std::string> partNames;
        std::string partsJson;
        bool entries = false;
        std::vector<std::string> overlayNames;
        std::string overlaysJson;
    } m_clip;
    // The writers: frames waiting to be written as PNGs
    struct Job {
        std::string folder;
        int number = 0;
        Scene scene;
    };
    std::mutex m_lock;
    std::condition_variable m_wake;
    std::deque<Job> m_jobs;
    std::vector<std::thread> m_writers;
    int m_writing = 0; // (jobs taken and not yet written)
    bool m_failed = false, m_stopping = false, m_finishing = false, m_cutShort = false, m_waiting = false;
    std::condition_variable m_taken; // (a job taken by a writer: for a clip that waits)
    std::string m_frameJson;
    int m_width = 0, m_height = 0;
    std::string m_finalJson, m_finalFolder;
    int m_finalFrames = 0;
    std::string m_said;
    bool m_saying = false;
    void Writer();
    void Finish();
};

} // namespace port
