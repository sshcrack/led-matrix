#!/bin/sh
# Installs the LED Matrix Controller desktop app for the current user (no root):
#
#   curl -fsSL https://raw.githubusercontent.com/sshcrack/led-matrix/master/scripts/install-desktop.sh | sh
#   curl -fsSL .../install-desktop.sh | sh -s -- 1.21.0    # a specific version
#
# It downloads the release tarball and runs `led-matrix-desktop --install`, which
# puts it in ~/.local/share/led-matrix-desktop, adds a launcher entry and
# ~/.local/bin/led-matrix-desktop. The app keeps itself up to date from then on
# (matching your matrix's version). Uninstall: led-matrix-desktop --uninstall
set -eu

REPO="sshcrack/led-matrix"
VERSION="${1:-}"

case "$(uname -s)-$(uname -m)" in
    Linux-x86_64) ;;
    *) echo "Only x86_64 Linux has a desktop build; on Windows use the setup .exe from the releases page." >&2; exit 1 ;;
esac

if [ -z "$VERSION" ]; then
    VERSION="$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" |
        sed -n 's/.*"tag_name": *"v\{0,1\}\([^"]*\)".*/\1/p' | head -n 1)"
    [ -n "$VERSION" ] || { echo "Could not find the latest release." >&2; exit 1; }
fi
VERSION="${VERSION#v}"

ASSET="led-matrix-desktop-$VERSION-Linux.tar.gz"
URL="https://github.com/$REPO/releases/download/v$VERSION/$ASSET"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT INT TERM

echo "Downloading LED Matrix Controller $VERSION..."
curl -fL --progress-bar -o "$TMP/$ASSET" "$URL"
mkdir "$TMP/pkg"
tar -xzf "$TMP/$ASSET" -C "$TMP/pkg"

EXE="$(find "$TMP/pkg" -path '*/bin/led-matrix-desktop' -type f | head -n 1)"
[ -n "$EXE" ] || { echo "The package does not contain bin/led-matrix-desktop." >&2; exit 1; }
"$EXE" --install

case ":$PATH:" in
    *":$HOME/.local/bin:"*) ;;
    *) echo "Tip: add ~/.local/bin to your PATH to start it with 'led-matrix-desktop'." ;;
esac
