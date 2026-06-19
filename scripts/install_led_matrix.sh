#!/usr/bin/env bash
set -e

# Colors
GREEN='\033[1;32m'; CYAN='\033[1;36m'; YELLOW='\033[1;33m'
RED='\033[1;31m'; BOLD='\033[1m'; RESET='\033[0m'

print_header() {
  echo -e "${CYAN}${BOLD}"
  echo "=============================================="
  echo "  🚦  LED Matrix Controller Installer  🚦"
  echo "=============================================="
  echo -e "${RESET}"
}
print_error()   { echo -e "${RED}${BOLD}❌ Error:${RESET} $1"; }
print_success() { echo -e "${GREEN}${BOLD}✅ $1${RESET}"; }
print_info()    { echo -e "${YELLOW}ℹ️  $1${RESET}"; }

print_header

# Verify user 'pi' exists (required as the post-drop identity)
if ! id -u pi >/dev/null 2>&1; then
  print_error "User 'pi' does not exist. Please create the user 'pi' before running this script."
  exit 1
fi

# Check architecture
ARCH=$(uname -m)
if [[ "$ARCH" != "aarch64" && "$ARCH" != "arm64" ]]; then
  print_error "This installer only supports arm64 systems."
  exit 1
fi

# Fetch latest release asset
REPO="sshcrack/led-matrix"
API_URL="https://api.github.com/repos/$REPO/releases/latest"
print_info "Fetching latest release info from GitHub... 🚀"
ASSET_URL=$(curl -s "$API_URL" | grep "browser_download_url" \
  | grep -E 'led-matrix-[0-9]+\.[0-9]+\.[0-9]+_arm64\.deb' \
  | cut -d '"' -f 4 | head -n 1)

if [[ -z "$ASSET_URL" ]]; then
  print_error "Could not find led-matrix-x.y.z-arm64.deb in the latest release."
  exit 1
fi

# Download
TMP_DIR=$(mktemp -d)
DEB_FILE="$TMP_DIR/led-matrix-arm64.deb"
print_info "⬇️  Downloading led-matrix for arm64..."
curl -# -fL -o "$DEB_FILE" "$ASSET_URL"

# Install — debconf will present the configuration dialog automatically
print_info "📦 Installing package (you will be prompted for configuration)..."
trap 'rm -rf "$TMP_DIR"' EXIT
sudo dpkg -i "$DEB_FILE" || sudo apt-get install -f -y || true
rm -rf "$TMP_DIR"

print_success "Installation complete! 🎉"
echo
print_info "You can check the service status with:"
echo -e "${BOLD}  sudo systemctl status led-matrix.service${RESET}"
echo
print_info "Access the web interface at:"
echo -e "${BOLD}  http://$(hostname -I | awk '{print $1}'):8080${RESET}"
echo
print_info "To reconfigure hardware or plugin settings, run:"
echo -e "${BOLD}  sudo dpkg-reconfigure led-matrix${RESET}"
echo
print_success "Enjoy your LED Matrix Controller! 🌈"
