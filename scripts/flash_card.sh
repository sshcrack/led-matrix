#!/usr/bin/env bash
# One-shot card flashing for Linux and macOS: downloads Raspberry Pi Imager
# (cached, nothing is installed system-wide) and starts it with this project's
# image list, so "LED Matrix" is already in the operating system list.
#
#   curl -fsSL https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/flash_card.sh | bash
#
# Options: --print  show the command instead of starting Imager
set -euo pipefail

REPO_URL="${LED_MATRIX_IMAGER_REPO:-https://github.com/sshcrack/led-matrix/releases/latest/download/led-matrix-imager.json}"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/led-matrix-imager"
DL="https://downloads.raspberrypi.com/imager"
PRINT=0
[[ "${1:-}" == "--print" ]] && PRINT=1

mkdir -p "$CACHE"
os="$(uname -s)"; arch="$(uname -m)"

fetch() { # url dest
  [[ -s "$2" ]] || { echo "Downloading $(basename "$2")..."; curl -fL# -o "$2.part" "$1" && mv "$2.part" "$2"; }
}

case "$os" in
  Linux)
    if [[ "$arch" == x86_64 ]]; then
      url="$DL/imager_latest_amd64.AppImage"
      final="$(curl -fsSIL -o /dev/null -w '%{url_effective}' "$url")"
    else
      final="$(curl -fsSL https://api.github.com/repos/raspberrypi/rpi-imager/releases/latest \
        | grep -o 'https://[^"]*desktop-aarch64\.AppImage' | head -n1)"
      [[ "$arch" == aarch64 || "$arch" == arm64 ]] || { echo "Unsupported CPU: $arch" >&2; exit 1; }
    fi
    [[ -n "$final" ]] || { echo "Could not find the Imager download." >&2; exit 1; }
    bin="$CACHE/$(basename "$final")"
    fetch "$final" "$bin"; chmod +x "$bin"
    # AppImages need FUSE; fall back to extracting when it is missing.
    if ! command -v fusermount >/dev/null 2>&1 && ! command -v fusermount3 >/dev/null 2>&1; then
      [[ -x "$CACHE/squashfs-root/AppRun" ]] || (cd "$CACHE" && "$bin" --appimage-extract >/dev/null)
      cmd=("$CACHE/squashfs-root/AppRun")
    else
      cmd=("$bin")
    fi
    ;;
  Darwin)
    final="$(curl -fsSIL -o /dev/null -w '%{url_effective}' "$DL/imager_latest.dmg")"
    app="$CACHE/Raspberry Pi Imager.app"
    if [[ ! -d "$app" ]]; then
      dmg="$CACHE/$(basename "$final")"; fetch "$final" "$dmg"
      mnt="$(mktemp -d)"; hdiutil attach -nobrowse -readonly -mountpoint "$mnt" "$dmg" >/dev/null
      cp -R "$mnt/Raspberry Pi Imager.app" "$app"; hdiutil detach "$mnt" >/dev/null
    fi
    cmd=(open -W "$app" --args)
    ;;
  *) echo "Unsupported OS: $os (on Windows use scripts/flash_card.ps1)" >&2; exit 1 ;;
esac

cmd+=(--repo "$REPO_URL")
if (( PRINT )); then printf '%q ' "${cmd[@]}"; echo; exit 0; fi
echo "Starting Raspberry Pi Imager with the LED Matrix image list..."
exec "${cmd[@]}"
