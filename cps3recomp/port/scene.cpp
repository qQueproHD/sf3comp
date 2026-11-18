// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); the files and scene.json are cps2recomp's
// (port/scene.cpp, whose writing is copied here as it is), the layers CPS-3's.
#include "scene.h"

#include "../runtime/game.h"
#include "../runtime/system.h"
#include "../video/raster.h"
#include "image_files.h"
#include "port.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace port {

using cps3recomp::Machine;
using cps3recomp::Video;
namespace image_files = cps3recomp::image_files;
namespace native = cps3recomp::native;

namespace {
    constexpr int kMostWaiting = 120; // (a clip's frames waiting to be written: past that it is cut short)

    std::filesystem::path PathOf(const std::string &text) {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }
    // A folder of its own under `under` ("name", or "name 2" and on), made; "" if it could not be
    std::string NewFolder(const std::string &under, const std::string &name) {
        std::error_code error;
        std::string folder = under + name;
        for (int n = 2; std::filesystem::exists(PathOf(folder), error); n++) {
            folder = under + name + " " + std::to_string(n);
        }
        std::filesystem::create_directories(PathOf(folder), error);
        return error ? "" : folder + "/";
    }
    std::string Hex(uint32_t colour) {
        char text[8];
        snprintf(text, sizeof(text), "#%02x%02x%02x", colour & 0xFF, (colour >> 8) & 0xFF, (colour >> 16) & 0xFF);
        return text;
    }
    std::string Quoted(const std::string &text) {
        std::string out = "\"";
        for (const char c : text) {
            if (c == '"' || c == '\\') {
                out += '\\';
            }
            out += c;
        }
        return out + "\"";
    }

    std::string BoxesJson(const SceneExport::Scene &scene) {
        static const char *const kKinds[] = {"", "hurt", "attack", "push", "throw", "throw-range", "body", "shot", "ship"};
        std::string json;
        for (const SceneExport::Scene::Box &box : scene.boxes) {
            json += std::string(json.empty() ? "" : ", ") + "{\"player\": " + (box.player < 0 ? "null" : std::to_string(box.player + 1)) +
                    ", \"kind\": \"" + (box.kind < std::size(kKinds) ? kKinds[box.kind] : "") + "\"" + (box.thrown ? ", \"thrown\": true" : "") +
                    ", \"x\": " + std::to_string(box.x) + ", \"y\": " + std::to_string(box.y) + ", \"width\": " + std::to_string(box.width) +
                    ", \"height\": " + std::to_string(box.height) + ", \"colour\": " + Quoted(Hex(box.colour)) +
                    ", \"fill\": " + std::to_string(box.fill) + "}";
        }
        return "[" + json + "]";
    }

    image_files::Picture AsPicture(const std::vector<uint32_t> &pixels, int width, int height) {
        image_files::Picture picture;
        picture.width = width;
        picture.height = height;
        picture.pixels = pixels;
        return picture;
    }

