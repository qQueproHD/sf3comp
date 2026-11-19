#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# The scene export's check (checkscene.sh runs it): each scene folder under the folders given (exports/scenes/*), its
# layers laid over each other on its backdrop in scene.json's order, against its picture.png: the same, pixel for pixel;
# a clip's, each frame's (frameList: its layers, from their folders) against picture/NNNN.png. A scene kept whole (the one
# layer "whole") is wrong here, where every frame saved is drawn as one frame. A fight's parts (scene.json "parts", each
# frame's of a clip) laid over each other on the backdrop the same (part-other behind the players or in front); each object by itself (a scene's entries, in
# entries.png), where the entry map says it is the object in front, the same as its layer there; and the training
# overlays' hit boxes, when there are any, each with its player. Prints each scene's layers and what it found, and
# "scenecheck: all the same" or what is wrong; exit 0 when all is well.
#
#   scenecheck.py FOLDER...
import glob
import json
import os
import struct
import sys
import zlib


def read_png(path):
    """(width, height, pixels as a list of RGBA tuples' bytes: 4 a pixel)"""
    data = open(path, 'rb').read()
    at, idat, width, height, kind = 8, b'', 0, 0, 2
    while at < len(data):
        length, name = struct.unpack('>I4s', data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        if name == b'IHDR':
            width, height, _, kind = struct.unpack('>IIBB', body[:10])
        elif name == b'IDAT':
            idat += body
        at += 12 + length
    raw = zlib.decompress(idat)
    step = 4 if kind == 6 else 3
    stride = width * step
    out, previous = bytearray(), bytearray(stride)
    for y in range(height):
        row = raw[y * (stride + 1):(y + 1) * (stride + 1)]
        f, line = row[0], bytearray(row[1:])
        if f:
            for x in range(stride):
                a = line[x - step] if x >= step else 0
                b = previous[x]
                c = previous[x - step] if x >= step else 0
                if f == 1:
                    line[x] = (line[x] + a) & 255
                elif f == 2:
                    line[x] = (line[x] + b) & 255
                elif f == 3:
                    line[x] = (line[x] + (a + b) // 2) & 255
                elif f == 4:
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        previous = line
        if step == 3:
            for x in range(width):
                out += line[x * 3:x * 3 + 3] + b'\xff'
        else:
            out += line
    return width, height, bytes(out)


def compose(width, height, backdrop, files):
    r, g, b = int(backdrop[1:3], 16), int(backdrop[3:5], 16), int(backdrop[5:7], 16)
    made = bytearray(bytes((r, g, b, 255)) * (width * height))
    for path in files:
        w, h, pixels = read_png(path)
        if (w, h) != (width, height):
            return None, f'{os.path.basename(path)} is {w}x{h}, not {width}x{height}'
        for i in range(3, len(pixels), 4):
            if pixels[i]:
                made[i - 3:i + 1] = pixels[i - 3:i + 1]
    return bytes(made), None


def compare(made, picture, width):
    if made == picture:
        return 0, None
    different = [i // 4 for i in range(0, len(made), 4) if made[i:i + 3] != picture[i:i + 3]]
    return len(different), (different[0] % width, different[0] // width) if different else None


def check_entries(folder, doc, width, height):
    """(what is wrong with a still's objects each by itself, or None; how many pixels of the map were compared)"""
    entries = doc.get('entries')
    if not entries:
        return None, 0
    layers = {l['name']: read_png(os.path.join(folder, l['file']))[2] for l in doc['layers'] if l.get('file')}
    atlas_width, _, atlas = read_png(os.path.join(folder, doc['entryAtlas']))
    w, h, front = read_png(os.path.join(folder, doc['entryMap']))
    if (w, h) != (width, height):
        return f'the entry map is {w}x{h}', 0
    compared = 0
    for i in range(width * height):
        if not front[i * 4 + 3]:
            continue
        n = front[i * 4] | front[i * 4 + 1] << 8
        if not 1 <= n <= len(entries):
            return f'the entry map has entry {n} at {(i % width, i // width)}', compared
        e = entries[n - 1]
        x, y = i % width - e['box'][0], i // width - e['box'][1]
        if not (0 <= x < e['box'][2] and 0 <= y < e['box'][3]):
            return f'entry {n} is outside its box at {(i % width, i // width)}', compared
        j = ((e['atlas'][1] + y) * atlas_width + e['atlas'][0] + x) * 4
        layer = layers.get(e['layer'])
        if layer is None or atlas[j:j + 4] != layer[i * 4:i * 4 + 4]:
            return f'entry {n} ({e["layer"]}) is not its layer at {(i % width, i // width)}', compared
        compared += 1
    return None, compared


def check_parts(folder, width, height, backdrop, parts, number, picture):
    """(how many pixels the parts make differently, the first; what is wrong with their files): laid over each other in
    their order, and with part-other over the players instead (a spark, a projectile in front of a fighter: the parts'
    order is fixed, the picture's is not); a pixel either makes as the picture has it is the same"""
    def files(names):
        return [os.path.join(folder, n, number) if number else os.path.join(folder, n + '.png') for n in names]
    made, why = compose(width, height, backdrop, files(parts))
    if made is None:
        return None, None, why
    later = [n for n in parts if n != 'part-other']
    if 'part-other' in parts:
        at = max([i for i, n in enumerate(later) if n.startswith('part-player')], default=len(later) - 1) + 1
        later.insert(at, 'part-other')
    again, why = compose(width, height, backdrop, files(later))
    if again is None:
        return None, None, why
    different = [i // 4 for i in range(0, len(made), 4) if made[i:i + 3] != picture[i:i + 3] and again[i:i + 3] != picture[i:i + 3]]
    return len(different), (different[0] % width, different[0] // width) if different else None, None


def check_boxes(doc, boxes):
    """(what is wrong with the hit boxes, or None)"""
    names = [o if isinstance(o, str) else o['name'] for o in doc.get('overlays') or []]
    if boxes and 'overlay-boxes' not in names:
        return 'hit boxes but no overlay-boxes'
    if any(b.get('player') not in (1, 2, None) for b in boxes):
        return 'a hit box of no player 1 or 2'
    return None


bad = 0
scenes = sorted({os.path.dirname(p) for root in sys.argv[1:] for p in glob.glob(os.path.join(root, '**', 'scene.json'), recursive=True)})
if not scenes:
    print('scenecheck: no scenes found')
    sys.exit(1)
for folder in scenes:
    doc = json.load(open(os.path.join(folder, 'scene.json')))
    name = os.path.basename(folder)
    width, height = doc['width'], doc['height']
    layers = doc['layers']
    names = [l['name'] for l in layers]
    if names == ['whole']:
        print(f'scenecheck: {name}: kept whole (not taken apart)')
        bad += 1
        continue
    if doc['kind'] == 'scene':
        files = [os.path.join(folder, l['file']) for l in layers if l.get('file')]
        made, why = compose(width, height, doc['backdrop'], files)
        w, h, picture = read_png(os.path.join(folder, doc['picture']))
        n, first = (None, None) if made is None else compare(made, picture, width)
        status = why or ('the same' if n == 0 else f'{n} pixels different, the first at {first}')
        svg = ' (scene.svg)' if os.path.exists(os.path.join(folder, 'scene.svg')) else ''
        print(f'scenecheck: {name}: {width}x{height}, {len(layers)} layers {", ".join(names)}{svg}: {status}')
        bad += 0 if n == 0 else 1
        if doc.get('parts'):
            parts = [p['name'] for p in doc['parts'] if p.get('file')]
            n, first, why = check_parts(folder, width, height, doc['backdrop'], parts, None, picture)
            status = why or ('the same' if n == 0 else f'{n} pixels different, the first at {first}')
            print(f'scenecheck: {name}: parts {", ".join(p["name"] for p in doc["parts"] if p.get("file"))}: {status}')
            bad += 0 if n == 0 else 1
        why, compared = check_entries(folder, doc, width, height)
        if doc.get('entries'):
            print(f'scenecheck: {name}: {len(doc["entries"])} objects each by itself: '
                  + (why or f'each the same as its layer where it is in front ({compared} pixels)'))
            bad += 1 if why else 0
        boxes = doc.get('boxes') or []
        why = check_boxes(doc, boxes)
        if boxes or why:
            print(f'scenecheck: {name}: {len(boxes)} hit boxes, players {sorted({b.get("player") or 0 for b in boxes})}: ' + (why or 'with the overlays'))
            bad += 1 if why else 0
    else:
        frames = doc['frameList']
        wrong = wrong_parts = parted = boxed = 0
        for frame in frames:
            number = f"{frame['number']:04d}.png"
            files = [os.path.join(folder, layer, number) for layer in frame['layers']]
            made, why = compose(width, height, frame['backdrop'], files)
            w, h, picture = read_png(os.path.join(folder, doc['picture'], number))
            if made is None or compare(made, picture, width)[0]:
                wrong += 1
                if wrong == 1:
                    print(f'scenecheck: {name}: frame {frame["number"]}: ' + (why or 'different'))
            if frame.get('parts'):
                parted += 1
                n, first, why = check_parts(folder, width, height, frame['backdrop'], frame['parts'], number, picture)
                if why or n:
                    wrong_parts += 1
                    if wrong_parts == 1:
                        print(f'scenecheck: {name}: frame {frame["number"]}: its parts ' + (why or 'different'))
            why = check_boxes(frame, frame.get('boxes') or [])
            boxed += 1 if frame.get('boxes') else 0
            if why:
                wrong += 1
                print(f'scenecheck: {name}: frame {frame["number"]}: {why}')
        print(f'scenecheck: {name}: a clip of {len(frames)} frames, {len(layers)} layers {", ".join(names)}: '
              + ('every frame the same' if not wrong else f'{wrong} frames different')
              + (f'; {parted} frames in parts, ' + ('every one the same' if not wrong_parts else f'{wrong_parts} different') if parted else '')
              + (f'; {boxed} with hit boxes' if boxed else ''))
        bad += 1 if wrong or wrong_parts or not frames else 0
print('scenecheck: all the same' if not bad else f'scenecheck: {bad} scenes DIFFERENT')
sys.exit(1 if bad else 0)
