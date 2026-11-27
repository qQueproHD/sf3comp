# Features

Every feature is a switch of its own, on the launcher's Features page and in the in-game menu. **Pure** plays exactly
as the board does; **Enhanced** (the default) adds free play and straight-to-the-title. `sfiii3 --list-features`
prints them all with their descriptions. Online, the host's choices of the ones that change play are the match's.

| Group | Features |
| --- | --- |
| Game | Widescreen (stage drawn out, or the board's own walls), Gill playable, free play, straight to the title, fight music from any stage, the definitive version's stages (New Generation's and 2nd Impact's, with their sets) and load ahead, arcade timing, run-ahead, rapid fire |
| Cheats | No damage, no stun (each player), one hit knocks out, full super art gauge, infinite time |
| Training mode | Stage, hit boxes, input display, frame data and its log, frame meter, combo, damage, stun and parry readouts, health and gauge refill, start over |
| Dummy | Stand, crouch or jump; guard (always, after a hit, at random) and what it does after; parries, escapes throws, gets up quickly, mashes out of stun, reversals; record and replay (slots); played by the game at a chosen level |
| Alternative modes | Survival, endless battle, ring out, tag team battle (tag, knock-out, co-op and 2 vs 2), bonus games, stage viewer, model viewer, colour editor, attract mode |
| Board | Difficulty, damage, timer speed, rounds, event, bonus games, continue, demo sound, sound output (stereo or the cabinet's mono); the board's test menu |
| Display | Scaling, picture shape (4:3 or square pixels), RetroArch shaders (and your own presets, from one shaders folder every game shares, chosen by name), zoom on a fighter or both, vsync, window size |
| Sound | Custom soundtrack (your own files, by song or as a playlist) |
| Screenshots | F12, as PNG; SVG pictures too |
| Scene export | Save scene (Shift+F12), record clip (Ctrl+F12) and its length, show the scenes folder |
| Video | Record video (Alt+F12), video size, save replay (Alt+Shift+F12) and its length, show the videos folder |
| Online | Ping and input delay shown, names hidden |
| Display | Input lag readout, FPS counter |
| Replays | Keep my matches (the Replays page lists them to watch and to play on from) |

The table is the definitive version's (3rd Strike, 990512). The launcher's other games (New Generation, 2nd Impact,
3rd Strike 990608 and 4rd Strike) have the shared part of it: free play, straight to the title, the board's settings,
cheats, training mode and the dummy, widescreen, online play, replays, run-ahead and the input lag readout.

How each part works and what it was checked against: [games/sfiii3/PROGRESS.md](../games/sfiii3/PROGRESS.md),
[games/sfiii3/DEFINITIVE.md](../games/sfiii3/DEFINITIVE.md) (the stages), [docs/TIMING.md](TIMING.md) (arcade timing).

## Screenshots

F12 saves the picture as the window shows it, at the board's size (384 by 224, wider in widescreen), with the readouts
on it but not the camera's zoom, to `screenshots/` in the game's folder, as `<game> <date> <time>.png`. Where it went is
said at the bottom for a few seconds.

**SVG pictures too** (the Game page's `export_svg`, cps2recomp's) writes an SVG beside each screenshot's PNG, and a
scene's `scene.svg` (below). Each of the
board's pixels is a square of the SVG's grid, so it scales to any size exactly: what a row has of a colour side by side is
one rectangle, made taller where the rows below have the same, each colour's rectangles one path, and the picture's
commonest colour one rectangle behind it all (`image_files::WriteSvg`, cps2recomp's `model/image_files.h`). It is shown
three times the board's lines, its pixels in the Display page's shape (`port::SvgSize`), and drawn with
`shape-rendering="crispEdges"`. Drawn, it is the PNG pixel for pixel.

## Video recording

The in-game menu's **Game > Video** page, every game (`cps3recomp/port/video_record.h`, cps2recomp's). **Record video**
(Alt+F12) records what is played and heard as an MP4 in `videos/` in the game's folder, as `<game> <date> <time>.mp4`,
until the row (now **Stop recording video**) or Alt+F12 again; quitting the game finishes it too. The picture is as
screenshots (F12) take it, readouts and all, without the port's lines at the bottom (the speed, the zoom's label, what
was saved: the window says **RECORDING VIDEO** and the time all the while, the video doesn't). The sound is the board's
as heard and the custom soundtrack's, mixed at 48 kHz.

The picture is made larger with each of the board's pixels a sharp block (only the edges between blocks blended, as
the Display page's Sharp scaling does), in the shape the Display page shows it in (4:3, or square pixels); **Video
size** chooses 3 times the board's lines (672, the default), 2, 4 or 1080 lines. The size is the game's widest picture's
(its widescreen), fixed for the file: a narrower picture has black beside it. The video's time is the game's: a frame
for each of the board's frames run, so stopped or in the menu nothing is recorded and slowed or sped up it plays at the
board's speed.

It is H.264 (Constrained Baseline, BT.709 said in the stream, a key frame every two seconds) and AAC-LC at 48 kHz, which
every player, browser and phone plays: minih264 and minimp4 in `cps3recomp/third_party/`, FAAC downloaded at build
time. The encoding is done on a thread of its own: should it fall behind (a slow machine at a large size), a picture is
left out and the one before shown longer, the sound all kept, rather than the game held up. `--record-video FILE.mp4`
records a whole run, from its first frame to its last, headless or not; headless (or `--unthrottled`) nothing is left
out, the game waiting for the encoder instead, so `--movie M.txt --headless --record-video M.mp4` makes a movie's play
into a video faster than it plays.

**Save replay** (Alt+Shift+F12, the same page) saves what was just played as a video, with no recording started: the
last 30 seconds, or 60 or 10 (**Replay length**; Off keeps nothing). All the while the port keeps the game's state once a
second and every frame's controls (`Port::ReplayKeep`, before each frame runs). A CPS-3 state is about 10 MB, most of
it the board's 8 MB of character RAM, and little of it changes in a second: each state kept shares its unchanged 64 KB
blocks with the one before (`Port::InstantState`), so 30 seconds is about 36 MB. On a New Generation or 2nd Impact
stage the stage board, which a save state does not hold, is kept beside each state the same way (about 60 MB).

Asked for, the frames are run again from the oldest state with those controls, in place of the port's own machine and
game, which are put back as they were after each tick (as run-ahead's frames are; the picture, the controls and the
switches kept by hand, as for a spare), and the stage board stepped as play steps it (`Port::ReplayMake`). They are run
in the time the window's tick has to spare before its next frame, a little over half of it, and only when that is more
than a swap of the machines and a frame have been taking, so that the game goes on at its speed; on a fast machine a
30-second replay is made in a few seconds. At each state kept the replay is put back to it, so that anything a frame
run again does differently lasts a second at most. `games/sfiii3/checkreplay.sh` checks it: on 3rd Strike's own stage
and on a guest stage, every picture and every frame's sound of the replay is the one the play had, and the play with a
replay made is the play without one. The custom soundtrack is not in it (its player is the port's, not the game's),
and online play keeps nothing.

## Scene export

The picture taken apart (`cps3recomp/port/scene.h`, cps2recomp's, with the same files and `scene.json`; the in-game
menu's **Game > Scene export** page, every game). **Save scene** (Shift+F12) writes a folder in `exports/scenes/` beside
the saves: `picture.png`, the picture as the board drew it, and each of its layers by itself, with nothing where the
layer has nothing. A CPS-3 has no fixed layers: its picture is the frame's object list drawn in its order, with the
board's four tilemaps among the objects, and the text layer over all of it. So each run of the list that is one
tilemap's lines is a layer (`tilemap1` to `tilemap4`; in widescreen `tilemap1-ends` and so on, a far layer's ends filled
in the added columns), each run of objects between them another (`objects-0` on: how many tilemap runs are behind
them; the objects hold the fighters, the panel and parts of stages), and the text layer `text`. A fight on New
Generation's or 2nd Impact's stage has that stage's layers too (`stage-...`), behind the game's, and its pieces in front
of the fighters in front of them. `scene.json` lists the layers back to front; laid over each other on the backdrop
colour in that order they make `picture.png` again.

Each layer is the frame drawn again from its records (`native::Rasteriser::Split`, from the frame the picture was drawn
from: `Machine::ShownFrame`), whole, what the layers in front of it cover too. The board's shadows and highlights are
blends: they darken or lighten what is already drawn under them. Each is in the layer it falls on (every layer is drawn
with every blend of the frame, as the board would draw them over it), so that the layers put back together make the
picture exactly, and a fighter's shadow stays on the floor in the diorama rather than floating as a layer of its own.
Every scene is put back together as it is taken and checked against the picture: one the layers do not make again, or
one the game made of more than one picture (a stage fading or sliding in, the ring out's way down), is saved whole, as
the one layer `whole`. **Record clip** (Ctrl+F12) does the same for every frame of 3, 5 or 10 seconds (**Clip length**),
each layer's frames in a folder of their own (`objects-0/0001.png`...), with each frame's layers in `scene.json`; the
files are written by threads beside the game. With **SVG pictures too** (Game page), a scene also has `scene.svg`: the
backdrop and each layer back to front, each a group (an Inkscape layer).

In a round (every game), a scene and each frame of a clip are split by what is in them as well (`parts` in
`scene.json`, cps2recomp's): `part-stage-behind`, `part-other` (shadows, sparks, projectiles, the round's banners),
`part-player1` and `part-player2` (in front the one the picture shows in front where they are over each other), `part-stage-front` (a stage's pieces in front of the
fighters: Remy's stage's bollard) and `part-panel` (the text layer: the life bars, the timer, the meters). A fighter is
told by its objects (6 bits a pixel, a palette below 0x20, not a blend), and whose by its palette: the palette DMA puts
player 1's at colours 0x000-0x3FF and player 2's from 0x400. Each part is whole where another covers it, and laid over
each other they too make the picture (but where a spark or a projectile is in front of a fighter, or the two fighters are drawn among each other's,
a throw's: the parts' order is fixed, `part-other` behind the players, one player over the other). A saved scene also has each object by itself (`entries` in `scene.json`, their
pictures side by side in `entries.png`, and `entry-map.png`, which object is in front at each pixel): a CPS-3 object is
a tile or a few, so an object here is a head of the board's list, all its tiles (a fighter's whole picture, a tree), drawn by itself
with any shadow that falls on it, as its layer has it.
In a training session with hit boxes or a readout on, `overlay-boxes.png` and `overlay-hud.png` hold what the port
draws over the picture, and `scene.json` lists the hit boxes, each with its player. The diorama shows the parts by
default, lets you pick out an object, and puts each player's hit boxes just in front of them.

`games/sfiii3/checkscene.sh` checks it: at the title and in a fight, on 3rd Strike's own stage and on New Generation's,
in 4:3 and widescreen, headless and through the window's GPU path, every scene's layers and a clip's every frame put
back together make the picture pixel for pixel, and so do a fight's parts (`part-other` behind the players or in
front of them, either player in front); with run-ahead too; each object by itself is its layer where it
is in front; and a training session's scenes have the overlays and both players' hit boxes.

**The scene diorama** (`tools/diorama/diorama.html`, cps2recomp's, unchanged) shows what these save as cut-outs at their
own depths: the launcher's **Open diorama** (Games page, Tools) opens it in the browser, as does opening the file
itself; then open a saved folder (or drop it on the page). Turn it, pull the layers apart, hide or solo them, take a
photo or record a video, save it as a 3D model (glTF). It loads three.js from jsDelivr; nothing leaves the browser.
