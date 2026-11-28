#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp's tools/check_rewind.sh.
#
# Rewind and the practice point (docs/FEATURES.md, port/port.h) leave the game as it would be without them. Made-up
# play is recorded as a movie and run straight; then again:
#   1. with both switched on and never used: every frame's state hash (the picture's too) the same;
#   2. a practice point set at frame FROM and gone back to 300 frames later, then played on: the frame the run ends on
#      the same as the straight run's (the movie's controls follow the frame count back);
#   3. rewind held for 60 ticks at FROM (180 frames back), then played on: the same;
#   4. both of those, with run-ahead at 2 frames;
#   5. the practice point's replay (Backslash) right after going back, then played on: the same (the movie's next
#      new press takes over from the replay, as a player's would).
# A difference is something kept outside the save state (TheGame().state), or a frame run twice that does twice.
# EXTRA=ARGS: given to every run too (EXTRA="--features stage=22", with sfiiin's set beside ROMSET: on New Generation's
# first stage, whose stage board rewind keeps beside each state).
#
#   cps3recomp/tools/check_rewind.sh ROMSET [FRAMES] [FROM]   (from the top of the repository, after a build; GAME=sfiii3)
set -eu
ROM=${1:?usage: check_rewind.sh ROMSET [FRAMES] [FROM]}
frames=${2:-6000}
from=${3:-$((frames / 2))}
GAME=${GAME:-sfiii3}
TOP=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${BIN:-$TOP/build/$GAME/$GAME}
work=${OUT:-$TOP/work/$GAME/check} # (OUT=DIR: its files there)
mkdir -p "$work"
: > "$work/rewind-settings.ini"
run() { # name, then the program's arguments
    name=$1; shift
    rm -f "$work/rewind-eeprom.bin"
    "$BIN" --rom "$ROM" --headless --frames "$frames" --settings "$work/rewind-settings.ini" \
        --eeprom "$work/rewind-eeprom.bin" --hash-log "$work/$name.txt" ${EXTRA:-} "$@" 2> "$work/$name.err" || {
        echo "FAILED: the $name run stopped"; cat "$work/$name.err"; exit 1; }
}
status=0
# (the hash of the frame the run ended on, the last line for it: a run that went back has the frames twice)
last() { awk -v f="$frames" '$1 == f {h = $2} END {print h}' "$work/$1.txt"; }
same_end() { # what, run
    a=$(last rewind-straight); b=$(last "$2")
    if [ -n "$a" ] && [ "$a" = "$b" ]; then
        echo "ok: $1: frame $frames the same ($a)"
    else
        echo "FAILED: $1: frame $frames is $b, straight $a"
        status=1
    fi
}
movie="$work/rewind-movie.txt"
run rewind-record --auto "11:0:2" --record "$movie"
run rewind-straight --movie "$movie"
CPS3RECOMP_REWIND_LOG=1 run rewind-unused --movie "$movie" --features rewind,practice_point
if cmp -s "$work/rewind-straight.txt" "$work/rewind-unused.txt"; then
    echo "ok: on and not used: every frame the same over $frames frames"
else
    echo "FAILED: on and not used: first difference at frame $(diff "$work/rewind-straight.txt" "$work/rewind-unused.txt" | grep '^<' | head -1 | cut -d' ' -f2)"
    status=1
fi
# (ticks are frames here until something goes back: the keys are at ticks)
run rewind-point --movie "$movie" --features practice_point --keys "$from:],$((from + 300)):k1"
same_end "a practice point set at $from, gone back to at $((from + 300))" rewind-point
run rewind-held --movie "$movie" --features rewind --keys "$from:w,$((from + 60)):w"
same_end "rewind held 60 ticks at $from" rewind-held
run rewind-ahead --movie "$movie" --features rewind,practice_point,run_ahead=2 \
    --keys "$from:],$((from + 300)):k1,$((from + 400)):w,$((from + 460)):w"
same_end "both, with run-ahead" rewind-ahead
run rewind-replay --movie "$movie" --features practice_point --keys "$from:],$((from + 300)):k1,$((from + 300)):k2"
same_end "the practice point's replay" rewind-replay
grep -h "rewind:" "$work"/rewind-unused.err 2>/dev/null | tail -3 || true
exit $status
