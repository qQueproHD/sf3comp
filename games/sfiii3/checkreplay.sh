#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# The instant replay's check (port.h, Instant: the Video page's Save replay). A one-player game of made-up play, headless,
# recorded as a video as it plays (--record-video) and Save replay asked for at frame 4200 (the last 30 seconds, made
# a few frames a tick beside the game, which goes on): each of the replay's pictures and its sound, as handed to its
# video, against the live video's of the same frame (CPS3RECOMP_VIDEO_SUMS: CRC-32s of the pixels and the samples),
# which must be the same, every one; and the live run's against the same run with no replay asked for, which must be the
# same too (the replay's frames are run in the port's machine, which is put back after each tick). On 3rd Strike's own
# stage, and on a guest stage (New Generation's, whose stage board the replay keeps and steps beside the states), each
# in 4:3. Prints what it compared and "checkreplay: all the same", or what is wrong. Exit 0 when all is well. About
# three minutes.
#
#   games/sfiii3/checkreplay.sh ROM [BUILD]     (ROM: the sfiii3nr1 zip, with sfiiin's beside it; BUILD: build/sfiii3)
ROM=${1:?usage: checkreplay.sh ROM [BUILD]}
BUILD=${2:-build/sfiii3}
OUT=${CHECKREPLAY_KEEP:-$(mktemp -d)} # (CHECKREPLAY_KEEP=DIR: the runs kept there)
mkdir -p "$OUT"
SAVE_AT=4200
FRAMES=4400
status=0
run() { # name replay(0/1) args...
    local name=$1 replay=$2
    shift 2
    rm -rf "$OUT/$name"
    mkdir -p "$OUT/$name"
    local keys=()
    [ "$replay" = 1 ] && keys=(--keys "$SAVE_AT:cvideo_replay_save")
    CPS3RECOMP_VIDEO_SUMS="$OUT/$name/sums.txt" nice -n 20 "$BUILD/sfiii3" --rom "$ROM" --headless --auto 7:300:1 --frames "$FRAMES" \
        --settings "$OUT/$name/settings.ini" --eeprom "$OUT/$name/eeprom.bin" --record-video "$OUT/$name/live.mp4" \
        "${keys[@]}" "$@" > /dev/null 2> "$OUT/$name/log.txt" || { echo "checkreplay: $name: the game failed (exit $?)"; status=1; }
}
compare() { # name
    python3 - "$OUT/$1" "$OUT/$1-plain" "$1" <<'EOF' || status=1
import sys, re, os
out, plain, name = sys.argv[1:]
def sums(path):
    live, replay = {}, {}
    for line in open(path):
        which, frame, picture, sound = line.split()
        (live if which == "live" else replay)[int(frame)] = (picture, sound)
    return live, replay
live, replay = sums(f"{out}/sums.txt")
alone, _ = sums(f"{plain}/sums.txt")
log = open(f"{out}/log.txt").read()
m = re.search(r"replay: .* frames (\d+) to (\d+) \((\d+) states, ([0-9.]+) MB kept\)", log)
saved = re.search(r"replay: saved ", log) is not None
bad = []
if not m or not saved:
    bad.append("no replay saved")
else:
    start, end = int(m.group(1)), int(m.group(2))
    if end - start != 1800:
        bad.append(f"the replay is frames {start} to {end}, not 30 seconds")
    frames = sorted(replay)
    if len(frames) != end - start:
        bad.append(f"{len(frames)} replay pictures, not {end - start}")
    pictures = [f for f in frames if live.get(f, (None,))[0] != replay[f][0]]
    sounds = [f for f in frames if live.get(f, (None, None))[1] != replay[f][1]]
    if pictures:
        bad.append(f"{len(pictures)} replay pictures not the play's (first: frame {pictures[0]})")
    if sounds:
        bad.append(f"{len(sounds)} replay frames' sound not the play's (first: frame {sounds[0]})")
    print(f"checkreplay: {name}: frames {start} to {end}, {m.group(3)} states kept in {m.group(4)} MB; "
          f"{len(frames) - len(pictures)} of {len(frames)} pictures and {len(frames) - len(sounds)} sounds the play's")
moved = [f for f in alone if live.get(f) != alone[f]]
if len(alone) != len(live) or moved:
    bad.append(f"the play with a replay made is not the play without one ({len(moved)} frames differ"
               + (f", first {moved[0]}" if moved else "") + f"; {len(live)} and {len(alone)} pictures)")
else:
    print(f"checkreplay: {name}: the play's {len(live)} frames the same with a replay made as without")
for b in bad:
    print(f"checkreplay: {name}: {b}")
sys.exit(1 if bad else 0)
EOF
}
for kind in own guest; do
    args=()
    [ "$kind" = guest ] && args=(--features "stage=22") # (New Generation's first stage: games/sfiii3/checkguests.sh's numbering)
    run "$kind" 1 "${args[@]}" &
    run "$kind-plain" 0 "${args[@]}" &
    wait
    compare "$kind"
done
[ $status = 0 ] && echo "checkreplay: all the same" || echo "checkreplay: DIFFERENT (runs in $OUT)"
exit $status
