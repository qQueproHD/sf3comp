// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp).
//
// Files from a Dreamcast disc image: dreamcast.h.
#include "dreamcast.h"

#include "port.h"

#include <zlib.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>

namespace port {

namespace {
    constexpr uint32_t kHighDensity = 45000; // (the high-density area's first sector: the file system's sector numbers count from 0)

    std::string Upper(std::string text) {
        for (char &c : text) {
            c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
        }
        return text;
    }
    std::string Lower(std::string text) {
        for (char &c : text) {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        return text;
    }
    uint32_t Le32(const uint8_t *p) {
        return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }
    uint16_t Le16(const uint8_t *p) {
        return static_cast<uint16_t>(p[0] | (p[1] << 8));
    }

    // The bytes of a track's file: read where asked, in any order
    struct Bytes {
        virtual ~Bytes() = default;
        virtual bool ReadAt(uint64_t at, uint8_t *out, size_t size) = 0;
    };

    // ... a file on disk
    struct FileBytes : Bytes {
        std::ifstream file;
        explicit FileBytes(const std::filesystem::path &path) : file(path, std::ios::binary) {}
        bool ReadAt(uint64_t at, uint8_t *out, size_t size) override {
            file.clear();
            file.seekg(static_cast<std::streamoff>(at));
            file.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(size));
            return file.gcount() == static_cast<std::streamsize>(size);
        }
    };

    // A zip file's list of files (zip.h's shape: romset.cpp's reader, with the sizes a track needs)
    struct Zip {
        struct Entry {
            std::string name;
            uint64_t packed = 0, size = 0, offset = 0;
            uint16_t method = 0;
        };
        std::filesystem::path path;
        std::vector<Entry> entries;

        bool Open(const std::filesystem::path &file) {
            path = file;
            std::ifstream f(file, std::ios::binary | std::ios::ate);
            if (!f) {
                return false;
            }
            const uint64_t size = static_cast<uint64_t>(f.tellg());
            const uint64_t tailSize = std::min<uint64_t>(size, 65536 + 22);
            std::vector<uint8_t> tail(tailSize);
            f.seekg(static_cast<std::streamoff>(size - tailSize));
            f.read(reinterpret_cast<char *>(tail.data()), static_cast<std::streamsize>(tailSize));
            if (!f) {
                return false;
            }
            int64_t end = -1;
            for (int64_t at = static_cast<int64_t>(tailSize) - 22; at >= 0; at--) {
                if (Le32(&tail[static_cast<size_t>(at)]) == 0x06054B50) {
                    end = at;
                    break;
                }
            }
            if (end < 0) {
                return false;
            }
            const uint32_t count = Le16(&tail[end + 10]), dirSize = Le32(&tail[end + 12]), dirAt = Le32(&tail[end + 16]);
            std::vector<uint8_t> dir(dirSize);
            f.seekg(dirAt);
            f.read(reinterpret_cast<char *>(dir.data()), dirSize);
            if (!f) {
                return false;
            }
            size_t at = 0;
            for (uint32_t n = 0; n < count && at + 46 <= dir.size() && Le32(&dir[at]) == 0x02014B50; n++) {
                Entry e;
                e.method = Le16(&dir[at + 10]);
                e.packed = Le32(&dir[at + 20]);
                e.size = Le32(&dir[at + 24]);
                e.offset = Le32(&dir[at + 42]);
                const size_t nameLen = Le16(&dir[at + 28]), extraLen = Le16(&dir[at + 30]);
                if (at + 46 + nameLen + extraLen > dir.size()) {
                    break;
                }
                e.name.assign(reinterpret_cast<const char *>(&dir[at + 46]), nameLen);
                // (zip64: the sizes and the place in its extra field, those that were too large for the record)
                for (size_t x = at + 46 + nameLen; x + 4 <= at + 46 + nameLen + extraLen;) {
                    const uint16_t id = Le16(&dir[x]), len = Le16(&dir[x + 2]);
                    if (id == 1) {
                        size_t v = x + 4;
                        auto take = [&](uint64_t &field) {
                            if (field == 0xFFFFFFFFu && v + 8 <= x + 4 + len) {
                                field = Le32(&dir[v]) | (static_cast<uint64_t>(Le32(&dir[v + 4])) << 32);
                                v += 8;
                            }
                        };
                        take(e.size);
                        take(e.packed);
                        take(e.offset);
                    }
                    x += 4 + len;
                }
                entries.push_back(e);
                at += 46 + nameLen + extraLen + Le16(&dir[at + 32]);
            }
            return !entries.empty();
        }
        const Entry *Find(const std::string &name) const {
            for (const Entry &e : entries) {
                const size_t slash = e.name.find_last_of('/');
                if (Lower(slash == std::string::npos ? e.name : e.name.substr(slash + 1)) == Lower(name)) {
                    return &e;
                }
            }
            return nullptr;
        }
        // Where an entry's packed data starts
        bool DataAt(const Entry &e, uint64_t &at) const {
            std::ifstream f(path, std::ios::binary);
            uint8_t local[30];
            f.seekg(static_cast<std::streamoff>(e.offset));
            f.read(reinterpret_cast<char *>(local), 30);
            if (!f || Le32(local) != 0x04034B50) {
                return false;
            }
            at = e.offset + 30 + Le16(&local[26]) + Le16(&local[28]);
            return true;
        }
        bool ReadSmall(const Entry &e, std::string &text) const {
            std::unique_ptr<Bytes> bytes = Open(e);
            text.resize(static_cast<size_t>(e.size));
            return bytes && e.size < (1u << 20) && bytes->ReadAt(0, reinterpret_cast<uint8_t *>(text.data()), text.size());
        }
        std::unique_ptr<Bytes> Open(const Entry &e) const;
    };