    // The entries' list for scene.json (issue #397), each with where it is in entries.png (`atX`; none: a clip's frame)
    std::string EntriesJson(const std::vector<SceneExport::Scene::Entry> &entries, const std::vector<int> &atX, const std::vector<int> &atY,
                            const std::string &indent) {
        std::string json;
        for (size_t n = 0; n < entries.size(); n++) {
            const auto &e = entries[n];
            char tile[16], attributes[8];
            snprintf(tile, sizeof(tile), "0x%05X", e.tile);
            snprintf(attributes, sizeof(attributes), "0x%04X", e.attributes);
            json += indent + "{\"board\": " + Quoted(e.board) + ", \"entry\": " + std::to_string(e.entry) + ", \"tile\": " + Quoted(tile) +
                    ", \"colour\": " + std::to_string(e.colour) + ", \"level\": " + std::to_string(e.level) +
                    ", \"attributes\": " + Quoted(attributes) + ", \"box\": [" + std::to_string(e.x) + ", " + std::to_string(e.y) +
                    ", " + std::to_string(e.w) + ", " + std::to_string(e.h) + "]" +
                    (n < atX.size() ? ", \"atlas\": [" + std::to_string(atX[n]) + ", " + std::to_string(atY[n]) + "]" : "") +
                    ", \"layer\": " + Quoted(e.layer) + ", \"part\": " + (e.part.empty() ? std::string("null") : Quoted(e.part)) +
                    ", \"what\": " + (e.what.empty() ? std::string("null") : Quoted(e.what)) + "}" +
                    (n + 1 < entries.size() ? ",\n" : "\n");
        }
        return json;
    }
    // The map of the entry each object pixel is of, as a picture: 1 + its place in the list, red the low byte, green the
    // high; nothing where no object is
    image_files::Picture EntryMapPicture(const SceneExport::Scene &scene) {
        image_files::Picture map = AsPicture({}, scene.width, scene.height);
        map.pixels.resize(scene.entryMap.size());
        for (size_t n = 0; n < map.pixels.size(); n++) {
            map.pixels[n] = scene.entryMap[n] ? (scene.entryMap[n] & 0xFFFFu) | 0xFF000000u : 0;
        }
        return map;
    }
    // A still's objects each by itself (issue #397): entries.png, every entry's picture side by side (a shelf at a time,
    // a pixel between them), and entry-map.png; and their list for scene.json
    bool WriteEntries(const SceneExport::Scene &scene, const std::string &folder, std::string &json) {
        const auto &entries = scene.entries;
        int atlasWidth = 1024;
        for (const auto &e : entries) {
            atlasWidth = std::max(atlasWidth, e.w);
        }
        // (the tallest first, so that the shelves waste little)
        std::vector<size_t> byHeight(entries.size());
        for (size_t n = 0; n < entries.size(); n++) {
            byHeight[n] = n;
        }
        std::stable_sort(byHeight.begin(), byHeight.end(), [&](size_t a, size_t b) { return entries[a].h > entries[b].h; });
        std::vector<int> atX(entries.size()), atY(entries.size());
        int x = 0, y = 0, shelf = 0;
        for (const size_t n : byHeight) {
            if (x + entries[n].w > atlasWidth) {
                x = 0;
                y += shelf + 1;
                shelf = 0;
            }
            atX[n] = x;
            atY[n] = y;
            x += entries[n].w + 1;
            shelf = std::max(shelf, entries[n].h);
        }
        image_files::Picture atlas = AsPicture({}, atlasWidth, y + shelf);
        atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height, 0);
        for (size_t n = 0; n < entries.size(); n++) {
            const auto &e = entries[n];
            for (int row = 0; row < e.h; row++) {
                std::copy_n(&e.pixels[static_cast<size_t>(row) * e.w], e.w, &atlas.pixels[static_cast<size_t>(atY[n] + row) * atlas.width + atX[n]]);
            }
        }
        if (!image_files::WritePng(folder + "entries.png", atlas) || !image_files::WritePng(folder + "entry-map.png", EntryMapPicture(scene))) {
            return false;
        }
        json = ",\n  \"entryAtlas\": \"entries.png\",\n  \"entryMap\": \"entry-map.png\",\n  \"entries\": [\n" +
               EntriesJson(entries, atX, atY, "    ") + "  ]";
        return true;
    }

    // A scene's files: `picture` and its layers' (the empty ones not written), named `file` (a still's: "<layer>.png";
    // a clip's "<layer>/0001.png"; a clip's frame's objects each by itself too: entry-map/0001.png, entries/0001.json)
    bool WriteScene(const SceneExport::Scene &scene, const std::string &folder, int number) {
        auto path = [&](const std::string &name) {
            if (number <= 0) {
                return folder + name + ".png";
            }
            char file[16];
            snprintf(file, sizeof(file), "%04d.png", number);
            std::error_code error;
            std::filesystem::create_directories(PathOf(folder + name), error);
            return folder + name + "/" + file;
        };
        bool written = image_files::WritePng(path("picture"), AsPicture(scene.picture, scene.width, scene.height));
        for (const auto *list : {&scene.layers, &scene.parts, &scene.overlays}) {
            for (const SceneExport::Layer &layer : *list) {
                if (!layer.empty) {
                    written = image_files::WritePng(path(layer.name), AsPicture(layer.pixels, scene.width, scene.height)) && written;
                }
            }
        }
        if (number > 0 && !scene.entries.empty()) {
            written = image_files::WritePng(path("entry-map"), EntryMapPicture(scene)) && written;
            const std::string json = "{\"entries\": [\n" + EntriesJson(scene.entries, {}, {}, "  ") + "]}\n";
            std::string file = path("entries");
            file.replace(file.size() - 4, 4, ".json");
            written = image_files::WriteAll(file, std::vector<uint8_t>(json.begin(), json.end())) && written;
        }
        return written;
    }


    std::string LayerJson(const SceneExport::Layer &layer, bool still) {
        if (layer.board == "part" || layer.board == "overlay") { // (a part of the picture by what is in it, or what is drawn
                                                                 // over it: its name, and its file or folder)
            return "{\"name\": " + Quoted(layer.name) + ", " +
                   (still ? "\"file\": " + (layer.empty ? std::string("null") : Quoted(layer.name + ".png")) : "\"folder\": " + Quoted(layer.name)) +
                   "}";
        }
        std::string json = "{\"name\": " + Quoted(layer.name) + ", \"board\": " + Quoted(layer.board) + ", \"kind\": " +
                           (layer.scroll ? "\"scroll\"" : "\"objects\"") + ", \"tile\": " + std::to_string(layer.tile);
        if (still) {
            json += ", \"file\": " + (layer.empty ? std::string("null") : Quoted(layer.name + ".png"));
        } else {
            json += ", \"folder\": " + Quoted(layer.name);
        }
        return json + "}";
    }

    // (the board's frames a second, for a clip's scene.json)
    std::string Fps() {
        char text[32];
        snprintf(text, sizeof(text), "%.6f", Machine::kFramesPerSecond);
        return text;
    }

    // The picture as one layer, not taken apart (Scene::whole)
    void Whole(SceneExport::Scene &scene) {
        scene.layers.clear();
        SceneExport::Layer layer;
        layer.name = "whole";
        layer.board = "game";
        layer.pixels = scene.picture;
        layer.empty = false;
        scene.layers.push_back(std::move(layer));
        scene.whole = true;
    }
} // namespace

