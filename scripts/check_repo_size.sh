#!/usr/bin/env bash
# Keeps the git history small so `git clone` stays fast.
#
# Every blob committed lives in history forever, so the cheap place to stop
# bloat is before it is committed. Checks only files that are ADDED or MODIFIED:
#
#   * any single file larger than MAX_FILE_KB (default 2048)  -> fail
#   * blocked artifact types (.aab .apk .zip .deb heaptrack.* ...) -> fail
#   * total bytes of added/modified files > MAX_TOTAL_KB (default 10240) -> fail
#
# Exceptions:
#   * list a path (one per line, `#` comments allowed) in .repo-size-allow
#     to exempt that file from the per-file and blocked-type checks
#   * the total budget can be waived for a range by putting `[large-ok]` in a
#     commit message (e.g. a deliberate full scene_previews regeneration)
#
# Usage:
#   scripts/check_repo_size.sh --staged        # pre-commit hook
#   scripts/check_repo_size.sh <base-ref>      # CI: compares <base-ref>...HEAD
#
# scene_previews/*.gif are always blocked: they are published as release assets
# (scripts/publish_scene_previews.sh), never committed.
set -euo pipefail

MAX_FILE_KB="${MAX_FILE_KB:-2048}"
MAX_TOTAL_KB="${MAX_TOTAL_KB:-10240}"
BLOCKED_RE='(\.(aab|apk|ipa|zst|hprof|dmp|exe|dll|so|dylib|deb|tar|gz|tgz|zip|7z)$|(^|/)heaptrack[^/]*$|^scene_previews/[^/]*\.gif$)'

cd "$(git rev-parse --show-toplevel)"

if [[ $# -ne 1 ]]; then
    sed -n '2,/^set -e/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//' >&2
    exit 2
fi

if [[ "$1" == "--staged" ]]; then
    diff_args=(--cached)
    waived=0
else
    diff_args=("$1...HEAD")
    if git log --format=%B "$1..HEAD" | grep -qiF '[large-ok]'; then waived=1; else waived=0; fi
fi

declare -A allowed=()
if [[ -f .repo-size-allow ]]; then
    while IFS= read -r line; do
        line="${line%%#*}"; line="${line//[$'\t\r ']/}"
        [[ -n "$line" ]] && allowed["$line"]=1
    done < .repo-size-allow
fi

fail=0
total=0
# raw format: ":<mode> <mode> <sha> <sha> <status>\0<path>\0"
while IFS= read -r -d '' meta && IFS= read -r -d '' path; do
    read -r _ new_mode _ new_sha _ <<<"${meta#:}"
    [[ "$new_mode" == "160000" ]] && continue   # submodule pointer
    size=$(git cat-file -s "$new_sha")
    total=$((total + size))
    [[ -n "${allowed[$path]:-}" ]] && continue
    if [[ "$path" =~ $BLOCKED_RE ]]; then
        echo "ERROR: blocked artifact type committed: $path ($((size / 1024)) KiB)" >&2
        fail=1
    elif (( size > MAX_FILE_KB * 1024 )); then
        echo "ERROR: $path is $((size / 1024)) KiB (limit ${MAX_FILE_KB} KiB)" >&2
        fail=1
    fi
done < <(git diff --raw --no-renames --diff-filter=AM -z "${diff_args[@]}")

if (( total > MAX_TOTAL_KB * 1024 )) && (( waived == 0 )); then
    echo "ERROR: added/modified files total $((total / 1024)) KiB (budget ${MAX_TOTAL_KB} KiB)." >&2
    echo "       Regenerate only the previews you changed, or add [large-ok] to a commit message if intended." >&2
    fail=1
fi

if (( fail )); then
    cat >&2 <<'EOF'

Files are kept in git history forever and slow every future clone.
Put big binaries (builds, profiles, videos) outside the repo, e.g. in a GitHub
release asset. Exempt a deliberate file via .repo-size-allow.
Scene preview GIFs: scripts/publish_scene_previews.sh (release assets, not git).
EOF
    exit 1
fi
echo "repo size check passed (changed files: $((total / 1024)) KiB)"
