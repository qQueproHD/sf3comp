# Dreamcast arranged music

3rd Strike can play the Dreamcast version's arranged stage themes in place of the arcade's, from your own copy of
that disc. Nothing from the disc comes with SF3comp: you supply the disc image, as you supply the ROM sets.

## What you need

An image of **Street Fighter III: 3rd Strike for the Dreamcast**, in any of these forms:

- a Redump-style `.cue` with its `.bin` tracks,
- a `.gdi` with its tracks,
- the `.zip` of either, exactly as downloaded (it doesn't need unpacking).

CDI images are not read (they are usually re-compressed and missing parts of the disc).

## Setting it up

1. Put the disc image **beside your 3rd Strike ROM set** (the folder that holds `sfiii3nr1.zip`). It is also found in
   the custom soundtrack folder chosen on the launcher, or beside the game's settings.
2. On the launcher's Features page (3rd Strike, **Sound**), or in the in-game menu's Sound page, set
   **Custom soundtrack** to **Dreamcast arranged**.
3. Start a fight. The first time, the arranged tracks are copied off the disc into a `dreamcast` folder beside the
   game's settings (about 60 MB, a few seconds). After that the disc image isn't needed, and they just play.

If it doesn't work, the arcade music plays and the in-game menu says why the next time it opens: no disc image found,
or which image was found and what was wrong with it (most often its track files aren't in the same folder as the
`.cue` or `.gdi`, which is only an index of them). The game's log (launcher → Show the log) has the same, on lines
starting `dreamcast:`. A `dreamcast` folder with 16 `.ADX` files beside the game's settings means the copy worked.

## What changes

- **Every stage theme** is the Dreamcast arrangement, looping where the Dreamcast loops it. The arcade has two songs
  for a stage (the second comes later in the match); the arrangement plays for both.
- **Everything else** (title, character select, versus, winner and so on) stays the arcade's own music.

## Guest stages

A fight on a **New Generation or 2nd Impact stage** plays the Dreamcast arrangement too: with a custom soundtrack on,
the guest stage's own music is left out, and the theme is that of the 3rd Strike stage the fight is on underneath (the
one the game chose, or the one **Fight music** picks).

## Trying it out

In a fight on a 3rd Strike stage, switch Custom soundtrack between **Off** (the arcade's) and **Dreamcast arranged**
in the in-game menu: the music changes at once.

## How the tracks were matched

The disc holds three files for each stage (`SOUND/NN_A_…`, `NN_B_…`, `NN_C_…` in the `.ADX` format). Comparing them
with recordings of the arcade's own music showed that **A** is the arcade's first song for that stage, **B** its second,
and **C** the arrangement, which is what this plays. The table is in `games/sfiii3/src/music.cpp`. Urien's theme
(`MEX`) was matched less surely than the others: if it sounds like another character's, please open an issue.
