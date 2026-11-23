# This fork

A personal fork of [tmcleroy/sf3comp](https://github.com/tmcleroy/sf3comp). `main` is upstream's `main` with
a short stack of fork commits on top. The ROM set and everything derived from it (`work/`, `build/`) stay local.

## Patches

Every fork change to an upstream file, one commit each. Keep this table current: it is what a rebase conflict is
resolved against.

| Change | Files | Why |
|---|---|---|
| Pause and frame step on Cmd+P / Cmd+O; the keyboard is not read as game input while Cmd is held | `cps3recomp/port/frontend_sdl.cpp` | Plain P and O are free for a button layout; Cmd+O does not also press O's button |

Fork-only files (never conflict): `FORK.md`, `CLAUDE.md`, `fork/`.

## Rules

- **Small patches.** Touch upstream files as little as the change needs. A feature of your own goes in new files with
  a minimal hook into upstream code.
- **Upstream first** for anything generally useful (remappable shortcuts, say): send it as a PR to tmcleroy/sf3comp,
  then drop the fork's commit once it lands.
- **Keep out of `cps3recomp/recomp-common/`**: it is a copy synced from another repository
  (`cps3recomp/tools/sync-common.sh`), and a change there is overwritten.
- One commit per change, and a row in Patches for it.

## Build

```sh
fork/build.sh /path/to/sfiii3nr1.zip    # translate, build, bundle with the fork icon -> build/collection/SF3comp.app
```

App icon: put your own picture at `fork/icon/custom.png` (or `.jpg`) and rebuild. It is git-ignored, so it never
reaches the public repository; without it the build uses `fork/icon/icon.svg`.

Needs Xcode's command-line tools and `brew install cmake ninja sdl3` (SDL 3.4.16+). Upstream's own steps are in
`docs/BUILDING.md`.

## Update from upstream

Upstream moves fast. Update when a release has something you want, not on every commit.

```sh
fork/update.sh                          # fetch, backup tag, rebase the fork commits onto upstream/main
fork/build.sh /path/to/sfiii3nr1.zip    # rebuild and play-test
git push --force-with-lease origin main
```

`main` is rewritten by each update, so it is force-pushed; `fork-backup/*` tags (local) are the undo.

## Checks

Upstream's CI runs on this fork's pull requests and on pushes to `main` (free: the fork is public). It builds without
game code, so it cannot test gameplay. Before pushing, run `tools/audit.sh` and
`bash cps3recomp/tools/sync-common.sh --check` locally, and play-test the build.

## Remotes

`origin` is this fork (qQueproHD/sf3comp), `upstream` is tmcleroy/sf3comp. An upstream PR comes from a branch made off
`upstream/main` that carries only the change being sent, never the fork's own commits.