bool SceneExport::Take(Machine &machine, Scene &scene, int entries) {
    (void)entries; // (each object by itself: not yet)
    const int width = machine.PictureWidth(), height = Video::kHeight;
    if (width <= 0) {
        return false;
    }
    scene.width = width;
    scene.height = height;
    scene.frame = machine.frame;
    const Machine &shown = machine;
    scene.picture.assign(shown.Picture(), shown.Picture() + static_cast<size_t>(width) * height);
    for (uint32_t &pixel : scene.picture) {
        pixel = (pixel & 0xFFFFFFu) | 0xFF000000u;
    }
    scene.layers.clear();
    scene.parts.clear();
    scene.entries.clear();
    scene.entryMap.clear();
    scene.overlays.clear();
    scene.boxes.clear();
    scene.whole = false;
    scene.backdrop = 0;
    Machine::ShownParts parts;
    if (!machine.ShownFrame(parts) || parts.frame->width != width || parts.frame->height != height ||
        parts.frame->tileFrom.size() != parts.frame->tiles.size()) {
        Whole(scene); // (not drawn as one frame: kept whole)
        return true;
    }
    const native::Frame &f = *parts.frame;
    // The layers: each producer's runs of tiles from one tilemap, or from objects, in the order the picture is made of
    // them (the guest's buffer, the game's, the guest's in front, the game's in front: Rasteriser::Split)
    std::vector<int> layerOf(f.tiles.size(), -1);
    struct Run {
        int producer = 0;
        uint8_t from = 0;
    };
    std::vector<Run> runs;
    for (const int producer : {1, 0, 2, 3}) {
        int last = -1;
        for (size_t i = 0; i < f.tiles.size(); i++) {
            if ((f.tiles[i].producer & 3) != producer) {
                continue;
            }
            if (last < 0 || runs[static_cast<size_t>(last)].from != f.tileFrom[i]) {
                runs.push_back({producer, f.tileFrom[i]});
                last = static_cast<int>(runs.size()) - 1;
            }
            layerOf[i] = last;
        }
    }
    native::Rasteriser raster;
    raster.text = parts.raster->text;
    raster.backdrop = parts.raster->backdrop;
    std::vector<std::vector<uint32_t>> images;
    std::vector<uint32_t> text;
    raster.Split(f, *parts.video, parts.ss, layerOf, static_cast<int>(runs.size()), images, text, scene.backdrop);
    // (named by what each is; objects by how many of their board's tilemap runs are behind them; a name twice gets a number)
    int behind[2] = {0, 0};
    for (size_t n = 0; n < runs.size(); n++) {
        const Run &run = runs[n];
        const bool stage = run.producer == 1 || run.producer == 2;
        Layer layer;
        layer.board = stage ? "stage" : "game";
        const std::string prefix = stage ? "stage-" : "";
        if (run.from == native::Frame::kFromObject) {
            layer.name = prefix + "objects-" + std::to_string(behind[stage]);
        } else {
            layer.scroll = true;
            layer.name = prefix + "tilemap" + std::to_string((run.from & 3) + 1) + (run.from & native::Frame::kFromFill ? "-ends" : "");
            behind[stage]++;
        }
        for (int again = 2; std::any_of(scene.layers.begin(), scene.layers.end(), [&](const Layer &l) { return l.name == layer.name; }); again++) {
            const std::string base = layer.name.substr(0, layer.name.rfind('#') == std::string::npos ? layer.name.size() : layer.name.rfind('#'));
            layer.name = base + "#" + std::to_string(again);
        }
        layer.pixels = std::move(images[n]);
        layer.empty = std::none_of(layer.pixels.begin(), layer.pixels.end(), [](uint32_t p) { return p >> 24; });
        scene.layers.push_back(std::move(layer));
    }
    if (!text.empty()) {
        Layer layer;
        layer.name = "text";
        layer.board = "game";
        layer.scroll = true;
        layer.tile = 8;
        layer.pixels = std::move(text);
        layer.empty = std::none_of(layer.pixels.begin(), layer.pixels.end(), [](uint32_t p) { return p >> 24; });
        scene.layers.push_back(std::move(layer));
    }
    // The layers laid over each other on the backdrop, against the picture: the same, or the picture kept whole
    std::vector<uint32_t> made(scene.picture.size(), scene.backdrop | 0xFF000000u);
    for (const Layer &layer : scene.layers) {
        for (size_t i = 0; i < made.size() && !layer.empty; i++) {
            if (layer.pixels[i] >> 24) {
                made[i] = layer.pixels[i];
            }
        }
    }
    size_t different = 0, first = 0;
    for (size_t i = 0; i < made.size(); i++) {
        if (made[i] != scene.picture[i] && different++ == 0) {
            first = i;
        }
    }
    if (different) {
        fprintf(stderr, "scene: frame %llu: its layers make %zu pixels of the picture differently (the first at %zu,%zu: %06X, not %06X); kept whole\n",
                static_cast<unsigned long long>(scene.frame), different, first % static_cast<size_t>(width), first / static_cast<size_t>(width),
                made[first] & 0xFFFFFFu, scene.picture[first] & 0xFFFFFFu);
        Whole(scene);
    }
    return true;
}

