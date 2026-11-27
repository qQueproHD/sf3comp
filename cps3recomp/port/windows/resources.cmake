# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
#
# recomp_windows_resources(target name description version): a Windows program's version information and manifest
# (program.rc.in, program.manifest), so it says what it is: one with none, unsigned and new, is what antivirus
# programs take for malware. The version is VERSION's (0.2.0-beta.2: 0.2.0.2 in the numbers, as it is in the text).
function(recomp_windows_resources target name description version)
    if (NOT WIN32)
        return()
    endif ()
    enable_language(RC)
    string(REGEX MATCHALL "[0-9]+" numbers "${version}")
    list(APPEND numbers 0 0 0 0)
    list(SUBLIST numbers 0 4 numbers)
    string(REPLACE ";" "," RES_VERSION_COMMAS "${numbers}")
    set(RES_VERSION "${version}")
    set(RES_NAME "${name}")
    string(REPLACE "\"" "'" RES_DESCRIPTION "${description}")
    set(RES_MANIFEST "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/program.manifest")
    set(rc "${CMAKE_CURRENT_BINARY_DIR}/${target}_resources.rc")
    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/program.rc.in" "${rc}" @ONLY)
    target_sources(${target} PRIVATE "${rc}")
    set_property(SOURCE "${rc}" APPEND PROPERTY OBJECT_DEPENDS "${RES_MANIFEST}")
endfunction()
