#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
shader="${1:-}"
out_dir="${2:-$repo_root/desktop_build/shader-preview-manual}"
width="${3:-128}"
height="${4:-64}"

if [[ -z "$shader" ]]; then
  echo "Usage: $0 <shader.frag> [output-dir] [width=128] [height=64]" >&2
  exit 2
fi

if [[ "$shader" != /* ]]; then
  shader="$repo_root/$shader"
fi
if [[ "$out_dir" != /* ]]; then
  out_dir="$repo_root/$out_dir"
fi
if [[ ! -f "$shader" ]]; then
  echo "Shader not found: $shader" >&2
  exit 2
fi

build_dir="$repo_root/desktop_build"
preview_bin="$build_dir/tools/shadertoy_shader_preview"

if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  cmake --preset desktop-linux -S "$repo_root"
fi
# Always ask the build system for the target so source/renderer changes cannot
# accidentally leave an agent inspecting a stale preview executable.
cmake --build "$build_dir" --target shadertoy_shader_preview -j"${JOBS:-4}"

mkdir -p "$out_dir"

runner=()
if [[ -z "${DISPLAY:-}" ]] && command -v xvfb-run >/dev/null 2>&1; then
  runner=(xvfb-run -a)
fi

render() {
  local name="$1"
  local frames="$2"
  local audio="$3"
  echo "Rendering $name (${audio}, frame-count=${frames})..."
  "${runner[@]}" "$preview_bin" "$shader" "$out_dir/$name.png" "$width" "$height" "$frames" "$audio"
}

# A compact visual review set. 61 lands on a strong synthetic kick at 120 BPM;
# the other snapshots exercise motion between beats. Idle proves graceful
# behavior when no live audio analysis is available.
render idle 120 none
render early 30 synthetic
render kick 61 synthetic
render groove 90 synthetic
render late 150 synthetic

echo
echo "Visual review set written to: $out_dir"
printf '  %s\n' "$out_dir"/{idle,early,kick,groove,late}.png
