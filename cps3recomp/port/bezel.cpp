// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp).

#include "bezel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>

namespace port {

namespace {
    struct Colour {
        float r, g, b;
    };
    Colour Mix(Colour a, Colour b, float by) {
        return {a.r + (b.r - a.r) * by, a.g + (b.g - a.g) * by, a.b + (b.b - a.b) * by};
    }
    Colour Hex(uint32_t rgb) {
        return {static_cast<float>((rgb >> 16) & 0xFF), static_cast<float>((rgb >> 8) & 0xFF), static_cast<float>(rgb & 0xFF)};
    }
    uint32_t Pack(Colour c) {
        auto byte = [](float v) { return static_cast<uint32_t>(std::clamp(std::lround(v), 0L, 255L)); };
        return 0xFF000000u | byte(c.b) << 16 | byte(c.g) << 8 | byte(c.r);
    }
    // A number from 0 to 1 that looks random, the same for the same arguments
    float Noise(uint32_t x, uint32_t y) {
        uint32_t h = x * 0x8DA6B343u ^ y * 0xD8163841u;
        h ^= h >> 13;
        h *= 0x85EBCA6Bu;
        h ^= h >> 16;
        return static_cast<float>(h & 0xFFFF) / 65535.0f;
    }
    // How near a line (`at` its distance from one in pixels): 1 on it, fading to 0 a pixel and a half away
    float Line(float at, float thick) {
        return std::clamp(1.0f - (std::fabs(at) - thick * 0.5f) / 1.5f, 0.0f, 1.0f);
    }

    // The patterns: each pixel's colour from where it is, `x` and `y` in the bezel's pixels, `h` its height (sizes go
    // by the height, so the pattern looks the same however wide the screen)
    Colour Dusk(float x, float y, float w, float h) {
        const float down = y / h;
        Colour sky = down < 0.55f ? Mix(Hex(0x120B2E), Hex(0x5B2A6E), down / 0.55f) : Mix(Hex(0x5B2A6E), Hex(0xE07856), (down - 0.55f) / 0.45f);
        // (a soft glow low in the middle)
        const float dx = (x - w / 2) / h, dy = (y - h) / h;
        const float glow = std::exp(-(dx * dx + dy * dy) * 2.2f);
        return Mix(sky, Hex(0xFFC27A), glow * 0.45f);
    }
    Colour Carbon(float x, float y, float h) {
        // (a twill weave: diagonal bands of strands, each cell's strands across or down, shaded along them)
        const float cell = std::max(4.0f, h / 90.0f);
        const int cx = static_cast<int>(x / cell), cy = static_cast<int>(y / cell);
        const float fx = x / cell - cx, fy = y / cell - cy;
        const bool across = ((cx + cy) / 2) % 2 == 0;
        const float along = across ? fy : fx;
        const float shade = 0.55f + 0.45f * std::sin(along * 3.14159f);
        const float strands = 0.85f + 0.15f * std::sin((across ? fx : fy) * 3.14159f * 6);
        const float level = 18 + 26 * shade * strands;
        return {level, level, level * 1.06f};
    }
    Colour NeonGrid(float x, float y, float w, float h) {
        const float horizon = h * 0.58f;
        if (y < horizon) {
            const float up = y / horizon;
            Colour sky = Mix(Hex(0x07031A), Hex(0x3A0F55), up);
            const float glow = std::exp(-(horizon - y) / (h * 0.04f));
            return Mix(sky, Hex(0xFF4FD8), glow * 0.7f);
        }
        // (the ground: lines toward the middle of the horizon, and across, closer together further away)
        const float depth = (y - horizon) / (h - horizon); // 0 at the horizon, 1 at the foot
        const float z = 1.0f / std::max(depth, 0.02f);
        const float spacing = h * 0.09f;
        const float fromMiddle = (x - w / 2) / std::max(depth, 0.02f);
        const float acrossLine = std::fmod(std::fabs(fromMiddle) + spacing / 2, spacing) - spacing / 2;
        const float rows = z * 2.0f;
        const float rowAt = (rows - std::floor(rows + 0.5f)) * h * 0.25f * depth * depth;
        const float lines = std::max(Line(acrossLine * depth, 1.5f), Line(rowAt, 1.5f));
        Colour ground = Mix(Hex(0x12052A), Hex(0x05020F), depth);
        Colour neon = Mix(Hex(0xFF4FD8), Hex(0x4FE3FF), depth);
        const float fade = std::clamp(depth * 4.0f, 0.0f, 1.0f);
        return Mix(ground, neon, lines * fade);
    }
    Colour BrushedMetal(float x, float y, float h) {
        // (streaks along the rows, a little different each row and along them, with a sheen across the middle)
        const uint32_t row = static_cast<uint32_t>(y);
        const float streak = Noise(row, 7) * 0.6f + Noise(row / 3, 11) * 0.4f;
        const float along = Noise(static_cast<uint32_t>(x / 37) + row * 131, row) * 0.15f;
        const float sheen = 1.0f - std::fabs(y / h - 0.4f) * 0.9f;
        const float level = 70 + 55 * sheen + 28 * (streak - 0.5f) + 18 * (along - 0.07f) + 6 * (Noise(static_cast<uint32_t>(x), row) - 0.5f);
        return {level, level * 1.01f, level * 1.04f};
    }

