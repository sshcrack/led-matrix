#!/usr/bin/env python3
"""
Generate preview PNG images for every scene registered in the LED matrix emulator.

Usage:
    python generate_previews.py [options]

Options:
    --binary PATH       Path to the emulator binary
                        (default: emulator_build/install/main)
    --output DIR        Directory to write PNG files into
                        (default: emulator_build/install/previews)
    --port PORT         HTTP API port used by the emulator (default: 18080)
    --wait SECONDS      Time to wait for a scene to render before capturing
                        (default: 3)
    --startup SECONDS   Time to wait for the emulator to start up (default: 5)
    --force             Re-generate previews that already exist
    --chain N           Number of chained panels (default: 2)
    --parallel N        Number of parallel chains (default: 2)
    --rows N            Rows per panel (default: 64)
    --cols N            Columns per panel (default: 64)
    --scale N           Scale factor for output PNG (default: 4)
"""

import argparse
import base64
import io
import json
import os
import signal
import struct
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("ERROR: Pillow is required. Install it with: pip install Pillow", file=sys.stderr)
    sys.exit(1)

PROJECT_ROOT = Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _http_get(path: str, port: int, timeout: float = 5.0):
    url = f"http://localhost:{port}{path}"
    with urllib.request.urlopen(url, timeout=timeout) as resp:
        return json.loads(resp.read().decode())


def _http_post(path: str, body: dict, port: int, timeout: float = 5.0):
    url = f"http://localhost:{port}{path}"
    data = json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode())


def _wait_for_api(port: int, max_wait: float = 30.0) -> bool:
    """Return True once the HTTP API responds, False if timed out."""
    deadline = time.time() + max_wait
    while time.time() < deadline:
        try:
            _http_get("/status", port=port, timeout=2.0)
            return True
        except Exception:
            time.sleep(0.5)
    return False


def _read_frame_as_png(frame_path: str, scale: int = 4) -> bytes:
    """Read a raw frame binary file and return PNG bytes.

    Frame binary format:
      [width:  uint32 LE]
      [height: uint32 LE]
      [R, G, B bytes × width × height]
    """
    with open(frame_path, "rb") as f:
        data = f.read()

    if len(data) < 8:
        raise ValueError(f"Frame file too small ({len(data)} bytes)")

    w, h = struct.unpack_from("<II", data, 0)
    expected = 8 + w * h * 3
    if len(data) < expected:
        raise ValueError(f"Frame file truncated: expected {expected} bytes, got {len(data)}")

    pixels = data[8: 8 + w * h * 3]
    img = Image.frombytes("RGB", (w, h), pixels)

    if scale > 1:
        img = img.resize((w * scale, h * scale), Image.NEAREST)

    buf = io.BytesIO()
    img.save(buf, "PNG")
    return buf.getvalue()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def parse_args():
    p = argparse.ArgumentParser(description="Generate scene preview images using the LED matrix emulator")
    p.add_argument("--binary", default=str(PROJECT_ROOT / "emulator_build" / "install" / "main"))
    p.add_argument("--output", default=str(PROJECT_ROOT / "emulator_build" / "install" / "previews"))
    p.add_argument("--port", type=int, default=18080)
    p.add_argument("--wait", type=float, default=3.0, help="Seconds to wait after activating a scene before capturing")
    p.add_argument("--startup", type=float, default=5.0, help="Extra seconds to wait after emulator starts")
    p.add_argument("--force", action="store_true", help="Re-generate previews even if they already exist")
    p.add_argument("--chain", type=int, default=2)
    p.add_argument("--parallel", type=int, default=2)
    p.add_argument("--rows", type=int, default=64)
    p.add_argument("--cols", type=int, default=64)
    p.add_argument("--scale", type=int, default=4)
    return p.parse_args()


def main():
    args = parse_args()

    binary_path = args.binary
    output_dir = Path(args.output)
    port = args.port
    frame_path = f"/tmp/led-matrix-preview-frame-{port}.bin"

    # Validate binary
    if not os.path.isfile(binary_path):
        print(f"ERROR: Emulator binary not found at: {binary_path}", file=sys.stderr)
        print("Build the emulator first with:", file=sys.stderr)
        print(f"  cmake --build emulator_build --target install", file=sys.stderr)
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)

    # Start the emulator in headless mode
    cmd = [
        binary_path,
        f"--led-chain={args.chain}",
        f"--led-parallel={args.parallel}",
        f"--led-rows={args.rows}",
        f"--led-cols={args.cols}",
        "--led-emulator",
        "--led-emulator-headless",
        f"--led-emulator-scale=1",
        f"--led-emulator-frame-export={frame_path}",
    ]

    env = dict(os.environ)
    env["PORT"] = str(port)

    print(f"Starting emulator on port {port}…")
    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        env=env,
        cwd=str(PROJECT_ROOT),
    )

    try:
        # Wait for API to become available
        print(f"Waiting up to {args.startup + 25}s for emulator API…")
        if not _wait_for_api(port, max_wait=args.startup + 25):
            print("ERROR: Emulator did not start in time.", file=sys.stderr)
            proc.terminate()
            sys.exit(1)

        # Extra settle time
        if args.startup > 0:
            time.sleep(args.startup)

        # Fetch scene list
        try:
            scenes = _http_get("/list_scenes", port=port)
        except Exception as e:
            print(f"ERROR: Could not fetch scene list: {e}", file=sys.stderr)
            proc.terminate()
            sys.exit(1)

        scene_names = [s["name"] for s in scenes]
        print(f"Found {len(scene_names)} scene(s): {', '.join(scene_names)}")

        generated = 0
        skipped = 0

        for scene_name in scene_names:
            out_file = output_dir / f"{scene_name}.png"

            if out_file.exists() and not args.force:
                print(f"  [skip] {scene_name} (already exists)")
                skipped += 1
                continue

            print(f"  [gen]  {scene_name}…", end=" ", flush=True)

            # Build a single-scene preset and activate it
            preset_id = f"__preview_{uuid.uuid4().hex[:8]}"
            preset_body = {
                "scenes": [
                    {
                        "type": scene_name,
                        "uuid": str(uuid.uuid4()),
                        "arguments": {},
                    }
                ],
                "transition_duration": 0,
                "transition_name": "blend",
            }

            try:
                _http_post(f"/preset?id={urllib.parse.quote(preset_id)}", preset_body, port=port)
                _http_get(f"/set_active?id={urllib.parse.quote(preset_id)}", port=port)
            except Exception as e:
                print(f"FAILED (API error: {e})")
                continue

            # Wait for the scene to render
            time.sleep(args.wait)

            # Capture frame
            if not os.path.exists(frame_path):
                print("FAILED (no frame exported)")
                continue

            try:
                png_bytes = _read_frame_as_png(frame_path, scale=args.scale)
                out_file.write_bytes(png_bytes)
                print(f"OK ({out_file.stat().st_size} bytes)")
                generated += 1
            except Exception as e:
                print(f"FAILED ({e})")

        print(f"\nDone. Generated: {generated}, Skipped: {skipped}, Total: {len(scene_names)}")
        print(f"Previews written to: {output_dir}")

    finally:
        print("Stopping emulator…")
        try:
            proc.send_signal(signal.SIGTERM)
            proc.wait(timeout=5)
        except Exception:
            proc.kill()
            proc.wait()

        # Clean up temp frame file
        try:
            os.unlink(frame_path)
        except FileNotFoundError:
            pass


if __name__ == "__main__":
    main()