SceneExport::~SceneExport() {
    {
        std::lock_guard<std::mutex> lock(m_lock);
        m_stopping = true;
    }
    m_wake.notify_all();
    for (std::thread &writer : m_writers) {
        writer.join();
    }
}

std::string SceneExport::Save(Machine &machine, const std::string &under, const std::string &name) {
    Scene scene;
    if (!Take(machine, scene, kEntryPictures)) {
        return "NO SCENE TO SAVE YET";
    }
    const std::string folder = NewFolder(under, name);
    if (folder.empty() || !WriteScene(scene, folder, 0)) {
        return "SCENE NOT SAVED";
    }
    // (the Game page's SVG pictures too, issue #368: scene.svg, the board's layers back to front and what is drawn over
    // them, each a group; and parts.svg, its parts, in a fighting game's round)
    if (cps3recomp::FeatureOn("export_svg")) {
        double width = 0, height = 0;
        SvgSize(scene.width, scene.height, width, height);
        std::vector<image_files::Picture> pictures;
        auto svg = [&](const std::string &file, const std::vector<const std::vector<Layer> *> &lists) {
            pictures.clear();
            pictures.reserve(32);
            std::vector<image_files::SvgLayer> layers;
            image_files::Picture backdrop = AsPicture(std::vector<uint32_t>(static_cast<size_t>(scene.width) * scene.height, scene.backdrop | 0xFF000000u),
                                                      scene.width, scene.height);
            layers.push_back({"backdrop", &backdrop});
            for (const auto *list : lists) {
                for (const Layer &layer : *list) {
                    if (!layer.empty) {
                        pictures.push_back(AsPicture(layer.pixels, scene.width, scene.height));
                    }
                }
            }
            size_t n = 0;
            for (const auto *list : lists) {
                for (const Layer &layer : *list) {
                    if (!layer.empty) {
                        layers.push_back({layer.name, &pictures[n++]});
                    }
                }
            }
            return image_files::WriteSvg(folder + file, layers, scene.width, scene.height, width, height);
        };
        if (!svg("scene.svg", {&scene.layers, &scene.overlays}) || (!scene.parts.empty() && !svg("parts.svg", {&scene.parts, &scene.overlays}))) {
            return "SCENE NOT SAVED";
        }
    }
    std::string entriesJson;
    if (!scene.entries.empty() && !WriteEntries(scene, folder, entriesJson)) {
        return "SCENE NOT SAVED";
    }
    std::string json = "{\n  \"kind\": \"scene\",\n  \"frame\": " + std::to_string(scene.frame) + ",\n  \"width\": " +
                       std::to_string(scene.width) + ",\n  \"height\": " + std::to_string(scene.height) + ",\n  \"backdrop\": " +
                       Quoted(Hex(scene.backdrop)) + ",\n  \"picture\": \"picture.png\",\n  \"layers\": [\n";
    for (size_t n = 0; n < scene.layers.size(); n++) {
        json += "    " + LayerJson(scene.layers[n], true) + (n + 1 < scene.layers.size() ? ",\n" : "\n");
    }
    json += "  ]";
    if (!scene.parts.empty()) {
        json += ",\n  \"parts\": [\n";
        for (size_t n = 0; n < scene.parts.size(); n++) {
            json += "    " + LayerJson(scene.parts[n], true) + (n + 1 < scene.parts.size() ? ",\n" : "\n");
        }
        json += "  ]";
    }
    json += entriesJson;
    if (!scene.overlays.empty()) {
        json += ",\n  \"overlays\": [\n";
        for (size_t n = 0; n < scene.overlays.size(); n++) {
            json += "    " + LayerJson(scene.overlays[n], true) + (n + 1 < scene.overlays.size() ? ",\n" : "\n");
        }
        json += "  ],\n  \"boxes\": " + BoxesJson(scene);
    }
    json += "\n}\n";
    if (!image_files::WriteAll(folder + "scene.json", std::vector<uint8_t>(json.begin(), json.end()))) {
        return "SCENE NOT SAVED";
    }
    fprintf(stderr, "scene: %s\n", folder.c_str());
    const size_t at = folder.find("exports/");
    return "SCENE SAVED  " + (at == std::string::npos ? folder : folder.substr(at));
}

