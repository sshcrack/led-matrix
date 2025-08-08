#!/usr/bin/env bash

set -e

# Colors and formatting
GREEN='\033[1;32m'
CYAN='\033[1;36m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
BOLD='\033[1m'
RESET='\033[0m'

# Ensure user 'pi' exists before setting SUDO_UID and SUDO_GID
if ! id -u pi >/dev/null 2>&1; then
  echo -e "${RED}${BOLD}❌ Error:${RESET} User 'pi' does not exist. Please create the user 'pi' before running this script."
  exit 1
fi
# Set SUDO_UID and SUDO_GID to those of user 'pi'
PI_UID=$(id -u pi)
PI_GID=$(id -g pi)

# Spinner function
spin() {
  local pid=$!
  local delay=0.1
  local spinstr='|/-\'
  while kill -0 $pid 2>/dev/null; do
    local temp=${spinstr#?}
    printf " [%c]  " "$spinstr"
    spinstr=$temp${spinstr%"$temp"}
    sleep $delay
    printf "\b\b\b\b\b\b"
  done
  wait $pid
}

print_header() {
  echo -e "${CYAN}${BOLD}"
  echo "=============================================="
  echo "  🚦  LED Matrix Controller Installer  🚦"
  echo "=============================================="
  echo -e "${RESET}"
}

print_error() {
  echo -e "${RED}${BOLD}❌ Error:${RESET} $1"
}

print_success() {
  echo -e "${GREEN}${BOLD}✅ $1${RESET}"
}

print_info() {
  echo -e "${YELLOW}ℹ️  $1${RESET}"
}

print_header

# Check for existing installation and offer full reinstall
if [[ -d "/opt/led-matrix" ]]; then
  print_info "Detected existing installation at /opt/led-matrix."
  echo -e "${RED}${BOLD}WARNING:${RESET} This will DELETE ALL DATA including config.json."
  read -p "Type DELETE to confirm full removal and reinstall: " REINSTALL_CONFIRM
  if [[ "$REINSTALL_CONFIRM" == "DELETE" ]]; then
    print_info "Stopping and disabling led-matrix.service..."
    sudo systemctl stop led-matrix.service 2>/dev/null || true
    sudo systemctl disable led-matrix.service 2>/dev/null || true
    sudo rm -f /etc/systemd/system/led-matrix.service
    print_info "Removing /opt/led-matrix..."
    sudo rm -rf /opt/led-matrix
    print_success "Previous installation removed. Proceeding with fresh install."
  else
    print_info "Reinstall cancelled by user. Exiting."
    exit 0
  fi
fi

# Check architecture
ARCH=$(uname -m)
if [[ "$ARCH" != "aarch64" && "$ARCH" != "arm64" ]]; then
  print_error "This installer only supports arm64 systems."
  exit 1
fi

# Get latest release info from GitHub API
REPO="sshcrack/led-matrix"
API_URL="https://api.github.com/repos/$REPO/releases/latest"
print_info "Fetching latest release info from GitHub... 🚀"
ASSET_URL=$(curl -s "$API_URL" | grep "browser_download_url" | grep -E 'led-matrix-[0-9]+\.[0-9]+\.[0-9]+-arm64\.tar\.gz' | cut -d '"' -f 4 | head -n 1)

if [[ -z "$ASSET_URL" ]]; then
  print_error "Could not find led-matrix-x.y.z-arm64.tar.gz in the latest release."
  exit 1
fi

# Prompt for all parameters before downloading and extracting

echo -e "${CYAN}${BOLD}"
echo "----------------------------------------------"
echo "   🖥️  LED Matrix Hardware Configuration"
echo "----------------------------------------------"
echo -e "${RESET}"
echo -e "Please enter your matrix setup parameters. Press Enter to use the recommended value in [brackets]."
echo

read -p "Rows per panel (--led-rows) [64]: " LED_ROWS
LED_ROWS=${LED_ROWS:-64}

read -p "Columns per panel (--led-cols) [64]: " LED_COLS
LED_COLS=${LED_COLS:-64}

read -p "Number of chained panels (--led-chain) [2]: " LED_CHAIN
LED_CHAIN=${LED_CHAIN:-2}

read -p "Number of parallel chains (--led-parallel) [2]: " LED_PARALLEL
LED_PARALLEL=${LED_PARALLEL:-2}

echo
echo -e "${YELLOW}If you are using a Raspberry Pi 3 Model B, it is recommended to set --led-slowdown-gpio to 3.${RESET}"
read -p "Set --led-slowdown-gpio? (Recommended for RPi 3 Model B) [3, leave blank to skip]: " LED_SLOWDOWN_GPIO

MATRIX_OPTS="--led-rows ${LED_ROWS} --led-cols ${LED_COLS} --led-chain ${LED_CHAIN} --led-parallel ${LED_PARALLEL}"
if [[ -n "$LED_SLOWDOWN_GPIO" ]]; then
  MATRIX_OPTS="$MATRIX_OPTS --led-slowdown-gpio ${LED_SLOWDOWN_GPIO}"
fi

echo
echo -e "${YELLOW}You can add any additional custom parameters for the matrix controller below.${RESET}"
read -p "Enter any extra parameters (leave blank to skip): " CUSTOM_MATRIX_OPTS
if [[ -n "$CUSTOM_MATRIX_OPTS" ]]; then
  MATRIX_OPTS="$MATRIX_OPTS $CUSTOM_MATRIX_OPTS"
fi

print_success "Matrix configuration: $MATRIX_OPTS"

echo -e "${CYAN}${BOLD}"
echo "----------------------------------------------"
echo "         🎵  Optional: Spotify Plugin"
echo "----------------------------------------------"
echo -e "${RESET}"
echo -e "${BOLD}The Spotify Plugin shows the cover image of the song you are currently listening to!${RESET}"
echo -e "To use it, you need a Spotify Developer account and must provide your Client ID and Client Secret."
echo
read -p "Do you want to use the Spotify Plugin? (y/N): " USE_SPOTIFY

SPOTIFY_ENV=""
if [[ "$USE_SPOTIFY" =~ ^[Yy]$ ]]; then
  read -p "🔑 Enter your Spotify Client ID: " SPOTIFY_CLIENT_ID
  read -p "🔒 Enter your Spotify Client Secret: " SPOTIFY_CLIENT_SECRET
  SPOTIFY_ENV="Environment=SPOTIFY_CLIENT_ID=$SPOTIFY_CLIENT_ID\nEnvironment=SPOTIFY_CLIENT_SECRET=$SPOTIFY_CLIENT_SECRET"
  print_success "Spotify Plugin will be enabled! 🎶"
else
  print_info "Spotify Plugin will not be enabled."
fi

echo -e "${CYAN}${BOLD}"
echo "----------------------------------------------"
echo "       🔄  Automatic Updates Configuration"
echo "----------------------------------------------"
echo -e "${RESET}"
echo -e "${BOLD}The LED Matrix Controller can automatically check for and install updates.${RESET}"
echo -e "This keeps your system up-to-date with the latest features and security fixes."
echo
read -p "Do you want to enable automatic updates? (Y/n): " ENABLE_AUTO_UPDATES
ENABLE_AUTO_UPDATES=${ENABLE_AUTO_UPDATES:-Y}

UPDATE_SETTINGS=""
if [[ "$ENABLE_AUTO_UPDATES" =~ ^[Yy]$ ]]; then
  print_success "Automatic updates will be enabled! 🔄"
  echo
  echo -e "${YELLOW}You can configure how often to check for updates:${RESET}"
  echo "1. Every 6 hours (recommended if you want to stay on the bleeding edge)"
  echo "2. Every 12 hours"
  echo "3. Every 24 hours (stable, recommended for production)"
  echo "4. Every 48 hours"
  echo "5. Every week (168 hours)"
  echo
  read -p "Select update check frequency [1-5] (default: 3): " UPDATE_FREQUENCY
  UPDATE_FREQUENCY=${UPDATE_FREQUENCY:-3}
  
  case $UPDATE_FREQUENCY in
    1) CHECK_INTERVAL=6; FREQ_DESC="6 hours";;
    2) CHECK_INTERVAL=12; FREQ_DESC="12 hours";;
    3) CHECK_INTERVAL=24; FREQ_DESC="24 hours";;
    4) CHECK_INTERVAL=48; FREQ_DESC="48 hours";;
    5) CHECK_INTERVAL=168; FREQ_DESC="1 week";;
    *) CHECK_INTERVAL=24; FREQ_DESC="24 hours (default)";;
  esac
  
  print_success "Updates will be checked every $FREQ_DESC"
  
  # Create initial config with update settings
  UPDATE_SETTINGS=$(cat <<EOF
{
  "update_settings": {
    "auto_update_enabled": true,
    "check_interval_hours": $CHECK_INTERVAL,
    "current_version": "1.0.0",
    "last_check_time": 0,
    "last_update_time": 0,
    "update_available": false,
    "latest_version": "",
    "update_download_url": ""
  }
}
EOF
  )
