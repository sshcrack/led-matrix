#!/usr/bin/env bash
#
# cleanup_worktree.sh: free the disk space of a settled worktree.
#
# A full build tree is several GB (emulator ~4 GB, cross-compile ~8 GB,
# desktop ~4 GB) and agents leave one in every short-lived worktree. Once a
# thread is settled, the build dirs are only a cache: ccache and the vcpkg
# binary cache make `scripts/setup_worktree.sh --build` cheap, so deleting them
# loses little.
#
# Removes (all git-ignored, so no source or history is touched):
#   build trees      build/ emulator_build/ desktop_build*/ emulator_*san_build/
#                    desktop_*san_build/ cmake-build-*/
#   web outputs      react-web/node_modules/ react-web/dist/
# Keeps: .env, scene_previews/, and everything tracked or untracked-but-not-ignored.
#
# Usage: scripts/cleanup_worktree.sh [--dry-run] [--force]
#   --dry-run   only print what would be removed
#   --force     allow running in the main checkout (refused by default: that is
#               the owner's own build, not a throwaway worktree)
# Idempotent: missing directories are skipped.
set -euo pipefail

DRY=0 FORCE=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY=1; shift ;;
        --force) FORCE=1; shift ;;
        -h|--help) sed -n '2,21p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"
main="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")"
say() { printf '\033[1;36m[cleanup]\033[0m %s\n' "$*"; }

if [[ "$main" == "$repo" && "$FORCE" != 1 ]]; then
    say "this is the main checkout; refusing (use --force to clean it anyway)"
    exit 0
fi

shopt -s nullglob
targets=(
    build emulator_build desktop_build desktop_build_*
    emulator_asan_build emulator_tsan_build desktop_asan_build desktop_tsan_build
    cmake-build-*
    react-web/node_modules react-web/dist
)

freed=0
for d in "${targets[@]}"; do
    [[ -d "$d" ]] || continue
    # Only delete what git ignores, so a tracked directory is never removed.
    if ! git check-ignore -q -- "$d" && ! git check-ignore -q -- "$d/"; then
        say "skip $d (not git-ignored)"
        continue
    fi
    kb=$(du -sk "$d" | cut -f1)
    freed=$((freed + kb))
    if [[ "$DRY" == 1 ]]; then
        say "would remove $d ($((kb / 1024)) MiB)"
    else
        rm -rf -- "$d"
        say "removed $d ($((kb / 1024)) MiB)"
    fi
done

if [[ "$DRY" == 1 ]]; then
    say "dry run: $((freed / 1024)) MiB would be freed"
else
    say "freed $((freed / 1024)) MiB. Rebuild with: scripts/setup_worktree.sh --build"
fi