    // Your own picture: loaded once, kept while its file is the same
    struct Own {
        std::string path;
        std::filesystem::file_time_type time;
        std::vector<uint32_t> pixels; // (red in the low byte)
        int width = 0, height = 0;
    };
    Own &TheOwn() {
        static Own own;
        return own;
    }
    bool LoadOwn(const std::string &folder) {
        std::error_code error;
        std::vector<std::filesystem::path> found;
        for (const auto &entry : std::filesystem::directory_iterator(folder, error)) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".png" || extension == ".bmp") {
                found.push_back(entry.path());
            }
        }
        Own &own = TheOwn();
        if (found.empty()) {
            if (!own.path.empty() || own.width == 0) {
                fprintf(stderr, "bezel: no .png or .bmp picture in %s\n", folder.c_str());
            }
            own = Own{};
            return false;
        }
        std::sort(found.begin(), found.end());
        const std::string path = found.front().string();
        const auto time = std::filesystem::last_write_time(found.front(), error);
        if (path == own.path && time == own.time && own.width > 0) {
            return true;
        }
        own = Own{};
        own.path = path;
        own.time = time;
        const bool png = std::tolower(static_cast<unsigned char>(found.front().extension().string()[1])) == 'p'; // (.png, else .bmp)
        SDL_Surface *loaded = png ? SDL_LoadPNG(path.c_str()) : SDL_LoadBMP(path.c_str());
        SDL_Surface *surface = loaded ? SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888) : nullptr;
        if (loaded) {
            SDL_DestroySurface(loaded);
        }
        if (!surface) {
            fprintf(stderr, "bezel %s: %s\n", path.c_str(), SDL_GetError());
            return false;
        }
        own.width = surface->w;
        own.height = surface->h;
        own.pixels.resize(static_cast<size_t>(own.width) * own.height);
        for (int y = 0; y < own.height; y++) {
            const uint32_t *row = reinterpret_cast<const uint32_t *>(static_cast<const uint8_t *>(surface->pixels) + static_cast<size_t>(y) * surface->pitch);
            for (int x = 0; x < own.width; x++) {
                own.pixels[static_cast<size_t>(y) * own.width + x] = row[x] | 0xFF000000u; // (opaque: under the picture)
            }
        }
        SDL_DestroySurface(surface);
        return true;
    }
} // namespace

bool MakeBezel(Bezel bezel, int width, int height, const SDL_FRect &picture, const std::string &ownFolder, std::vector<uint32_t> &out,
               int &outWidth, int &outHeight) {
    if (bezel == Bezel::Off || bezel == Bezel::Black || width <= 0 || height <= 0) {
        return false;
    }
    if (bezel == Bezel::YourOwn && !LoadOwn(ownFolder)) {
        return false;
    }
    // (no larger than a 1080-line screen's: stretched to the window, smoothly)
    const float by = std::min({1.0f, 1080.0f / height, 4096.0f / width});
    outWidth = std::max(1, static_cast<int>(std::lround(width * by)));
    outHeight = std::max(1, static_cast<int>(std::lround(height * by)));
    out.resize(static_cast<size_t>(outWidth) * outHeight);
    const float w = static_cast<float>(outWidth), h = static_cast<float>(outHeight);
    // (the picture's rectangle in the bezel's pixels, for the shadow round it)
    const float left = picture.x * by, top = picture.y * by, right = (picture.x + picture.w) * by, foot = (picture.y + picture.h) * by;
    const float shadow = h * 0.025f;
    const Own &own = TheOwn();
    // (Your own: the picture as large as covers the whole window, its middle in the middle, the rest cut off)
    const float cover = own.width > 0 ? std::max(w / own.width, h / own.height) : 1.0f;
    for (int yi = 0; yi < outHeight; yi++) {
        for (int xi = 0; xi < outWidth; xi++) {
            const float x = xi + 0.5f, y = yi + 0.5f;
            Colour c = {0, 0, 0};
            switch (bezel) {
            case Bezel::White: c = {255, 255, 255}; break;
            case Bezel::Dusk: c = Dusk(x, y, w, h); break;
            case Bezel::Carbon: c = Carbon(x, y, h); break;
            case Bezel::NeonGrid: c = NeonGrid(x, y, w, h); break;
            case Bezel::BrushedMetal: c = BrushedMetal(x, y, h); break;
            case Bezel::YourOwn: {
                const float sx = std::clamp((x - w / 2) / cover + own.width / 2.0f - 0.5f, 0.0f, own.width - 1.0f);
                const float sy = std::clamp((y - h / 2) / cover + own.height / 2.0f - 0.5f, 0.0f, own.height - 1.0f);
                const int x0 = static_cast<int>(sx), y0 = static_cast<int>(sy);
                const int x1 = std::min(x0 + 1, own.width - 1), y1 = std::min(y0 + 1, own.height - 1);
                auto at = [&](int px, int py) {
                    const uint32_t p = own.pixels[static_cast<size_t>(py) * own.width + px];
                    return Colour{static_cast<float>(p & 0xFF), static_cast<float>((p >> 8) & 0xFF), static_cast<float>((p >> 16) & 0xFF)};
                };
                c = Mix(Mix(at(x0, y0), at(x1, y0), sx - x0), Mix(at(x0, y1), at(x1, y1), sx - x0), sy - y0);
                break;
            }
            default: break;
            }
            // (a soft shadow the picture casts beside it: not on plain white, nor on the player's own)
            if (bezel != Bezel::White && bezel != Bezel::YourOwn) {
                const float dx = std::max({left - x, x - right, 0.0f}), dy = std::max({top - y, y - foot, 0.0f});
                const float away = std::sqrt(dx * dx + dy * dy);
                if (away < shadow) {
                    c = Mix(c, {0, 0, 0}, 0.6f * (1.0f - away / shadow) * (1.0f - away / shadow));
                }
            }
            out[static_cast<size_t>(yi) * outWidth + xi] = Pack(c);
        }
    }
    return true;
}

} // namespace port