else
  print_info "Automatic updates will be disabled. You can enable them later from the web interface."
  UPDATE_SETTINGS=$(cat <<EOF
{
  "update_settings": {
    "auto_update_enabled": false,
    "check_interval_hours": 24,
    "current_version": "1.0.0",
    "last_check_time": 0,
    "last_update_time": 0,
    "update_available": false,
    "latest_version": "",
    "update_download_url": ""
  }
}
EOF
  )
fi

# Now perform the download and extraction after all prompts
TMP_DIR=$(mktemp -d)
cd "$TMP_DIR"
print_info "⬇️  Downloading LED Matrix binary for arm64..."
curl -# -L -o led-matrix-arm64.tar.gz "$ASSET_URL"

# Check if already installed
if [[ -f "/opt/led-matrix/main" ]]; then
  print_info "Detected existing installation at /opt/led-matrix."
  print_info "All files except for the configuration file (config.json) will be deleted and replaced with the latest version."
  read -p "Do you want to continue with the update? (y/N): " CONFIRM_UPDATE
  if [[ ! "$CONFIRM_UPDATE" =~ ^[Yy]$ ]]; then
    print_info "Update cancelled by user. Exiting."
    exit 0
  fi
  print_info "Updating to latest version..."
  # Backup config.json if it exists
  if [[ -f "/opt/led-matrix/config.json" ]]; then
    sudo cp /opt/led-matrix/config.json "$TMP_DIR/config.json.bak"
  fi
  # Remove everything except config.json
  sudo find /opt/led-matrix -mindepth 1 -not -name 'config.json' -exec rm -rf {} +
  # Extract new files
  print_info "Extracting update..."
  (sudo tar -xzf led-matrix-arm64.tar.gz -C /opt/led-matrix --strip-components=1) & spin
  # Restore config.json if it was backed up
  if [[ -f "$TMP_DIR/config.json.bak" ]]; then
    sudo mv "$TMP_DIR/config.json.bak" /opt/led-matrix/config.json
  fi
  print_success "Update complete!"
