#!/usr/bin/env bash
# Publish scene_previews/*.gif to the rolling `scene-previews` GitHub release
# instead of committing them. Every committed GIF stays in git history forever
# and slows every clone; release assets cost the repo nothing.
#
# Publishes:
#   scene_previews.tar.gz   all GIFs with their exact names; what builds fetch
#                           (scripts/fetch_scene_previews.sh) - spaces etc. survive
#   <name>.gif              each GIF with a URL-safe name, for stable README embeds:
#                           https://github.com/<repo>/releases/download/scene-previews/<name>.gif
#
# Unchanged assets are skipped (compared by sha256); assets for removed scenes
# are deleted. The release is a prerelease, so it never counts as the app's
# "latest" release and never reaches the updater.
#
# Usage: scripts/publish_scene_previews.sh [previews_dir]    (needs: gh auth login)
# Env:   LED_MATRIX_REPO (default sshcrack/led-matrix)
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo="${LED_MATRIX_REPO:-sshcrack/led-matrix}"
dir="${1:-$repo_dir/scene_previews}"
tag="scene-previews"

command -v gh >/dev/null || { echo "ERROR: gh CLI required" >&2; exit 1; }
mapfile -t gifs < <(find "$dir" -maxdepth 1 -name '*.gif' -printf '%f\n' | LC_ALL=C sort)
(( ${#gifs[@]} > 0 )) || { echo "ERROR: no GIFs in $dir" >&2; exit 1; }

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

# Deterministic archive (no mtimes/owners) so an unchanged set hashes the same.
printf '%s\n' "${gifs[@]}" | tar -cf "$stage/scene_previews.tar" -C "$dir" --owner=0 --group=0 --numeric-owner --mtime=@0 -T -
gzip -n -9 "$stage/scene_previews.tar"

if ! gh release view "$tag" --repo "$repo" >/dev/null 2>&1; then
    echo "Creating release $tag"
    gh release create "$tag" --repo "$repo" --prerelease --latest=false \
        --title "Scene previews (rolling)" \
        --notes "Rolling asset store for the scene preview GIFs. Not an app release: published by scripts/publish_scene_previews.sh, consumed by scripts/fetch_scene_previews.sh and the README. Do not delete."
fi

declare -A existing=()
while IFS=$'\t' read -r name digest; do existing["$name"]="$digest"; done \
    < <(gh release view "$tag" --repo "$repo" --json assets --jq '.assets[] | [.name, (.digest // "")] | @tsv')

declare -A wanted=()
to_upload=()
consider() { # <name> <path>
    local name="$1" path="$2" sum
    wanted["$name"]=1
    sum="sha256:$(sha256sum "$path" | cut -d' ' -f1)"
    if [[ "${existing[$name]:-}" == "$sum" ]]; then return; fi
    to_upload+=("$path")
}

consider scene_previews.tar.gz "$stage/scene_previews.tar.gz"
skipped=()
for g in "${gifs[@]}"; do
    if [[ "$g" =~ ^[A-Za-z0-9_.-]+$ ]]; then
        consider "$g" "$dir/$g"
    else
        skipped+=("$g")   # only in the tarball: GitHub would rename it
    fi
done

if (( ${#to_upload[@]} > 0 )); then
    echo "Uploading ${#to_upload[@]} changed asset(s)..."
    gh release upload "$tag" "${to_upload[@]}" --repo "$repo" --clobber
else
    echo "All assets already up to date."
fi

for name in "${!existing[@]}"; do
    if [[ -z "${wanted[$name]:-}" ]]; then
        echo "Removing stale asset: $name"
        gh release delete-asset "$tag" "$name" --repo "$repo" --yes
    fi
done

(( ${#skipped[@]} == 0 )) || printf 'Note: only in the tarball (name not URL-safe): %s\n' "${skipped[@]}"
echo "Done: https://github.com/$repo/releases/tag/$tag"
