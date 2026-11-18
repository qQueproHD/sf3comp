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
| Display | Scaling, picture shape (4:3 or square pixels), RetroArch shaders, zoom on a fighter or both, vsync, window size |
| Sound | Custom soundtrack (your own files, by song or as a playlist) |
| Video | Record video (Alt+F12), video size, show the videos folder |
| Online | Ping and input delay shown, names hidden |
| Display | Input lag readout |
| Replays | Keep my matches (the Replays page lists them to watch and to play on from) |

The table is the definitive version's (3rd Strike, 990512). The launcher's other games (New Generation, 2nd Impact,
3rd Strike 990608 and 4rd Strike) have the shared part of it: free play, straight to the title, the board's settings,
cheats, training mode and the dummy, widescreen, online play, replays, run-ahead and the input lag readout.

How each part works and what it was checked against: [games/sfiii3/PROGRESS.md](../games/sfiii3/PROGRESS.md),
[games/sfiii3/DEFINITIVE.md](../games/sfiii3/DEFINITIVE.md) (the stages), [docs/TIMING.md](TIMING.md) (arcade timing).

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
