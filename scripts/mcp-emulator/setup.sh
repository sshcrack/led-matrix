#!/usr/bin/env bash
# Setup script for the LED matrix MCP emulator server.
# Creates a Python virtual environment and installs dependencies.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"
BINARY="$PROJECT_ROOT/emulator_build/install/main"

# ── Python venv ────────────────────────────────────────────────────────────
if ! command -v python3 &>/dev/null; then
    echo "ERROR: python3 not found. Install Python 3.10+ first." >&2
    exit 1
fi


echo "Creating virtual environment at $VENV_DIR …"
python3 -m venv "$VENV_DIR"
source "$VENV_DIR/bin/activate"

echo "Installing Python dependencies …"
pip install --quiet --upgrade pip
pip install --quiet -r "$SCRIPT_DIR/requirements.txt"

deactivate

# ── Emulator binary check ──────────────────────────────────────────────────
echo ""
if [ -f "$BINARY" ]; then
    echo "✓ Emulator binary found: $BINARY"
else
    echo "⚠  Emulator binary NOT found at: $BINARY"
    echo "   Build it first:"
    echo "     cd $PROJECT_ROOT"
    echo "     cmake --preset emulator"
    echo "     cmake --build emulator_build --target install"
fi

# ── MCP configuration ──────────────────────────────────────────────────────
SERVER_PY="$SCRIPT_DIR/server.py"
PYTHON_BIN="$VENV_DIR/bin/python"

echo ""
echo "────────────────────────────────────────────────────────────────"
echo " MCP server ready. Add to your .mcp.json (project root):"
echo "────────────────────────────────────────────────────────────────"
cat <<EOF
{
  "mcpServers": {
    "led-matrix": {
      "command": "$PYTHON_BIN",
      "args": ["$SERVER_PY"]
    }
  }
}
EOF
echo "────────────────────────────────────────────────────────────────"
echo ""
echo "Or run the server manually to test it:"
echo "  $PYTHON_BIN $SERVER_PY"