std::string SceneExport::StartClip(const std::string &under, const std::string &name, int frames) {
    {
        std::lock_guard<std::mutex> lock(m_lock);
        if (m_clip.on || m_finishing || !m_jobs.empty() || m_writing) {
            return "A CLIP IS STILL BEING SAVED";
        }
        m_failed = false;
    }
    m_clip.folder = NewFolder(under, name);
    if (m_clip.folder.empty()) {
        return "CLIP NOT SAVED";
    }
    m_clip.wanted = frames;
    m_clip.taken = 0;
    m_clip.missed = 0;
    m_clip.names.clear();
    m_clip.json.clear();
    m_clip.partNames.clear();
    m_clip.partsJson.clear();
    m_clip.entries = false;
    m_clip.overlayNames.clear();
    m_clip.overlaysJson.clear();
    m_clip.on = true;
    if (m_writers.empty()) {
        const unsigned threads = std::clamp(std::thread::hardware_concurrency() / 2, 1u, 4u);
        for (unsigned n = 0; n < threads; n++) {
            m_writers.emplace_back([this] { Writer(); });
        }
    }
    return "RECORDING SCENE CLIP";
}

void SceneExport::Frame(Machine &machine) {
    if (!m_clip.on) {
        return;
    }
    Job job;
    if (!Take(machine, job.scene, kEntryMap)) {
        // (no records this frame: a frame of the clip fewer; none for as long as the clip was to be, and it ends)
        if (++m_clip.missed >= m_clip.wanted) {
            StopClip();
        }
        return;
    }
    const Scene &scene = job.scene;
    std::string order, empty;
    for (const Layer &layer : scene.layers) {
        if (std::find(m_clip.names.begin(), m_clip.names.end(), layer.name) == m_clip.names.end()) {
            m_clip.names.push_back(layer.name);
            m_clip.json += std::string(m_clip.json.empty() ? "" : "\x1F") + LayerJson(layer, false);
        }
        (layer.empty ? empty : order) += std::string((layer.empty ? empty : order).empty() ? "" : ", ") + Quoted(layer.name);
    }
    std::string parts; // (the parts by what is in the picture, issue #367: those this frame has)
    for (const Layer &part : scene.parts) {
        if (std::find(m_clip.partNames.begin(), m_clip.partNames.end(), part.name) == m_clip.partNames.end()) {
            m_clip.partNames.push_back(part.name);
            m_clip.partsJson += std::string(m_clip.partsJson.empty() ? "" : "\x1F") + LayerJson(part, false);
        }
        if (!part.empty) {
            parts += std::string(parts.empty() ? "" : ", ") + Quoted(part.name);
        }
    }
    std::string overlays; // (what is drawn over the picture, issue #398: those this frame has)
    for (const Layer &overlay : scene.overlays) {
        if (std::find(m_clip.overlayNames.begin(), m_clip.overlayNames.end(), overlay.name) == m_clip.overlayNames.end()) {
            m_clip.overlayNames.push_back(overlay.name);
            m_clip.overlaysJson += std::string(m_clip.overlaysJson.empty() ? "" : "\x1F") + LayerJson(overlay, false);
        }
        if (!overlay.empty) {
            overlays += std::string(overlays.empty() ? "" : ", ") + Quoted(overlay.name);
        }
    }
    m_clip.taken++;
    job.folder = m_clip.folder;
    job.number = m_clip.taken;
    m_frameJson += std::string(m_frameJson.empty() ? "" : ",\n") + "    {\"number\": " + std::to_string(job.number) +
                   ", \"frame\": " + std::to_string(scene.frame) + ", \"backdrop\": " + Quoted(Hex(scene.backdrop)) +
                   ", \"layers\": [" + order + "]" + (m_clip.partNames.empty() ? "" : ", \"parts\": [" + parts + "]") +
                   (scene.overlays.empty() ? "" : ", \"overlays\": [" + overlays + "], \"boxes\": " + BoxesJson(scene)) +
                   ", \"entries\": " + std::to_string(scene.entries.size()) + "}";
    m_clip.entries = m_clip.entries || !scene.entries.empty();
    m_width = scene.width;
    m_height = scene.height;
    bool full;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        if (m_waiting) {
            m_taken.wait(lock, [this] { return static_cast<int>(m_jobs.size()) < kMostWaiting - 1; });
        }
        m_jobs.push_back(std::move(job));
        full = static_cast<int>(m_jobs.size()) >= kMostWaiting;
    }
    m_wake.notify_one();
    if (m_clip.taken >= m_clip.wanted || full) {
        StopClip(full && m_clip.taken < m_clip.wanted);
    }
}

