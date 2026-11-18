#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy
#
# The scene export's check (checkscene.sh runs it): each scene folder under the folders given (exports/scenes/*), its
# layers laid over each other on its backdrop in scene.json's order, against its picture.png: the same, pixel for pixel;
# a clip's, each frame's (frameList: its layers, from their folders) against picture/NNNN.png. A scene kept whole (the one
# layer "whole") is wrong here, where every frame saved is drawn as one frame. Prints each scene's layers and what it
# found, and "scenecheck: all the same" or what is wrong; exit 0 when all is well.
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
    else:
        frames = doc['frameList']
        wrong = 0
        for frame in frames:
            number = f"{frame['number']:04d}.png"
            files = [os.path.join(folder, layer, number) for layer in frame['layers']]
            made, why = compose(width, height, frame['backdrop'], files)
            w, h, picture = read_png(os.path.join(folder, doc['picture'], number))
            if made is None or compare(made, picture, width)[0]:
                wrong += 1
                if wrong == 1:
                    print(f'scenecheck: {name}: frame {frame["number"]}: ' + (why or 'different'))
        print(f'scenecheck: {name}: a clip of {len(frames)} frames, {len(layers)} layers {", ".join(names)}: '
              + ('every frame the same' if not wrong else f'{wrong} frames different'))
        bad += 1 if wrong or not frames else 0
print('scenecheck: all the same' if not bad else f'scenecheck: {bad} scenes DIFFERENT')
sys.exit(1 if bad else 0)
