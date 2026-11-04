#!/usr/bin/env bash
# Download the scene preview GIFs (kept out of git, see scripts/publish_scene_previews.sh)
# from the rolling `scene-previews` GitHub release into scene_previews/.
#
# CMake installs scene_previews/*.gif into the package, so run this before a
# build/deploy on a fresh clone. CI does it for the cross-compile (.deb) job.
#
# Usage: scripts/fetch_scene_previews.sh [--if-missing] [dest_dir]
#   --if-missing   do nothing when dest_dir already contains GIFs
# Env:   LED_MATRIX_REPO (default sshcrack/led-matrix)
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo="${LED_MATRIX_REPO:-sshcrack/led-matrix}"
if_missing=0
dest=""
for arg in "$@"; do
    case "$arg" in
        --if-missing) if_missing=1 ;;
        -h|--help) sed -n '2,/^set -e/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) dest="$arg" ;;
    esac
done
dest="${dest:-$repo_dir/scene_previews}"

count_gifs() { find "$dest" -maxdepth 1 -name '*.gif' 2>/dev/null | wc -l; }

if (( if_missing )) && (( $(count_gifs) > 0 )); then
    echo "scene previews already present in $dest ($(count_gifs) GIFs), skipping fetch"
    exit 0
fi

url="https://github.com/$repo/releases/download/scene-previews/scene_previews.tar.gz"
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

echo "Fetching scene previews from $url"
curl -fsSL --retry 4 --retry-delay 3 -o "$tmp" "$url"
mkdir -p "$dest"
tar -xzf "$tmp" -C "$dest" --no-same-owner

n="$(count_gifs)"
if (( n == 0 )); then
    echo "ERROR: archive contained no GIFs" >&2
    exit 1
fi
echo "Fetched $n scene previews into $dest"
