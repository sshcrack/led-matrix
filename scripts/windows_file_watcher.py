#!/usr/bin/env python3
"""
Windows File Watcher Client - Watches for file changes and notifies WSL sync server
"""

import os
import sys
import json
import time
import requests
import subprocess
import argparse
import fnmatch
from datetime import datetime
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import threading

class GitignoreParser:
    """Parser for .gitignore files"""

    def __init__(self, gitignore_path):
        self.patterns = []
        self.load_gitignore(gitignore_path)

    def load_gitignore(self, gitignore_path):
        """Load patterns from .gitignore file"""
        if not os.path.exists(gitignore_path):
            print(f"Warning: .gitignore file not found at {gitignore_path}")
            return

        try:
            with open(gitignore_path, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()

                    # Skip empty lines and comments
                    if not line or line.startswith('#'):
                        continue

                    # Handle negation patterns (lines starting with !)
                    negation = line.startswith('!')
                    if negation:
                        line = line[1:]

                    self.patterns.append({
                        'pattern': line,
                        'negation': negation
                    })

        except Exception as e:
            print(f"Error reading .gitignore file: {e}")

    def is_ignored(self, filepath, base_path):
        """Check if a file should be ignored based on gitignore patterns"""
        # Convert to relative path from base directory
        try:
            rel_path = os.path.relpath(filepath, base_path).replace('\\', '/')
        except ValueError:
            # If we can't get relative path, assume it's not ignored
            return False

        ignored = False

        for pattern_info in self.patterns:
            pattern = pattern_info['pattern']
            negation = pattern_info['negation']

            # Handle directory patterns (ending with /)
            if pattern.endswith('/'):
                pattern = pattern[:-1]
                # Check if any parent directory matches
                path_parts = rel_path.split('/')
                for i in range(len(path_parts)):
                    partial_path = '/'.join(path_parts[:i+1])
                    if fnmatch.fnmatch(partial_path, pattern):
                        if negation:
                            ignored = False
                        else:
                            ignored = True
                        break
            else:
                # Check if file matches pattern
                if fnmatch.fnmatch(rel_path, pattern):
                    if negation:
                        ignored = False
                    else:
                        ignored = True

                # Also check if any parent directory matches
                path_parts = rel_path.split('/')
                for i in range(len(path_parts)):
                    partial_path = '/'.join(path_parts[:i+1])
                    if fnmatch.fnmatch(partial_path, pattern):
                        if negation:
                            ignored = False
                        else:
                            ignored = True
                        break

        return ignored

class SyncEventHandler(FileSystemEventHandler):
    def __init__(self, server_url, watch_dir, debounce_time=2.0):
        self.server_url = server_url
        self.watch_dir = watch_dir
        self.debounce_time = debounce_time
        self.last_sync_time = 0
        self.sync_timer = None
        self.lock = threading.Lock()

        # Initialize gitignore parser
        gitignore_path = os.path.join(watch_dir, '.gitignore')
        self.gitignore_parser = GitignoreParser(gitignore_path)

    def on_any_event(self, event):
        # Skip directory events and temporary files
        if event.is_directory:
            return

        # Skip certain file types and patterns
        if self._should_ignore_file(event.src_path):
            return

        print(f"[{datetime.now().strftime('%H:%M:%S')}] File change detected: {event.src_path}")

        # Debounce rapid changes
        with self.lock:
            if self.sync_timer:
                self.sync_timer.cancel()

            self.sync_timer = threading.Timer(self.debounce_time, self._trigger_sync)
            self.sync_timer.start()

    def _should_ignore_file(self, filepath):
        """Check if file should be ignored based on gitignore and common patterns"""

        # First check gitignore patterns
        if self.gitignore_parser.is_ignored(filepath, self.watch_dir):
            return True

        # Additional common ignore patterns for temporary files
        common_ignore_patterns = [
            '.tmp', '.temp', '.swp', '.swo', '~',
            '.DS_Store', 'Thumbs.db', '.git'
        ]

        common_ignore_extensions = [
            '.tmp', '.temp', '.cache', '.log'
        ]

        # Convert to forward slashes for consistent pattern matching
        normalized_path = filepath.replace('\\', '/')

        # Check common patterns
        for pattern in common_ignore_patterns:
            if pattern in normalized_path:
                return True

        # Check extensions
        _, ext = os.path.splitext(filepath)
        if ext.lower() in common_ignore_extensions:
            return True

        return False

    def _trigger_sync(self):
        """Send sync request to server"""
        try:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Triggering sync...")

            response = requests.post(
                f"{self.server_url}/sync",
                timeout=10,
                headers={'Content-Type': 'application/json'}
            )

            if response.status_code == 200:
                result = response.json()
                print(f"[{datetime.now().strftime('%H:%M:%S')}] Sync triggered successfully")
            else:
                print(f"[{datetime.now().strftime('%H:%M:%S')}] Sync request failed: {response.status_code}")

        except requests.exceptions.RequestException as e:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Error contacting server: {e}")
        except Exception as e:
            print(f"[{datetime.now().strftime('%H:%M:%S')}] Unexpected error: {e}")

def check_server_health(server_url):
    """Check if the sync server is running"""
    try:
        response = requests.get(f"{server_url}/health", timeout=5)
        if response.status_code == 200:
            health_data = response.json()
            print(f"Server is healthy:")
            print(f"  Source: {health_data.get('source')}")
            print(f"  Destination: {health_data.get('dest')}")
            return True
        else:
            print(f"Server health check failed: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"Cannot connect to server: {e}")
        return False

def check_dependencies():
    """Check if required Python packages are available"""
    try:
        import watchdog
        import requests
    except ImportError as e:
        print(f"Error: Missing Python package: {e}")
        print("Install with: pip install watchdog requests")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description='Windows File Watcher Client')
    parser.add_argument('watch_dir', help='Directory to watch for changes')
    parser.add_argument('-s', '--server', default='http://localhost:8080',
                       help='Sync server URL (default: http://localhost:8080)')
    parser.add_argument('-d', '--debounce', type=float, default=2.0,
                       help='Debounce time in seconds (default: 2.0)')
    parser.add_argument('--no-health-check', action='store_true',
                       help='Skip initial server health check')

    args = parser.parse_args()

    # Check dependencies
    check_dependencies()

    # Validate watch directory
    watch_dir = os.path.abspath(args.watch_dir)
    if not os.path.exists(watch_dir):
        print(f"Error: Watch directory does not exist: {watch_dir}")
        sys.exit(1)

    print(f"File Watcher Client")
    print(f"Watch directory: {watch_dir}")
    print(f"Server URL: {args.server}")
    print(f"Debounce time: {args.debounce}s")

    # Check server health
    if not args.no_health_check:
        print("Checking server health...")
        if not check_server_health(args.server):
            print("Server health check failed. Use --no-health-check to skip this check.")
            sys.exit(1)

    # Create event handler and observer
    event_handler = SyncEventHandler(args.server, watch_dir, args.debounce)
    observer = Observer()
    observer.schedule(event_handler, watch_dir, recursive=True)

    # Start watching
    print("Starting file watcher...")
    print("Press Ctrl+C to stop...")

    observer.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping file watcher...")
        observer.stop()

        # Cancel any pending sync timer
        if event_handler.sync_timer:
            event_handler.sync_timer.cancel()

    observer.join()
    print("File watcher stopped.")

if __name__ == '__main__':
    main()