    // ... a file packed in a zip: stored, read where asked; deflated, unpacked from its start as far as asked (asked for
    // an earlier place, it starts again: the reads here mostly go forward)
    struct ZipBytes : Bytes {
        std::ifstream file;
        uint64_t start = 0, packed = 0, size = 0;
        bool deflated = false;
        z_stream z = {};
        bool begun = false;
        uint64_t at = 0;        // (unpacked so far)
        uint64_t readPacked = 0;
        std::vector<uint8_t> in = std::vector<uint8_t>(1 << 16);

        ~ZipBytes() override {
            if (begun) {
                inflateEnd(&z);
            }
        }
        bool Begin() {
            if (begun) {
                inflateEnd(&z);
            }
            z = {};
            begun = inflateInit2(&z, -MAX_WBITS) == Z_OK;
            at = 0;
            readPacked = 0;
            return begun;
        }
        // Unpacks `size` bytes on into `out` (null: passed over)
        bool Unpack(uint8_t *out, uint64_t count) {
            std::vector<uint8_t> scratch;
            while (count > 0) {
                const size_t step = static_cast<size_t>(std::min<uint64_t>(count, 1 << 20));
                uint8_t *to = out;
                if (!to) {
                    scratch.resize(step);
                    to = scratch.data();
                }
                z.next_out = to;
                z.avail_out = static_cast<uInt>(step);
                while (z.avail_out > 0) {
                    if (z.avail_in == 0) {
                        const uint64_t want = std::min<uint64_t>(in.size(), packed - readPacked);
                        if (want == 0) {
                            return false;
                        }
                        file.clear();
                        file.seekg(static_cast<std::streamoff>(start + readPacked));
                        file.read(reinterpret_cast<char *>(in.data()), static_cast<std::streamsize>(want));
                        if (file.gcount() != static_cast<std::streamsize>(want)) {
                            return false;
                        }
                        readPacked += want;
                        z.next_in = in.data();
                        z.avail_in = static_cast<uInt>(want);
                    }
                    const int result = inflate(&z, Z_NO_FLUSH);
                    if (result != Z_OK && !(result == Z_STREAM_END && z.avail_out == 0)) {
                        return false;
                    }
                }
                at += step;
                count -= step;
                if (out) {
                    out += step;
                }
            }
            return true;
        }
        bool ReadAt(uint64_t where, uint8_t *out, size_t count) override {
            if (where + count > size) {
                return false;
            }
            if (!deflated) {
                file.clear();
                file.seekg(static_cast<std::streamoff>(start + where));
                file.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(count));
                return file.gcount() == static_cast<std::streamsize>(count);
            }
            if (!begun || where < at) {
                if (!Begin()) {
                    return false;
                }
            }
            return Unpack(nullptr, where - at) && Unpack(out, count);
        }
    };

    std::unique_ptr<Bytes> Zip::Open(const Entry &e) const {
        auto bytes = std::make_unique<ZipBytes>();
        bytes->file.open(path, std::ios::binary);
        if (!bytes->file || !DataAt(e, bytes->start) || (e.method != 0 && e.method != 8)) {
            return nullptr;
        }
        bytes->packed = e.packed;
        bytes->size = e.size;
        bytes->deflated = e.method == 8;
        return bytes;
    }

