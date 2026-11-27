#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of cps3recomp (sf3comp); from cps2recomp.
#
# Online play (issue #70, port/netplay.h) between two copies of the game on this machine: one hosts, the other
# joins it over localhost, each with made-up play for its own player, every packet sent late (50 ms, give or take 25)
# and one in twenty lost (--net-fake), so that the guesses go wrong and frames are rolled back. The host has every
# feature that changes play on and the joining side none, so the joining side must take the host's (and both turn
# off the cheats). Both write a
# frame's state hash once it has both players' real controls (--hash-log); the two logs must be the same, and
# neither side may stop on a desync. And the host's record of both players' controls (--record), replayed without the
# network, must give the same hashes: each player's controls drove their own player. At the board's pace: FRAMES
# (3000 unless said) take FRAMES / 60 seconds. Each side gives a name and says a line in the chat (--net-name,
# --net-say): both sides must have both names, and each the other's line.
#
# JOIN_ROM: the joining side's set, another region's (game.cfg's other_set): it plays the host's program, whose online_words
# the host sends, so the two logs and the replay (of the host's set) must still be the same.
#
# ARCADE=1: the host plays alone while it waits (--net-arcade) and the other side joins a few seconds later, so that
# the match begins from the game as the host's play left it, sent whole; the replay from the board's reset is not tried.
#
# FIRST_TO=0|3|5|10: the host's Casual, FT3, FT5 or FT10 (online_first_to), the joining side asking for another: both
# must say the host's format and the same score at the same frames.
#
# SWAP=FRAME: the host swaps the sides at FRAME (--net-command FRAME:swap=1, its match menu's Swap sides): both must
# swap them on the same frame, and the logs and the replay of the host's record (by the board's player each played)
# still be the same.
#
# RESELECT=FRAME:FRAME: the host, then the joining side, ask to pick characters again (--net-command FRAME:reselect=1):
# both must start the board again on the same frame (the replay without the events is then not tried).
#
# WORK: where the files go (work/<id>/check unless said).
#
#   [ARCADE=1] [JOIN_ROM=<set>] [FIRST_TO=N] [SWAP=FRAME] [RESELECT=FRAME:FRAME] GAME=<id> check_online.sh [FRAMES]
#   (make GAME=<id> check-online: from the repository's top, after `make`; ROM=your set)
set -eu
GAME=${GAME:-sfiii3}
ROM=${ROM:?the ROM set: ROM=path}
JOIN_ROM=${JOIN_ROM:-$ROM}
frames=${1:-3000}
arcade=${ARCADE:-}
work=${WORK:-work/$GAME/check}
first_to=${FIRST_TO:-}
case "$first_to" in ""|0|3|5|10) ;; *) echo "FIRST_TO must be 0 (Casual), 3, 5 or 10"; exit 1 ;; esac
host_set= join_set=
if [ -n "$first_to" ]; then
    # (online_first_to's choices: 0 Casual, 1 FT3, 2 FT5, 3 FT10; the joining side asks for another, which it doesn't get)
    case "$first_to" in 0) choice=0 ;; 3) choice=1 ;; 5) choice=2 ;; 10) choice=3 ;; esac
    host_set="online_first_to=$choice" join_set="online_first_to=$(( (choice + 1) % 4 ))"
    case "$first_to" in 0) format=Casual ;; *) format=FT$first_to ;; esac
fi
host_commands=() join_commands=()
[ -n "${SWAP:-}" ] && host_commands+=(--net-command "$SWAP:swap=1")
if [ -n "${RESELECT:-}" ]; then
    host_commands+=(--net-command "${RESELECT%%:*}:reselect=1")
    join_commands+=(--net-command "${RESELECT#*:}:reselect=1")
fi
mkdir -p "$work"
: > "$work/online-host.ini"
: > "$work/online-join.ini"
rm -f "$work/online-host.txt" "$work/online-join.txt" "$work/online-host.eeprom" "$work/online-join.eeprom"
# (the features that change play, as tools/check_rollback.sh has them, but for the custom colours: each player's own)
play=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '($2=="Game"||$2=="Cheats"||$2=="Display") && NF==5 &&
    $1 !~ /^(display_|window_size|run_ahead|custom_soundtrack|widescreen_walls|custom_colours)/ {print $1}' | paste -sd, -)
# (the match has them but for the cheats, which are off on both sides: the replay is given the same)
cheats=$("build/$GAME/$GAME" --list-features 2>&1 | awk -F'\t' '$2=="Cheats" {print $1}')
match=$(printf '%s\n' "$play" | tr ',' '\n' | grep -vxF "$cheats" | paste -sd, -)
port=$((7900 + RANDOM % 1000))
side() { # host | join, then the program's arguments
    name=$1; shift
    rom=$ROM; [ "$name" = join ] && rom=$JOIN_ROM
    "build/$GAME/$GAME" --rom "$rom" --headless --frames "$frames" --settings "$work/online-$name.ini" \
        --eeprom "$work/online-$name.eeprom" --hash-log "$work/online-$name.txt" --net-fake 50:25:5 --net-service off "$@" 2> "$work/online-$name.err"
}
side host --auto 11:0:1 --features "$play${host_set:+,$host_set}" ${host_commands[@]+"${host_commands[@]}"} --host "$port" --record "$work/online-host.mov" ${arcade:+--net-arcade} \
    --net-name "Host P1" --net-say "hello from the host" &