else
  print_info "No existing installation found. Installing fresh..."
  print_info "📦 Extracting to /opt/led-matrix (requires sudo)..."
  (sudo mkdir -p /opt/led-matrix && sudo tar -xzf led-matrix-arm64.tar.gz -C /opt/led-matrix --strip-components=1) & spin
fi

print_info "📝 Creating initial configuration..."
# Create config.json with update settings if it doesn't exist
if [[ ! -f "/opt/led-matrix/config.json" ]]; then
  (echo "$UPDATE_SETTINGS" | sudo tee /opt/led-matrix/config.json > /dev/null) & spin
  print_success "Configuration file created with update settings"
else
  print_info "Configuration file already exists, keeping existing settings"
fi

SERVICE_FILE="/etc/systemd/system/led-matrix.service"
print_info "🛠️  Setting up systemd service..."
(sudo bash -c "cat > $SERVICE_FILE" <<EOF
[Unit]
Description=LED Matrix Controller
After=network.target

[Service]
Type=simple
WorkingDirectory=/opt/led-matrix
ExecStart=/opt/led-matrix/main $MATRIX_OPTS
Restart=on-failure
User=root
Environment=SUDO_UID=$PI_UID
Environment=SUDO_GID=$PI_GID
$SPOTIFY_ENV

[Install]
WantedBy=multi-user.target
EOF
) & spin

print_info "🔄 Reloading systemd and enabling service..."
(sudo systemctl daemon-reload && sudo systemctl enable led-matrix.service && sudo systemctl restart led-matrix.service) & spin

print_success "Installation complete! 🎉"
echo
print_info "You can check the service status with:"
echo -e "${BOLD}  sudo systemctl status led-matrix.service${RESET}"
echo
print_info "Access the web interface at:"
echo -e "${BOLD}  http://$(hostname -I | awk '{print $1}'):8080${RESET}"
echo
if [[ "$ENABLE_AUTO_UPDATES" =~ ^[Yy]$ ]]; then
  print_info "Automatic updates are enabled and will check every $FREQ_DESC"
  print_info "You can manage update settings from the web interface at /updates"
else
  print_info "Automatic updates are disabled. Enable them from the web interface if needed."
fi
echo
print_success "Enjoy your LED Matrix Controller! 🌈"
