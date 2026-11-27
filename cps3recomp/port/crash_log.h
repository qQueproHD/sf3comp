// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); after cps2recomp's port/main.cpp (spelunkomp's
// port/crash_log.h).
//
// A crash leaves its log. The launcher sends a game's output to logs/game.log; buffered in blocks, a crash lost the
// block and the launcher's report pointed at an empty log. KeepLogOnCrash writes the output as it goes (the
// standard output a line at a time, its errors at once; Windows' C library has no line buffering, its _IOLBF is
// full buffering, so there both are unbuffered), and on Windows an unhandled exception writes the exception, its
// address and, for an access violation, the address read or written, then the calls that led there as
// program+offset: the same build's unstripped program names them (build/release-windows/symbols,
// tools/release/crash_where.sh). Android sets its own output up (port/android.cpp).
#pragma once

#include <cstdint>

namespace port {

void KeepLogOnCrash();

// CPS3RECOMP_TEST_CRASH_AT_FRAME=N: the program crashes (a write to address 0) as frame N starts, to see what a
// crash leaves in the log (Port::RunFrame)
void TestCrashAt(uint64_t frame);

} // namespace port
