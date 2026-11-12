#!/usr/bin/env bash
# Checks that the C++ lines you changed follow .clang-format.
#
# Scope is deliberately narrow: only lines changed relative to <base-ref> in
# first-party C++ files are checked (git-clang-format), so historical
# formatting is never forced into unrelated PRs and nothing is mass-reformatted.
# Excluded: any thirdparty/ directory, rpi-rgb-led-matrix/, vcpkg/.
#
# Usage:
#   scripts/check_cpp_format.sh [<base-ref>]         # default origin/master; checks <base-ref>...HEAD
#   scripts/check_cpp_format.sh --staged             # staged changes only
#   scripts/check_cpp_format.sh --fix [<base-ref>]   # rewrite the working tree to match
#
# Needs git-clang-format (apt: clang-format; pip: clang-format==21.1.8, the
# version CI uses). Output varies slightly between clang-format versions, so
# use the CI version if the check disagrees with your editor.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

FIX=0
if [[ "${1:-}" == "--fix" ]]; then FIX=1; shift; fi

if ! command -v git-clang-format >/dev/null; then
    echo "git-clang-format not found (apt install clang-format, or pip install clang-format==21.1.8)" >&2
    exit 2
fi

EXT="c,cc,cpp,cxx,h,hh,hpp,hxx"
# First-party files only: filter out vendored code ourselves (git-clang-format takes paths, not pathspecs).
first_party() { grep -vE '(^|/)(thirdparty|vcpkg)/|^rpi-rgb-led-matrix/' || true; }

if [[ "${1:-}" == "--staged" ]]; then
    COMMITS=(--staged)
    FILES=$(git diff --cached --name-only --diff-filter=d | first_party)
    BASE=""
else
    BASE="${1:-origin/master}"
    MB=$(git merge-base "$BASE" HEAD)
    COMMITS=("$MB" HEAD)
    FILES=$(git diff --name-only --diff-filter=d "$MB" HEAD | first_party)
fi

if [[ -z "$FILES" ]]; then echo "C++ format: no changed files"; exit 0; fi

if [[ $FIX -eq 1 ]]; then
    [[ "${COMMITS[0]}" == "--staged" ]] && FIXC=(--staged) || FIXC=("${COMMITS[0]}")
    # shellcheck disable=SC2086
    git-clang-format --extensions "$EXT" "${FIXC[@]}" -- $FILES
    exit 0
fi

# shellcheck disable=SC2086
OUT=$(git-clang-format --diff --extensions "$EXT" "${COMMITS[@]}" -- $FILES 2>&1) || true

if [[ -z "$OUT" || "$OUT" == *"no modified files to format"* || "$OUT" == *"did not modify any files"* ]]; then
    echo "C++ format: OK"
    exit 0
fi

echo "$OUT"
echo
echo "C++ format: the changed lines above differ from .clang-format."
echo "Fix with: scripts/check_cpp_format.sh --fix ${BASE}"
exit 1