    // The high-density area's data track: its file's bytes, and how a sector is laid out in it
    struct Track {
        std::unique_ptr<Bytes> bytes;
        uint32_t sectorSize = 2352, dataAt = 16; // (2048-byte sectors: all data)
        bool Sector(uint32_t lba, uint8_t *out) const {
            if (lba < kHighDensity) {
                return false;
            }
            return bytes->ReadAt(static_cast<uint64_t>(lba - kHighDensity) * sectorSize + dataAt, out, 2048);
        }
    };

    std::string Unquote(std::string text) {
        const size_t from = text.find_first_not_of(" \t");
        text = from == std::string::npos ? "" : text.substr(from);
        if (!text.empty() && text[0] == '"') {
            const size_t end = text.find('"', 1);
            return text.substr(1, end == std::string::npos ? std::string::npos : end - 1);
        }
        return text.substr(0, text.find_first_of(" \t\r"));
    }

    // From a .cue's or .gdi's text: the high-density data track's file name and sector size
    bool TrackOf(const std::string &sheet, bool gdi, std::string &file, uint32_t &sectorSize) {
        std::istringstream lines(sheet);
        std::string line;
        if (gdi) {
            // ("3 45000 4 2352 track03.bin 0": its number, first sector, kind (4 data), sector size, file, offset)
            std::getline(lines, line);
            while (std::getline(lines, line)) {
                std::istringstream fields(line);
                unsigned number = 0, lba = 0, kind = 0, size = 0;
                if (!(fields >> number >> lba >> kind >> size)) {
                    continue;
                }
                std::string rest;
                std::getline(fields, rest);
                if (lba == kHighDensity && kind == 4) {
                    file = Unquote(rest);
                    sectorSize = size;
                    return !file.empty();
                }
            }
            return false;
        }
        // (a cue: the first data track after "REM HIGH-DENSITY AREA"; or, with no such remark, track 3)
        std::string lastFile;
        bool high = false, any = sheet.find("HIGH-DENSITY") != std::string::npos;
        while (std::getline(lines, line)) {
            const std::string upper = Upper(line);
            const size_t first = upper.find_first_not_of(" \t");
            const std::string word = first == std::string::npos ? "" : upper.substr(first);
            if (word.rfind("REM HIGH-DENSITY", 0) == 0) {
                high = true;
            } else if (word.rfind("FILE ", 0) == 0) {
                lastFile = Unquote(line.substr(line.find_first_of("Ff") + 4));
            } else if (word.rfind("TRACK ", 0) == 0) {
                unsigned number = 0;
                char mode[32] = {};
                if (sscanf(word.c_str(), "TRACK %u %31s", &number, mode) == 2 && strncmp(mode, "MODE1/", 6) == 0 &&
                    (any ? high : number == 3)) {
                    file = lastFile;
                    sectorSize = static_cast<uint32_t>(atoi(mode + 6));
                    return !file.empty();
                }
            }
        }
        return false;
    }

    bool OpenTrack(const std::filesystem::path &image, Track &track, std::string &error) {
        const std::string ending = Lower(PathText(image.extension()));
        std::string file;
        uint32_t sectorSize = 0;
        if (ending == ".zip") {
            auto zip = std::make_shared<Zip>();
            if (!zip->Open(image)) {
                error = "not a zip file this reads";
                return false;
            }
            for (const Zip::Entry &e : zip->entries) {
                const std::string name = Lower(e.name);
                const bool gdi = name.size() > 4 && name.compare(name.size() - 4, 4, ".gdi") == 0;
                if (!gdi && !(name.size() > 4 && name.compare(name.size() - 4, 4, ".cue") == 0)) {
                    continue;
                }
                std::string sheet;
                if (zip->ReadSmall(e, sheet) && TrackOf(sheet, gdi, file, sectorSize)) {
                    const Zip::Entry *data = zip->Find(std::filesystem::path(file).filename().string());
                    if (!data) {
                        error = "the zip has not the track " + file;
                        return false;
                    }
                    track.bytes = zip->Open(*data);
                    break;
                }
            }
            if (!track.bytes) {
                error = error.empty() ? "no disc's .cue or .gdi in the zip" : error;
                return false;
            }
        } else {
            std::ifstream sheetFile(image);
            std::stringstream sheet;
            sheet << sheetFile.rdbuf();
            if (!sheetFile || !TrackOf(sheet.str(), ending == ".gdi", file, sectorSize)) {
                error = "no high-density data track in it";
                return false;
            }
            const std::filesystem::path at = image.parent_path() / TextPath(file);
            std::error_code e;
            if (!std::filesystem::exists(at, e)) {
                error = "its track " + file + " is not beside it";
                return false;
            }
            track.bytes = std::make_unique<FileBytes>(at);
        }
        if (sectorSize != 2352 && sectorSize != 2048) {
            error = "sectors of " + std::to_string(sectorSize) + " bytes";
            return false;
        }
        track.sectorSize = sectorSize;
        track.dataAt = sectorSize == 2352 ? 16 : 0;
        return true;
    }

