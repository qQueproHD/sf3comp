#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Fork-only. Replays the fork's commits on top of the latest upstream main.
#   fork/update.sh           (on main, with a clean working tree)
# Leaves a local backup tag (fork-backup/<time>) at the old main, so `git reset --hard fork-backup/<time>` undoes it.
set -eu
cd "$(dirname "$0")/.."
[ "$(git rev-parse --abbrev-ref HEAD)" = main ] || { echo "fork/update.sh: check out main first" >&2; exit 1; }
[ -z "$(git status --porcelain)" ] || { echo "fork/update.sh: commit or stash your changes first" >&2; exit 1; }

git fetch upstream
backup=fork-backup/$(date +%Y%m%d-%H%M%S)
git tag "$backup"
echo "backup: $backup"
echo "upstream commits coming in: $(git rev-list --count HEAD..upstream/main)"
if ! git rebase upstream/main; then
    echo "fork/update.sh: conflict. Resolve it keeping the fork's intent (FORK.md, Patches), then" >&2
    echo "  git add <files> && git rebase --continue      or      git rebase --abort" >&2
    exit 1
fi
echo "fork commits now on top of upstream:"
git log --oneline upstream/main..HEAD
echo "next: fork/build.sh, play-test, then git push --force-with-lease origin main"
