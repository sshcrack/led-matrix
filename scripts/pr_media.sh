#!/usr/bin/env bash
#
# pr_media.sh: attach screenshots / GIFs / MP4s to a pull request from the CLI.
#
# GitHub has no API for the drag-and-drop "user-attachments" upload (it needs a
# browser session), so media goes to the rolling `pr-media` prerelease of this
# repo instead (like `scene-previews`): release assets never enter git history,
# and the updater ignores non-v* tags. Markdown can only embed images from
# release URLs, so every video also gets a small looping GIF preview that links
# to the full MP4.
#
# Usage:
#   scripts/pr_media.sh [--pr <number>] [--comment] [--title <text>] [--no-gif] <files...>
#
#   --pr <n>       PR number used to namespace asset names (default: PR of the
#                  current branch, else the branch name)
#   --comment      post the markdown as a PR comment (otherwise just print it)
#   --title <t>    heading for the comment
#   --no-gif       do not generate GIF previews for videos
#   --gif-width    preview width in px (default 320), --gif-seconds (default 6)
#
# Output: markdown on stdout, ready for a PR body or comment.
# Env:    LED_MATRIX_REPO (default: the gh default repo, else sshcrack/led-matrix)
# Needs:  gh (authenticated), ffmpeg for video previews.
set -euo pipefail

PR="" COMMENT=0 TITLE="" GIF=1 GIF_WIDTH=320 GIF_SECONDS=6
FILES=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        --pr) PR="$2"; shift 2 ;;
        --comment) COMMENT=1; shift ;;
        --title) TITLE="$2"; shift 2 ;;
        --no-gif) GIF=0; shift ;;
        --gif-width) GIF_WIDTH="$2"; shift 2 ;;
        --gif-seconds) GIF_SECONDS="$2"; shift 2 ;;
        -h|--help) sed -n '2,25p' "$0"; exit 0 ;;
        -*) echo "unknown option: $1" >&2; exit 2 ;;
        *) FILES+=("$1"); shift ;;
    esac
done
(( ${#FILES[@]} > 0 )) || { echo "no files given (see --help)" >&2; exit 2; }
command -v gh >/dev/null || { echo "gh CLI required" >&2; exit 1; }

repo="${LED_MATRIX_REPO:-$(gh repo view --json nameWithOwner -q .nameWithOwner 2>/dev/null || echo sshcrack/led-matrix)}"
tag="pr-media"
if [[ -z "$PR" ]]; then
    PR="$(gh pr view --repo "$repo" --json number -q .number 2>/dev/null || true)"
fi
if [[ -n "$PR" ]]; then
    prefix="pr${PR}"
else
    prefix="$(git rev-parse --abbrev-ref HEAD | tr -c 'A-Za-z0-9._-' '-' | sed 's/-*$//')"
fi

if ! gh release view "$tag" --repo "$repo" >/dev/null 2>&1; then
    echo "Creating release $tag" >&2
    gh release create "$tag" --repo "$repo" --prerelease --latest=false \
        --title "PR media (rolling)" \
        --notes "Screenshots and videos attached to pull requests by scripts/pr_media.sh. Not an app release; assets are named pr<number>-<file>. Safe to prune old assets." >&2
fi

stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
url() { echo "https://github.com/$repo/releases/download/$tag/$1"; }
safe() { echo "$1" | tr -c 'A-Za-z0-9._-\n' '-'; }

uploads=()
markdown=()
[[ -n "$TITLE" ]] && markdown+=("### $TITLE" "")
for f in "${FILES[@]}"; do
    [[ -f "$f" ]] || { echo "missing file: $f" >&2; exit 1; }
    base="$(basename "$f")"
    name="$(safe "${prefix}-${base}")"
    cp "$f" "$stage/$name"
    uploads+=("$stage/$name")
    stem="${base%.*}"
    case "${base,,}" in
        *.mp4|*.webm|*.mov)
            if [[ "$GIF" == 1 ]] && command -v ffmpeg >/dev/null; then
                gif="$(safe "${prefix}-${stem}.preview.gif")"
                ffmpeg -loglevel error -y -t "$GIF_SECONDS" -i "$f" \
                    -vf "fps=12,scale=${GIF_WIDTH}:-1:flags=neighbor,split[a][b];[a]palettegen=max_colors=96[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" \
                    "$stage/$gif"
                uploads+=("$stage/$gif")
                markdown+=("**${stem}**" "" "[![${stem}]($(url "$gif"))]($(url "$name"))" "" "▶ [${base} (full video)]($(url "$name"))" "")
            else
                markdown+=("▶ [${base}]($(url "$name"))" "")
            fi
            ;;
        *.png|*.jpg|*.jpeg|*.gif|*.webp)
            markdown+=("**${stem}**" "" "![${stem}]($(url "$name"))" "")
            ;;
        *)
            markdown+=("[${base}]($(url "$name"))" "")
            ;;
    esac
done

echo "Uploading ${#uploads[@]} file(s) to $repo@$tag" >&2
gh release upload "$tag" "${uploads[@]}" --repo "$repo" --clobber >&2

body="$(printf '%s\n' "${markdown[@]}")"
if [[ "$COMMENT" == 1 ]]; then
    [[ -n "$PR" ]] || { echo "--comment needs a PR (open one first or pass --pr)" >&2; exit 1; }
    gh pr comment "$PR" --repo "$repo" --body "$body" >&2
fi
printf '%s\n' "$body"
