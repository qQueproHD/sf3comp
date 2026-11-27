# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy

cmake_minimum_required(VERSION 3.28)
project(sf3comp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif ()

set(GAME "sfiii3" CACHE STRING "Game id (directory under games/)")
set(GAME_DIR "${CMAKE_SOURCE_DIR}/games/${GAME}")
set(WORK_DIR "${CMAKE_SOURCE_DIR}/work/${GAME}")
set(CR "${CMAKE_SOURCE_DIR}/cps3recomp")

# (Linux on 64-bit ARM: the translated code makes a program of over 128 MB, past the reach of a call; GNU ld then
# fails with "relocation truncated to fit", gold adds the long-branch stubs)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    add_link_options(-fuse-ld=gold)
endif ()

# (Android: each game is a shared library, so the static libraries it is made of are position-independent too)
if (ANDROID)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

find_package(ZLIB REQUIRED)
if (APPLE)
    find_package(SDL3 3.4.16 REQUIRED CONFIG)
else ()
    find_package(SDL3 REQUIRED CONFIG)
endif ()
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" RECOMP_VERSION LIMIT_COUNT 1)

# game.cfg's values as GAME_<KEY> strings
add_custom_command(OUTPUT "${WORK_DIR}/game_cfg.h"
    COMMAND python3 "${CR}/tools/gamecfg.py" --header "${WORK_DIR}/game_cfg.h" ${GAME}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS "${GAME_DIR}/game.cfg" "${CR}/tools/gamecfg.py")
add_custom_command(OUTPUT "${WORK_DIR}/game_info.h"
    COMMAND python3 "${CR}/tools/gamecfg.py" --header "${WORK_DIR}/game_info.h" ${GAME}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS "${GAME_DIR}/game.cfg" "${CR}/tools/gamecfg.py")
add_custom_target(game_cfg DEPENDS "${WORK_DIR}/game_cfg.h" "${WORK_DIR}/game_info.h")

set(MACHINE_SOURCES
    ${CR}/machine/sh2.cpp ${CR}/machine/machine.cpp ${CR}/machine/crypt.cpp ${CR}/machine/romset.cpp
    ${CR}/machine/state.cpp ${CR}/machine/bus.cpp ${CR}/video/renderer.cpp ${CR}/video/native.cpp ${CR}/video/raster.cpp ${CR}/sound/pcm.cpp ${CR}/sound/native_sound.cpp)
add_library(cps3machine STATIC ${MACHINE_SOURCES})
target_include_directories(cps3machine PUBLIC ${CR}/machine ${CR}/video ${CR}/sound ${CR}/runtime ${CR}/port)
target_link_libraries(cps3machine PUBLIC ZLIB::ZLIB)
target_compile_options(cps3machine PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_compile_options(cps3machine PUBLIC -fsigned-char)

# The game's SH-2 code translated: work/<game>/gen/blocks_*.cpp, written by cps3recomp/tools/recomp.py (make
# translate). Without it the game still builds and everything runs in the interpreter. A game whose code was
# generated with --prefix (a stage game: its tables register themselves by name) gets the empty table of its own and
# its code among the registered ones (STAGE_GEN below).
set(STAGE_GEN_SOURCES "")
file(GLOB GEN_SOURCES CONFIGURE_DEPENDS "${WORK_DIR}/gen/blocks_*.cpp")
if (GEN_SOURCES AND EXISTS "${WORK_DIR}/gen/blocks_index.cpp")
    file(READ "${WORK_DIR}/gen/blocks_index.cpp" GEN_INDEX)
    string(FIND "${GEN_INDEX}" "RegisterBlockTables(" GEN_PREFIXED)
    if (NOT GEN_PREFIXED EQUAL -1)
        list(APPEND STAGE_GEN_SOURCES ${GEN_SOURCES})
        set(GEN_SOURCES "")
    endif ()
endif ()
if (GEN_SOURCES)
    message(STATUS "sf3comp: ${GAME}: translated code found in ${WORK_DIR}/gen")
else ()
    message(STATUS "sf3comp: ${GAME}: no translated code of its own (interpreter, or registered code)")
    set(GEN_SOURCES ${CR}/runtime/no_blocks.cpp)
endif ()
# The stage games' translated code (the definitive version's stage boards: games/sfiii3/src/stages.cpp), linked into
# 3rd Strike's program when it has been generated (work/<id>/gen, with --prefix: tools/translate.sh does that)
if (GAME STREQUAL "sfiii3")
    foreach (stage sfiiin sfiii2n)
        file(GLOB stage_gen CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/work/${stage}/gen/blocks_*.cpp")
        if (stage_gen)
            message(STATUS "sf3comp: ${GAME}: the stage game ${stage}'s translated code too")
            list(APPEND STAGE_GEN_SOURCES ${stage_gen})
        endif ()
    endforeach ()
endif ()
add_library(cps3runtime STATIC ${CR}/runtime/recomp.cpp ${GEN_SOURCES})
target_link_libraries(cps3runtime PUBLIC cps3machine)
target_compile_options(cps3runtime PRIVATE -Wno-unused-label -Wno-unused-variable)
# (an object library: its index files are referenced by nothing, they register themselves; a static library would
# leave them out)
if (STAGE_GEN_SOURCES)
    add_library(cps3stagegen OBJECT ${STAGE_GEN_SOURCES})
    target_link_libraries(cps3stagegen PUBLIC cps3machine)
    target_compile_options(cps3stagegen PRIVATE -Wno-unused-label -Wno-unused-variable)
    target_link_libraries(cps3runtime PUBLIC cps3stagegen $<TARGET_OBJECTS:cps3stagegen>)
endif ()

add_executable(${GAME}-headless ${CR}/port/headless.cpp)
add_dependencies(${GAME}-headless game_cfg)
target_include_directories(${GAME}-headless PRIVATE "${WORK_DIR}")
target_link_libraries(${GAME}-headless PRIVATE cps3runtime cps3machine)

# The game, with its window, menu and features (port/, taken from cps2recomp; recomp-common/ is the shared copy)
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS "${GAME_DIR}/src/*.cpp")
# (sources several games share: game.cfg's `shared_sources`, a folder under games/, e.g. shared/sf3)
file(STRINGS "${GAME_DIR}/game.cfg" SHARED_LINE REGEX "^shared_sources[ \t]*=")
string(REGEX REPLACE "^shared_sources[ \t]*=[ \t]*" "" SHARED_DIR "${SHARED_LINE}")
if (SHARED_DIR)
    file(GLOB SHARED_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/games/${SHARED_DIR}/*.cpp")
    list(APPEND GAME_SOURCES ${SHARED_SOURCES})
endif ()
if (NOT GAME_SOURCES)
    set(GAME_SOURCES ${CR}/runtime/no_game.cpp)
endif ()
set(PORT_SOURCES
    ${CR}/port/main.cpp ${CR}/port/crash_log.cpp ${CR}/port/scene.cpp ${CR}/port/video_record.cpp ${CR}/port/frontend_sdl.cpp ${CR}/port/present.cpp ${CR}/port/bezel.cpp ${CR}/port/shading.cpp ${CR}/port/zoom.cpp
    ${CR}/port/replay.cpp ${CR}/port/menu.cpp ${CR}/port/hud.cpp ${CR}/port/touch.cpp ${CR}/port/netplay.cpp
    ${CR}/port/soundtrack.cpp ${CR}/port/decoders.cpp ${CR}/port/dreamcast.cpp ${CR}/port/native_gpu.cpp
    ${CR}/recomp-common/launcher/src/online.cpp ${CR}/recomp-common/launcher/src/run_program.cpp
    ${CR}/recomp-common/menu.cpp ${CR}/recomp-common/settings.cpp ${CR}/recomp-common/arcade_screen.cpp
    ${CR}/runtime/game.cpp ${CR}/runtime/system.cpp ${CR}/runtime/soundtrack.cpp ${CR}/runtime/spare.cpp ${CR}/fight/frames.cpp ${CR}/fight/training.cpp ${CR}/fight/session.cpp ${GAME_SOURCES})
# The video recording's encoders (port/video_record.h), other projects' code compiled as it is: minih264 (H.264) and
# minimp4 (the MP4 file), CC0, in cps3recomp/third_party/ and compiled by port/encoders*.c; and FAAC 2.2 (AAC),
# LGPL-2.1, downloaded (its release's archive, checked against its SHA-256) and compiled with port/faac_config.h: its
# plain C quantizer only, the SSE2 one left out, so that it is the same everywhere (cps2recomp's, the same)
include(FetchContent)
FetchContent_Declare(faac
    URL https://github.com/knik0/faac/archive/refs/tags/faac-2.2.tar.gz
    URL_HASH SHA256=a93963573907c83e26e8cfabbf80d3a9c360f06ea4ecf1ea6cb74a202494d8d9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(faac) # (its sources only: it has no CMakeLists.txt, and its own build, meson's, is not used)
file(GLOB FAAC_SOURCES "${faac_SOURCE_DIR}/libfaac/*.c")
list(REMOVE_ITEM FAAC_SOURCES "${faac_SOURCE_DIR}/libfaac/quantize_sse.c")
add_library(video_encoders STATIC ${CR}/port/encoders.c ${CR}/port/encoders_mp4.c ${FAAC_SOURCES})
target_include_directories(video_encoders PUBLIC ${CR}/third_party "${faac_SOURCE_DIR}/include" PRIVATE "${faac_SOURCE_DIR}/libfaac")
# (minih264's colour description on, and its scalable video off: minih264e.h)
target_compile_definitions(video_encoders PRIVATE H264E_VUI_BT709=1 H264E_SVC_API=0 NDEBUG)
target_compile_options(video_encoders PRIVATE -w -include "${CR}/port/faac_config.h")
set_target_properties(video_encoders PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (UNIX AND NOT APPLE)
    target_link_libraries(video_encoders PRIVATE m)
endif ()
if (APPLE)
    enable_language(OBJCXX)
    list(APPEND PORT_SOURCES ${CR}/port/front_macos.mm)
endif ()
# (Android: a library, lib<id>.so, which the game's activity loads and calls the main of: android/,
# tools/release/android.sh)
if (ANDROID)
    list(APPEND PORT_SOURCES ${CR}/port/android.cpp)
    add_library(${GAME} SHARED ${PORT_SOURCES})
else ()
    add_executable(${GAME} ${PORT_SOURCES})
endif ()
add_dependencies(${GAME} game_cfg)
if (WIN32)
    set_target_properties(${GAME} PROPERTIES WIN32_EXECUTABLE ON)
    # (what the program is, whose and which version, and its manifest: cps3recomp/port/windows/)
    include(${CR}/port/windows/resources.cmake)
    file(STRINGS "${GAME_DIR}/game.cfg" GAME_NAME_LINE REGEX "^name *=" LIMIT_COUNT 1)
    string(REGEX REPLACE "^name *= *" "" GAME_FULL_NAME "${GAME_NAME_LINE}")
    recomp_windows_resources(${GAME} ${GAME} "${GAME_FULL_NAME}" "${RECOMP_VERSION}")
endif ()
target_include_directories(${GAME} PRIVATE ${CR}/runtime ${CR}/port ${CR}/fight "${WORK_DIR}" "${GAME_DIR}")
if (SHARED_DIR)
    target_include_directories(${GAME} PRIVATE "${CMAKE_SOURCE_DIR}/games/${SHARED_DIR}")
endif ()
target_link_libraries(${GAME} PRIVATE cps3runtime cps3machine SDL3::SDL3 video_encoders)
# (Switchres, GPL-2.0-or-later, downloaded and linked in on Windows: the Arcade monitor's modes made at the board's
# exact rate, recomp-common's arcade_screen.h)
include(${CR}/recomp-common/switchres.cmake)
recomp_switchres(${GAME})
if (APPLE)
    target_link_libraries(${GAME} PRIVATE "-framework AppKit")
endif ()
if (WIN32)
    target_link_libraries(${GAME} PRIVATE ws2_32 iphlpapi)
    # (online play's requests made with WinHTTP, not a hidden curl.exe, which antivirus programs take for a
    # downloader: recomp-common/launcher/src/winhttp_curl.h)
    target_link_libraries(${GAME} PRIVATE winhttp)
    target_compile_definitions(${GAME} PRIVATE RECOMP_WINHTTP=1)
endif ()
if (ANDROID)
    target_link_libraries(${GAME} PRIVATE log)
endif ()
target_compile_options(${GAME} PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-label -Wno-unused-variable)
target_compile_definitions(${GAME} PRIVATE RECOMP_VERSION="${RECOMP_VERSION}")

# The launcher (cps3recomp/launcher; recomp-common's), told about the game from its game.cfg
function(game_cfg key out)
    file(STRINGS "${GAME_DIR}/game.cfg" line REGEX "^${key}[ \t]*=")
    string(REGEX REPLACE "^${key}[ \t]*=[ \t]*" "" line "${line}")
    set(${out} "${line}" PARENT_SCOPE)
endfunction()
game_cfg(name GAME_NAME)
game_cfg(app_id APP_ID)
game_cfg(disclaimer DISCLAIMER)
game_cfg(rom_set ROM_SET)
game_cfg(rom_parent ROM_PARENT)
game_cfg(rom_name ROM_NAME)
game_cfg(bios ROM_BIOS)
foreach (n 1 2 3 4 5 6)
    game_cfg(simm${n} ROM_SIMM${n})
endforeach ()
game_cfg(program_enciphered PROGRAM_ENCIPHERED)
game_cfg(launcher_extra_sets EXTRA_SETS_WORDS)
game_cfg(display DISPLAY_SWITCHES)
set(GAME_EXE ${GAME})
string(TOUPPER "${GAME}_HOME" HOME_ENV)
set(GAME_VERSION ${RECOMP_VERSION})
add_subdirectory(cps3recomp/launcher)
set_target_properties(launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