void SceneExport::StopClip(bool cutShort) {
    if (!m_clip.on) {
        return;
    }
    m_clip.on = false;
    // scene.json, written when the last frame is
    auto list = [](const std::string &joined) {
        std::string out;
        for (size_t at = 0; at <= joined.size();) {
            const size_t end = std::min(joined.find('\x1F', at), joined.size());
            out += "    " + joined.substr(at, end - at) + (end < joined.size() ? ",\n" : "\n");
            at = end + 1;
        }
        return out;
    };
    const std::string layers = list(m_clip.json);
    const std::string parts = (m_clip.partsJson.empty() ? "" : ",\n  \"parts\": [\n" + list(m_clip.partsJson) + "  ]") +
                              (m_clip.overlaysJson.empty() ? "" : ",\n  \"overlays\": [\n" + list(m_clip.overlaysJson) + "  ]");
    std::string json = "{\n  \"kind\": \"clip\",\n  \"width\": " + std::to_string(m_width) + ",\n  \"height\": " +
                       std::to_string(m_height) + ",\n  \"fps\": " + Fps() + ",\n  \"frames\": " + std::to_string(m_clip.taken) +
                       ",\n  \"picture\": \"picture\",\n  \"layers\": [\n" + (m_clip.taken ? layers : "") +
                       "  ]" + parts + (m_clip.entries ? ",\n  \"entryMap\": \"entry-map\",\n  \"entries\": \"entries\"" : "") +
                       ",\n  \"frameList\": [\n" + m_frameJson + "\n  ]\n}\n";
    m_frameJson.clear();
    std::lock_guard<std::mutex> lock(m_lock);
    m_finalJson = std::move(json);
    m_finalFolder = m_clip.folder;
    m_finalFrames = m_clip.taken;
    m_cutShort = cutShort;
    m_finishing = true;
    if (m_jobs.empty() && m_writing == 0) {
        Finish();
    }
}

