# WSL File Sync with Web Server

This system replaces the original `wsl_sync.sh` script with a web server-based approach that can receive file change notifications from Windows.

## Components

1. **WSL Sync Server** (`wsl_sync_server.py`) - Runs on WSL/Linux, listens for sync requests
2. **Windows File Watcher** - Watches for file changes on Windows and notifies the server
   - PowerShell version: `windows_file_watcher.ps1`
   - Python version: `windows_file_watcher.py`
   - Batch launcher: `start_windows_watcher.bat`

## Setup

### 1. WSL/Linux Side (Server)

First, make sure you have the required dependencies:

```bash
# Install dependencies
sudo apt update
sudo apt install python3 python3-pip rsync ripgrep

# Make the server executable
chmod +x wsl_sync_server.py
```

### 2. Windows Side (Client)

Install Python dependencies:
```pwsh
pip install watchdog requests
```

## Usage

On WSL, navigate to your project directory and run:

```bash
# Basic usage
./wsl_sync_server.py /mnt/e/C++Proj/led-matrix ~/led-matrix

# With custom port
./wsl_sync_server.py /mnt/e/C++Proj/led-matrix ~/led-matrix -p 8080
```

The server will:
- Perform an initial sync
- Start listening on `http://localhost:8080` (or your custom port)
- Wait for POST requests to `/sync` endpoint

```pwsh
# Basic usage
python windows_file_watcher.py "E:\C++Proj\led-matrix"

# With custom server URL
python windows_file_watcher.py "E:\C++Proj\led-matrix" -s http://localhost:8080

# With custom debounce time
python windows_file_watcher.py "E:\C++Proj\led-matrix" -d 3.0
```

## How It Works

1. **File Change Detection**: The Windows client monitors your project directory for file changes
2. **Debouncing**: Multiple rapid changes are debounced to avoid excessive sync operations
3. **Filtering**: Files are filtered based on `.gitignore` patterns and common temporary files
4. **Web Request**: When changes are detected, the client sends a POST request to the WSL server
5. **Sync Trigger**: The server receives the request and triggers `rsync` with `ripgrep` file listing
6. **Gitignore Support**: Both client and server respect `.gitignore` and `.rgignore` files

## Server Endpoints

- `GET /` - Web interface showing server status
- `GET /health` - Health check endpoint
- `POST /sync` - Trigger sync operation

## File Filtering

The system automatically respects `.gitignore` patterns and also ignores common temporary files:

### Gitignore Support:
- Reads and parses `.gitignore` file from the watch directory
- Supports standard gitignore patterns including wildcards and negation (!)
- Handles directory patterns (ending with `/`)
- Both the Windows client and WSL server respect gitignore rules

### Additional Temporary File Patterns:
- Temporary files: `.tmp`, `.temp`, `.swp`, `.swo`, `~`, `.git`
- System files: `.DS_Store`, `Thumbs.db`
- Log files: `.log`, `.cache`

## Troubleshooting

### Server won't start
- Check if the port is already in use: `netstat -an | grep :8080`
- Ensure ripgrep and rsync are installed
- Verify source and destination directories exist

### Windows client can't connect
- Check if the WSL server is running
- Verify the server URL (default: `http://localhost:8080`)
- Ensure Windows can reach the WSL instance

### Sync not working
- Check server logs for error messages
- Verify file permissions on both sides
- Test manual sync request: `curl -X POST http://localhost:8080/sync`

## Security Note

The server listens on `localhost` only by default. If you need to access it from a different machine, modify the server code to bind to a specific IP address, but be aware of security implications.
