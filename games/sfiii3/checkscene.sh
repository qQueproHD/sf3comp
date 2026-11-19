#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# The scene export's check (port/scene.h): made-up play to a fight, Save scene asked for at the title (frame 1500) and in
# the fight (3900), and Record clip there (3600, its 3 seconds), with SVG pictures too; on 3rd Strike's own stage and on
# a guest stage (New Generation's), each in 4:3 and in widescreen; headless (each picture drawn from this board's
# interface, with the guest's or not), and a guest stage's fight through the window's GPU path (SDL's offscreen video:
# the picture made for the GPU, drawn on the CPU when the scene asks for it). Every scene's layers, laid over each other
# on its backdrop, must make its picture again pixel for pixel, a clip's every frame too (scenecheck.py); in a fight,
# its parts too, and each object by itself must be its layer where it is in front. And a training session (made-up
# characters chosen, hit boxes and the frame meter on): its scenes' training overlays and hit boxes, each with its
# player. Prints what it compared and "checkscene: all the same", or what is wrong. Exit 0 when all is well. About
# six minutes.
#
#   games/sfiii3/checkscene.sh ROM [BUILD]     (ROM: the sfiii3nr1 zip, with sfiiin's beside it; BUILD: build/sfiii3)
ROM=${1:?usage: checkscene.sh ROM [BUILD]}
BUILD=${2:-build/sfiii3}
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${CHECKSCENE_KEEP:-$(mktemp -d)} # (CHECKSCENE_KEEP=DIR: the runs kept there)
mkdir -p "$OUT"
KEYS="1500:cscene_save,3600:cscene_clip,3900:cscene_save"
status=0
run() { # name features headless|gpu [keys]
    local name=$1 features=$2 mode=$3 keys=${4:-$KEYS}
    rm -rf "${OUT:?}/${name:?}"
    mkdir -p "$OUT/$name"
    local env=() args=(--headless)
    if [ "$mode" = gpu ]; then
        env=(SDL_AUDIO_DRIVER=dummy SDL_VIDEO_DRIVER=offscreen CPS3RECOMP_WINDOW_OFFSCREEN=1 CPS3RECOMP_WINDOW_SIZE=1280x720)
        args=(--unthrottled --mute)
    fi
    env "${env[@]}" nice -n 20 "$BUILD/sfiii3" --rom "$ROM" "${args[@]}" --auto 7:300:1 --frames 4000 --features "export_svg$features" \
        --keys "$keys" --settings "$OUT/$name/settings.ini" --eeprom "$OUT/$name/eeprom.bin" > /dev/null 2> "$OUT/$name/log.txt" ||
        { echo "checkscene: $name: the game failed (exit $?)"; status=1; }
}
run own "" headless &
run guest ",stage=22" headless &
wait
run own-wide ",widescreen" headless &
run guest-wide ",stage=22,widescreen" headless &
# (a training session: its fight from about frame 1100; the keys are the port's ticks, which run more than a frame
# each while the session's start is made)
run training ",hit_boxes,frame_meter" headless "30:ctraining_start,400:cscene_save,500:cscene_clip,800:cscene_save" &
wait
# (the window's GPU path: one at a time, as the GPU allows)
run guest-gpu ",stage=22" gpu
for name in own guest own-wide guest-wide guest-gpu training; do
    grep -h "kept whole\|scene:.*differently" "$OUT/$name/log.txt" | sed "s/^/checkscene: $name: /"
    python3 "$HERE/scenecheck.py" "$OUT/$name/exports/scenes" | sed "s/^scenecheck:/checkscene: $name:/" | grep -v "all the same$"
    [ "${PIPESTATUS[0]}" = 0 ] || status=1
done
# (the training session's fight: its scenes in parts, with hit boxes of both players)
grep -q '"part-player1"' "$OUT"/training/exports/scenes/*/scene.json 2>/dev/null &&
    grep -q '"overlay-boxes"' "$OUT"/training/exports/scenes/*/scene.json 2>/dev/null ||
    { echo "checkscene: training: no scene in parts with hit boxes"; status=1; }
[ $status = 0 ] && echo "checkscene: all the same" || echo "checkscene: DIFFERENT (runs in $OUT)"
exit $status
