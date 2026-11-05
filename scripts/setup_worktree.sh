#!/usr/bin/env bash
#
# setup_worktree.sh: make a fresh git worktree ready to build in seconds.
#
# Agents work in many short-lived worktrees. Rebuilding everything from scratch
# in each one is slow, so a worktree shares as much as possible with the main
# checkout:
#
#   * vcpkg ports      -> vcpkg binary cache (~/.cache/vcpkg/archives), restored
#                         in about a second on configure; VCPKG_ROOT is found
#                         automatically (cmake/BuildCache.cmake)
#   * object files     -> ccache with base_dir=<worktree>, hash_dir=false, so
#                         compiles from any worktree/main checkout are cache hits
#   * .env, react-web/dist, scene_previews/ -> copied from the main checkout
#                         (all git-ignored, so a new worktree lacks them)
#
# Usage: scripts/setup_worktree.sh [--preset emulator] [--build] [--no-configure]
#   --preset <name>   configure preset (default: emulator)
#   --build           also build + install (mostly ccache hits after the first)
#   --no-configure    only copy the shared, git-ignored files
# Idempotent: safe to run again; existing files/build dirs are left alone.
set -euo pipefail

PRESET=emulator BUILD=0 CONFIGURE=1
while [[ $# -gt 0 ]]; do
    case "$1" in
        --preset) PRESET="$2"; shift 2 ;;
        --build) BUILD=1; shift ;;
        --no-configure) CONFIGURE=0; shift ;;
        -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
        *) echo "unknown option: $1" >&2; exit 2 ;;
    esac
done

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo"
main="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")"
say() { printf '\033[1;36m[worktree]\033[0m %s\n' "$*"; }

if [[ "$main" != "$repo" ]]; then
    [[ -f .env || ! -f "$main/.env" ]] || { cp "$main/.env" .env; say "copied .env"; }
    if [[ ! -d react-web/dist && -d "$main/react-web/dist" ]]; then
        cp -r "$main/react-web/dist" react-web/dist
        say "copied react-web/dist (lets -DSKIP_WEB_BUILD=ON installs still ship the UI)"
    fi
    if ! compgen -G "scene_previews/*.gif" >/dev/null && compgen -G "$main/scene_previews/*.gif" >/dev/null; then
        mkdir -p scene_previews
        cp "$main"/scene_previews/*.gif scene_previews/
        say "copied scene_previews/*.gif"
    fi
else
    say "this is the main checkout; nothing to copy"
fi

# The repo-size pre-commit hook (core.hooksPath is shared by all worktrees).
[[ "$(git config core.hooksPath || true)" == ".githooks" ]] || git config core.hooksPath .githooks

command -v ccache >/dev/null || say "ccache not installed: every worktree compiles from scratch (apt install ccache)"

case "$PRESET" in
    emulator) build_dir=emulator_build ;;
    cross-compile) build_dir=build ;;
    desktop*) build_dir=desktop_build ;;
    *) build_dir="" ;;
esac

if [[ "$CONFIGURE" == 1 ]]; then
    if [[ -n "$build_dir" && -f "$build_dir/CMakeCache.txt" ]]; then
        say "$build_dir already configured"
    else
        say "configuring preset $PRESET (react-web build skipped)"
        cmake --preset "$PRESET" -DSKIP_WEB_BUILD=ON >/dev/null
    fi
fi

if [[ "$BUILD" == 1 ]]; then
    say "building $PRESET"
    cmake --build --preset "$PRESET" --target install
    command -v ccache >/dev/null && ccache --show-stats 2>/dev/null | grep -E 'Hits|Misses' | head -2 || true
fi
say "ready. Build with: cmake --build --preset $PRESET --target install"
