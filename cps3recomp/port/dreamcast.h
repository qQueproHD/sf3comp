// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp).
//
// Files from a Dreamcast disc image the player has (the Dreamcast version's arranged music, say: Game::dreamcastMusic).
// The image is the disc's high-density area as dumps give it: a .cue (Redump: "REM HIGH-DENSITY AREA" and its tracks)
// or a .gdi (TOSEC: the track at LBA 45000), with their track files beside them, or a .zip holding one of those with its
// tracks (read through once, unpacked as it goes: nothing is unpacked to disk but the files asked for). Its file system
// is ISO 9660 at LBA 45000 on, its sectors 2352 bytes (mode 1: 2048 of data after 16) or 2048.
#pragma once

#include <string>
#include <vector>

namespace port {

// The disc images in a folder, by their endings (.cue, .gdi, .zip): what CopyFromDisc may be given
std::vector<std::string> DiscImagesIn(const std::string &folder);

// Copies the files named (by their paths on the disc, "SOUND/27_C_AFR.ADX", any case) from the image into `folder`
// under their own names (written to a part file first, so that none is left half made). True when every one was; else
// `error` says why (the image not a disc's, a file not on it).
bool CopyFromDisc(const std::string &image, const std::vector<std::string> &files, const std::string &folder, std::string &error);
// ... and whether that image looked like a disc's at all (its .cue or .gdi read): for telling a player which of the
// images beside the ROM set was the one meant (a ROM set's zip is not)
bool LooksLikeDisc(const std::string &image);

} // namespace port
