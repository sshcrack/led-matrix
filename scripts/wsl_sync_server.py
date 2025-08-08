#!/usr/bin/env python3
"""
WSL Sync Server - A simple HTTP server that triggers rsync when notified by Windows client
"""

import os
import sys
import json
import subprocess
import tempfile
import threading
import time
from datetime import datetime
from http.server import HTTPServer, BaseHTTPRequestHandler
from urllib.parse import urlparse
import argparse

def perform_sync(source_dir, dest_dir):
    """Perform the actual sync operation outside of HTTP context"""
    try:
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Starting sync...")

        os.chdir(source_dir)

        with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
            temp_filename = temp_file.name

            rg_cmd = ['rg', '--files', '--hidden']
            result = subprocess.run(rg_cmd, capture_output=True, text=True, cwd=source_dir)

            if result.returncode == 0:
                temp_file.write(result.stdout)
                temp_file.flush()

                file_count = len(result.stdout.strip().split('\n')) if result.stdout.strip() else 0
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Syncing {file_count} files...")

                rsync_cmd = [
                    'rsync', '-av',
                    '--delete',
                    '--delete-excluded',
                    f'--files-from={temp_filename}',
                    f'{source_dir}/',
                    f'{dest_dir}/'
                ]

                rsync_result = subprocess.run(rsync_cmd, capture_output=True, text=True)

                if rsync_result.returncode == 0:
                    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Sync completed successfully")
                else:
                    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Rsync error: {rsync_result.stderr}")

            else:
                print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Ripgrep error: {result.stderr}")

    except Exception as e:
        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Sync error: {e}")
    finally:
        try:
            os.unlink(temp_filename)
        except:
            pass


class SyncHandler(BaseHTTPRequestHandler):
    def __init__(self, *args, source_dir=None, dest_dir=None, **kwargs):
        self.source_dir = source_dir
        self.dest_dir = dest_dir
        super().__init__(*args, **kwargs)

    def log_message(self, format, *args):
        # Custom logging with timestamps
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"[{timestamp}] {format % args}")

    def do_GET(self):
        path = urlparse(self.path).path

        if path == '/health':
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.end_headers()

            response = {
                'status': 'healthy',
                'source': self.source_dir,
                'dest': self.dest_dir,
                'timestamp': datetime.now().isoformat()
            }
            self.wfile.write(json.dumps(response).encode())

        elif path == '/':
            self.send_response(200)
            self.send_header('Content-Type', 'text/html')
            self.end_headers()

            html = """
            <html>
            <head><title>WSL Sync Server</title></head>
            <body>
                <h1>WSL Sync Server</h1>
                <p><strong>Source:</strong> {source}</p>
                <p><strong>Destination:</strong> {dest}</p>
                <h2>Endpoints:</h2>
                <ul>
                    <li><code>POST /sync</code> - Trigger sync</li>
                    <li><code>GET /health</code> - Health check</li>
                </ul>
            </body>
            </html>
            """.format(source=self.source_dir, dest=self.dest_dir)

            self.wfile.write(html.encode())

        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not Found')

    def do_POST(self):
        path = urlparse(self.path).path

        if path == '/sync':
            # Read request body if present
            content_length = int(self.headers.get('Content-Length', 0))
            post_data = self.rfile.read(content_length) if content_length > 0 else b''

            try:
                # Perform sync in a separate thread to avoid blocking
                sync_thread = threading.Thread(target=self._perform_sync)
                sync_thread.start()

                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()

                response = {
                    'status': 'success',
                    'message': 'Sync triggered',
                    'timestamp': datetime.now().isoformat()
                }
                self.wfile.write(json.dumps(response).encode())

            except Exception as e:
                self.send_response(500)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()

                response = {
                    'status': 'error',
                    'message': str(e),
                    'timestamp': datetime.now().isoformat()
                }
                self.wfile.write(json.dumps(response).encode())
        else:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b'Not Found')

    def _perform_sync(self):
        """Perform the actual sync operation"""
        try:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Starting sync...")

            # Change to source directory
            os.chdir(self.source_dir)

            # Create temporary file for file list
            with tempfile.NamedTemporaryFile(mode='w', delete=False) as temp_file:
                temp_filename = temp_file.name

                # Use ripgrep to get file list, respecting .gitignore
                rg_cmd = ['rg', '--files', '--hidden']
                result = subprocess.run(rg_cmd, capture_output=True, text=True, cwd=self.source_dir)

                if result.returncode == 0:
                    temp_file.write(result.stdout)
                    temp_file.flush()

                    # Count files
                    file_count = len(result.stdout.strip().split('\n')) if result.stdout.strip() else 0
                    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Syncing {file_count} files...")

                    # Run rsync
                    rsync_cmd = [
                        'rsync', '-av',
                        '--delete',
                        '--delete-excluded',
                        f'--files-from={temp_filename}',
                        f'{self.source_dir}/',
                        f'{self.dest_dir}/'
                    ]

                    rsync_result = subprocess.run(rsync_cmd, capture_output=True, text=True)

                    if rsync_result.returncode == 0:
                        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Sync completed successfully")
                    else:
                        print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Rsync error: {rsync_result.stderr}")

                else:
                    print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Ripgrep error: {result.stderr}")

        except Exception as e:
            print(f"[{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}] Sync error: {e}")
        finally:
            # Clean up temp file
            try:
                os.unlink(temp_filename)
            except:
                pass

def check_dependencies():
    """Check if required dependencies are available"""
    dependencies = ['rg', 'rsync']
    missing = []

    for dep in dependencies:
        if subprocess.run(['which', dep], capture_output=True).returncode != 0:
            missing.append(dep)

    if missing:
        print("Error: Missing dependencies:")
        for dep in missing:
            if dep == 'rg':
                print("  - ripgrep: sudo apt install ripgrep")
            elif dep == 'rsync':
                print("  - rsync: sudo apt install rsync")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='WSL Sync Server')
    parser.add_argument('source_dir', help='Source directory to sync from')
    parser.add_argument('dest_dir', help='Destination directory to sync to')
    parser.add_argument('-p', '--port', type=int, default=8080, help='Port to listen on (default: 8080)')

    args = parser.parse_args()

    # Check dependencies
    check_dependencies()

    # Convert to absolute paths
    source_dir = os.path.abspath(args.source_dir)
    dest_dir = os.path.abspath(args.dest_dir)

    # Validate directories
    if not os.path.exists(source_dir):
        print(f"Error: Source directory does not exist: {source_dir}")
        sys.exit(1)

    # Create destination directory if it doesn't exist
    os.makedirs(dest_dir, exist_ok=True)

    print(f"Source: {source_dir}")
    print(f"Destination: {dest_dir}")
    print(f"Server port: {args.port}")

    # Perform initial sync
    print("Performing initial sync...")
    perform_sync(source_dir, dest_dir)

    # Create handler class with bound directories
    def handler_factory(*args, **kwargs):
        return SyncHandler(*args, source_dir=source_dir, dest_dir=dest_dir, **kwargs)

    # Start server
    server = HTTPServer(('localhost', args.port), handler_factory)
    print(f"Starting server on http://localhost:{args.port}")
    print("Send POST requests to /sync to trigger sync")
    print("Press Ctrl+C to stop...")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nShutting down server...")
        server.shutdown()

if __name__ == '__main__':
    main()