host=$!
if [ -n "$arcade" ]; then sleep 4; else sleep 0.5; fi
status=0
side join --auto 12:0:1 ${join_set:+--features "$join_set"} ${join_commands[@]+"${join_commands[@]}"} --join "127.0.0.1:$port" --net-name "Joiner" --net-say "good luck!" || { echo "FAILED: the joining side stopped"; tail -3 "$work/online-join.err"; status=1; }
wait "$host" || { echo "FAILED: the host stopped"; tail -3 "$work/online-host.err"; status=1; }
# (each side's log ends where it last had the other's controls: the shorter is what both have)
lines=$(wc -l < "$work/online-host.txt" | tr -d ' ')
joined=$(wc -l < "$work/online-join.txt" | tr -d ' ')
[ "$joined" -lt "$lines" ] && lines=$joined
if [ "$lines" -lt $((frames - 30)) ]; then
    echo "FAILED: only $lines frames had both players' controls"
    status=1
elif cmp -s <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-join.txt"); then
    echo "ok: the two sides the same over $lines frames, with $play"
else
    echo "FAILED: the two sides differ from frame $(diff <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-join.txt") | grep '^<' | head -1 | cut -d' ' -f2)"
    status=1
fi
# (each side knows both names, and has the other's line of the chat: sent straight to it, through the lost packets)
for name in host join; do
    if ! grep -q '^online: player 1 Host P1, player 2 Joiner$' "$work/online-$name.err"; then
        echo "FAILED: the $name doesn't have both players' names"
        status=1
    fi
done
if grep -q '^online: chat: Joiner: good luck!$' "$work/online-host.err" && grep -q '^online: chat: Host P1: hello from the host$' "$work/online-join.err"; then
    echo "ok: the names and the chat came across"
else
    echo "FAILED: the chat didn't come across"
    status=1
fi
if [ -n "$first_to" ]; then
    # (the host's format on both sides, and the same games counted at the same frames)
    for name in host join; do
        grep -q "^online: set format $format\$" "$work/online-$name.err" || { echo "FAILED: the $name's set isn't the host's $format"; status=1; }
        grep '^online: set ' "$work/online-$name.err" | grep -v '^online: set format' > "$work/online-$name.set" || true
    done
    if cmp -s "$work/online-host.set" "$work/online-join.set"; then
        echo "ok: both sides have the host's $format, and the same $(wc -l < "$work/online-host.set" | tr -d ' ') games counted at the same frames"
    else
        echo "FAILED: the two sides counted the set differently"
        status=1
    fi
fi
if [ -n "${SWAP:-}" ]; then
    swapped=$(grep '^online: sides swapped at frame' "$work/online-host.err" || true)
    if [ -n "$swapped" ] && [ "$swapped" = "$(grep '^online: sides swapped at frame' "$work/online-join.err" || true)" ]; then
        echo "ok: both sides swapped the sides on the same frame (${swapped#online: sides swapped at })"
    else
        echo "FAILED: the sides weren't swapped on the same frame on both"
        status=1
    fi
fi
if [ -n "${RESELECT:-}" ]; then
    again=$(grep '^online: both asked to pick characters again' "$work/online-host.err" | tail -1 || true)
    if [ -n "$again" ] && [ "$again" = "$(grep '^online: both asked to pick characters again' "$work/online-join.err" | tail -1 || true)" ]; then
        echo "ok: both sides started the board again for character select on the same frame (${again##* at })"
    else
        echo "FAILED: the board wasn't started again on the same frame on both"
        status=1
    fi
    for name in host join; do
        echo "$name: $(grep '^online: frame' "$work/online-$name.err" | tail -1 | sed 's/^online: //')"
    done
    exit $status
fi
if [ -n "$arcade" ]; then
    # (the host played before the other side came: the match began from there)
    played=$(sed -n 's/^online: the match begins after \([0-9]*\) frames of the host.s play.*/\1/p' "$work/online-host.err")
    if [ "${played:-0}" -lt 120 ]; then
        echo "FAILED: the host had not played while it waited (${played:-no} frames)"
        status=1
    else
        echo "ok: the match began after $played frames of the host's own play"
    fi
    for name in host join; do
        echo "$name: $(grep '^online: frame' "$work/online-$name.err" | tail -1 | sed 's/^online: //')"
    done
    exit $status
fi
# And the match is what the game does with those controls: both players' (--record), replayed without the network
: > "$work/online-replay.ini"
"build/$GAME/$GAME" --rom "$ROM" --headless --frames "$lines" --settings "$work/online-replay.ini" --features "$match" \
    --movie "$work/online-host.mov" --hash-log "$work/online-replay.txt" --hash-no-picture --hash-no-sound 2> /dev/null
if cmp -s <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-replay.txt"); then
    echo "ok: the match the same as both players' controls replayed without the network"
else
    echo "FAILED: the match and its replay differ from frame $(diff <(head -n "$lines" "$work/online-host.txt") <(head -n "$lines" "$work/online-replay.txt") | grep '^<' | head -1 | cut -d' ' -f2)"
    status=1
fi
for name in host join; do
    echo "$name: $(grep '^online: frame' "$work/online-$name.err" | tail -1 | sed 's/^online: //')"
done
exit $status
