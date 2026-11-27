// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); after cps2recomp's port/main.cpp (spelunkomp's
// port/crash_log.cpp).
//
// A crash leaves its log (crash_log.h).
#include "crash_log.h"

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstring>

namespace {

// (an address as the program or library it is in, and how far into it: the same build's unstripped program
// turns that into a function: tools/release/crash_where.sh)
void SayWhere(DWORD64 address) {
    HMODULE module = nullptr;
    char path[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(address), &module) &&
        module) {
        GetModuleFileNameA(module, path, sizeof path);
    }
    const char *name = strrchr(path, '\\');
    fprintf(stderr, "  %016llx  %s+0x%llx\n", static_cast<unsigned long long>(address), name ? name + 1 : path,
            static_cast<unsigned long long>(module ? address - reinterpret_cast<DWORD64>(module) : 0));
}

// A crash (a bad memory access and the like): where it happened and the calls that led there, into the log, then
// Windows ends the program as before (the launcher shows its code)
LONG WINAPI SayCrash(EXCEPTION_POINTERS *e) {
    const EXCEPTION_RECORD *r = e->ExceptionRecord;
    char program[MAX_PATH] = "the game";
    if (GetModuleFileNameA(nullptr, program, sizeof program) == 0) {
        strcpy(program, "the game");
    }
    const char *name = strrchr(program, '\\');
    fprintf(stderr, "\n%s crashed: exception %08lX at %p", name ? name + 1 : program, r->ExceptionCode,
            r->ExceptionAddress);
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2) {
        const ULONG_PTR how = r->ExceptionInformation[0];
        fprintf(stderr, " (%s %p)", how == 0 ? "reading" : how == 1 ? "writing" : "executing",
                reinterpret_cast<void *>(r->ExceptionInformation[1]));
    }
    fprintf(stderr, ", thread %lu\ncalls, the crash first:\n", GetCurrentThreadId());
#if defined(_M_X64) || defined(__x86_64__)
    CONTEXT c = *e->ContextRecord;
    for (int i = 0; i < 40 && c.Rip; i++) {
        SayWhere(c.Rip);
        DWORD64 base = 0;
        RUNTIME_FUNCTION *f = RtlLookupFunctionEntry(c.Rip, &base, nullptr);
        if (!f) { // (a leaf function: its return address is on top of the stack)
            c.Rip = *reinterpret_cast<DWORD64 *>(c.Rsp);
            c.Rsp += 8;
            continue;
        }
        void *handler = nullptr;
        DWORD64 frame = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, c.Rip, f, &c, &handler, &frame, nullptr);
    }
#else
    SayWhere(reinterpret_cast<DWORD64>(r->ExceptionAddress));
#endif
    fflush(stderr);
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace
#endif

namespace port {

void KeepLogOnCrash() {
#ifdef _WIN32
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    SetUnhandledExceptionFilter(SayCrash);
#else
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
#endif
}

void TestCrashAt(uint64_t frame) {
    static const uint64_t at = [] {
        const char *n = getenv("CPS3RECOMP_TEST_CRASH_AT_FRAME");
        return n ? strtoull(n, nullptr, 10) : 0ull;
    }();
    if (at && frame == at) {
        fprintf(stdout, "CPS3RECOMP_TEST_CRASH_AT_FRAME: crashing at frame %llu\n", static_cast<unsigned long long>(frame));
        *static_cast<volatile int *>(nullptr) = 1;
    }
}

} // namespace port