    struct DiscEntry {
        uint32_t lba = 0, size = 0;
    };

    // The disc's files (ISO 9660), by their paths in capitals ("SOUND/27_C_AFR.ADX")
    bool ReadDirectory(const Track &track, uint32_t lba, uint32_t size, const std::string &prefix, std::map<std::string, DiscEntry> &files,
                       int depth) {
        if (depth > 8 || size > (16u << 20)) {
            return false;
        }
        std::vector<uint8_t> data((size + 2047) / 2048 * 2048);
        for (uint32_t n = 0; n < data.size() / 2048; n++) {
            if (!track.Sector(lba + n, &data[n * 2048])) {
                return false;
            }
        }
        for (size_t at = 0; at < size;) {
            const uint8_t length = data[at];
            if (length == 0) {
                at = (at / 2048 + 1) * 2048; // (records don't cross a sector: the rest of this one is empty)
                continue;
            }
            if (length < 34 || at + length > data.size()) {
                return false;
            }
            const uint8_t *r = &data[at];
            at += length;
            const uint8_t nameLength = r[32];
            if (33u + nameLength > length) {
                return false;
            }
            std::string name(reinterpret_cast<const char *>(r + 33), nameLength);
            if (name == std::string(1, '\0') || name == "\x01") {
                continue;
            }
            name = Upper(name.substr(0, name.find(';')));
            const DiscEntry e{Le32(r + 2), Le32(r + 10)};
            if (r[25] & 2) {
                if (!ReadDirectory(track, e.lba, e.size, prefix + name + "/", files, depth + 1)) {
                    return false;
                }
            } else {
                files[prefix + name] = e;
            }
        }
        return true;
    }
} // namespace

std::vector<std::string> DiscImagesIn(const std::string &folder) {
    std::vector<std::string> images;
    std::error_code error;
    for (std::filesystem::directory_iterator each(TextPath(folder), error); !error && each != std::filesystem::directory_iterator();
         each.increment(error)) {
        const std::string ending = Lower(PathText(each->path().extension()));
        if (ending == ".cue" || ending == ".gdi" || ending == ".zip") {
            images.push_back(PathText(each->path()));
        }
    }
    std::sort(images.begin(), images.end());
    return images;
}

bool CopyFromDisc(const std::string &image, const std::vector<std::string> &files, const std::string &folder, std::string &error) {
    Track track;
    if (!OpenTrack(TextPath(image), track, error)) {
        return false;
    }
    // The volume's description (sector 16 of the area), and from it the root directory
    uint8_t pvd[2048];
    if (!track.Sector(kHighDensity + 16, pvd) || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        error = "no disc's file system on its high-density track";
        return false;
    }
    std::map<std::string, DiscEntry> all;
    if (!ReadDirectory(track, Le32(pvd + 156 + 2), Le32(pvd + 156 + 10), "", all, 0)) {
        error = "its file system could not be read";
        return false;
    }
    // The files asked for, in the order they are on the disc (a zip's track is unpacked once, going forward)
    std::vector<std::pair<std::string, DiscEntry>> wanted;
    for (const std::string &name : files) {
        const auto found = all.find(Upper(name));
        if (found == all.end()) {
            error = "no " + name + " on the disc";
            return false;
        }
        wanted.emplace_back(name, found->second);
    }
    std::sort(wanted.begin(), wanted.end(), [](const auto &a, const auto &b) { return a.second.lba < b.second.lba; });
    std::error_code e;
    std::filesystem::create_directories(TextPath(folder), e);
    std::vector<uint8_t> sector(2048);
    for (const auto &[name, entry] : wanted) {
        const std::filesystem::path to = TextPath(folder) / std::filesystem::path(name).filename();
        std::filesystem::path part = to;
        part += ".part";
        {
            std::ofstream out(part, std::ios::binary | std::ios::trunc);
            for (uint32_t done = 0, n = 0; done < entry.size; n++) {
                if (!track.Sector(entry.lba + n, sector.data())) {
                    error = "its track ends inside " + name;
                    return false;
                }
                const uint32_t take = std::min<uint32_t>(2048, entry.size - done);
                out.write(reinterpret_cast<const char *>(sector.data()), take);
                done += take;
            }
            if (!out) {
                error = "could not write " + PathText(to);
                return false;
            }
        }
        std::filesystem::rename(part, to, e);
        if (e) {
            error = "could not write " + PathText(to);
            return false;
        }
    }
    return true;
}

} // namespace port