// (with the lock held: the clip's frames all written)
void SceneExport::Finish() {
    m_finishing = false;
    const bool written = image_files::WriteAll(m_finalFolder + "scene.json", std::vector<uint8_t>(m_finalJson.begin(), m_finalJson.end()));
    std::string folder = m_finalFolder;
    const size_t at = folder.find("exports/");
    if (at != std::string::npos) {
        folder = folder.substr(at);
    }
    if (m_failed || !written) {
        m_said = "CLIP NOT SAVED";
    } else if (m_finalFrames == 0) {
        m_said = "NO SCENE TO SAVE YET";
    } else {
        m_said = std::string(m_cutShort ? "CLIP CUT SHORT  " : "CLIP SAVED  ") + folder;
    }
    m_saying = true;
    fprintf(stderr, "scene clip: %s (%d frames)\n", m_finalFolder.c_str(), m_finalFrames);
}

void SceneExport::Writer() {
    for (;;) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            m_wake.wait(lock, [this] { return m_stopping || !m_jobs.empty(); });
            if (m_jobs.empty()) {
                return;
            }
            job = std::move(m_jobs.front());
            m_jobs.pop_front();
            m_writing++;
        }
        m_taken.notify_all();
        const bool written = WriteScene(job.scene, job.folder, job.number);
        std::lock_guard<std::mutex> lock(m_lock);
        m_writing--;
        m_failed = m_failed || !written;
        if (m_finishing && m_jobs.empty() && m_writing == 0) {
            Finish();
        }
    }
}

// --- the whole stage (StageStitch) ---

bool SceneExport::TakeSaid(std::string &said) {
    std::lock_guard<std::mutex> lock(m_lock);
    if (!m_saying) {
        return false;
    }
    m_saying = false;
    said = m_said;
    return true;
}

} // namespace port
