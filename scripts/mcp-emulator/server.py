#!/usr/bin/env python3
"""
MCP server for LED matrix emulator control.

Provides tools to start/stop the emulator, capture frames as PNG images,
and control scenes/presets via the emulator's HTTP API.

Configure in Claude Code by adding to .mcp.json:
{
  "mcpServers": {
    "led-matrix": {
      "command": "python",
      "args": ["/path/to/scripts/mcp-emulator/server.py"]
    }
  }
}
"""

import asyncio
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
from pathlib import Path

import mcp.types as types
from mcp.server import Server
from mcp.server.stdio import stdio_server
from PIL import Image, ImageDraw

# ---------------------------------------------------------------------------
# Constants / defaults
# ---------------------------------------------------------------------------

# Project root is three levels up: scripts/mcp-emulator/server.py
PROJECT_ROOT = Path(__file__).resolve().parent.parent.parent

DEFAULT_BINARY = str(PROJECT_ROOT / "emulator_build" / "install" / "main")
DEFAULT_FRAME_PATH = "/tmp/led-matrix-frame.bin"
DEFAULT_PORT = 8080

# ---------------------------------------------------------------------------
# Global emulator state
# ---------------------------------------------------------------------------

_emulator_proc: subprocess.Popen | None = None
_emulator_start_time: float | None = None
_emulator_config: dict = {}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _is_running() -> bool:
    if _emulator_proc is None:
        return False
    return _emulator_proc.poll() is None


def _read_frame_as_png(frame_path: str, scale: int = 6, show_grid: bool = True) -> str:
    """Read a raw frame binary file and return a base64-encoded PNG.

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

    pixels = data[8 : 8 + w * h * 3]
    img = Image.frombytes("RGB", (w, h), pixels)

    # Scale up so individual LEDs are clearly visible
    if scale > 1:
        img = img.resize((w * scale, h * scale), Image.NEAREST)

    # Optionally overlay a subtle grid to show LED boundaries
    if show_grid and scale >= 4:
        draw = ImageDraw.Draw(img)
        grid_color = (30, 30, 30)
        for x in range(0, w * scale, scale):
            draw.line([(x, 0), (x, h * scale - 1)], fill=grid_color, width=1)
        for y in range(0, h * scale, scale):
            draw.line([(0, y), (w * scale - 1, y)], fill=grid_color, width=1)

    buf = io.BytesIO()
    img.save(buf, "PNG")
    return base64.b64encode(buf.getvalue()).decode()


def _http_get(path: str, port: int = DEFAULT_PORT, timeout: float = 3.0) -> dict | list | str:
    url = f"http://localhost:{port}{path}"
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            body = resp.read().decode()
            try:
                return json.loads(body)
            except json.JSONDecodeError:
                return body
    except urllib.error.URLError as e:
        raise RuntimeError(f"HTTP request to {url} failed: {e}")


def _http_post(path: str, body: dict, port: int = DEFAULT_PORT, timeout: float = 3.0) -> dict | list | str:
    url = f"http://localhost:{port}{path}"
    data = json.dumps(body).encode()
    req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            response_body = resp.read().decode()
            try:
                return json.loads(response_body)
            except json.JSONDecodeError:
                return response_body
    except urllib.error.URLError as e:
        raise RuntimeError(f"HTTP POST to {url} failed: {e}")


# ---------------------------------------------------------------------------
# MCP server
# ---------------------------------------------------------------------------

server = Server("led-matrix-emulator")


@server.list_tools()
async def list_tools() -> list[types.Tool]:
    return [
        types.Tool(
            name="start_emulator",
            description=(
                "Build (if needed) and start the LED matrix emulator. "
                "In headless mode the emulator renders frames to a binary file "
                "instead of displaying an SDL window. "
                "Use get_frame to capture the rendered output."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "chain": {
                        "type": "integer",
                        "description": "Number of chained panels (cols multiplier). Default: 2",
                        "default": 2,
                    },
                    "parallel": {
                        "type": "integer",
                        "description": "Number of parallel chains (rows multiplier). Default: 2",
                        "default": 2,
                    },
                    "rows": {
                        "type": "integer",
                        "description": "Rows per panel. Default: 64",
                        "default": 64,
                    },
                    "cols": {
                        "type": "integer",
                        "description": "Columns per panel. Default: 64",
                        "default": 64,
                    },
                    "scale": {
                        "type": "integer",
                        "description": "SDL window scale factor. Default: 4",
                        "default": 4,
                    },
                    "headless": {
                        "type": "boolean",
                        "description": "Run without a visible SDL window. Default: true",
                        "default": True,
                    },
                    "port": {
                        "type": "integer",
                        "description": "HTTP API port. Default: 8080",
                        "default": 8080,
                    },
                    "frame_path": {
                        "type": "string",
                        "description": f"Path for frame export binary. Default: {DEFAULT_FRAME_PATH}",
                        "default": DEFAULT_FRAME_PATH,
                    },
                    "binary_path": {
                        "type": "string",
                        "description": f"Path to the emulator binary. Default: {DEFAULT_BINARY}",
                        "default": DEFAULT_BINARY,
                    },
                },
            },
        ),
        types.Tool(
            name="stop_emulator",
            description="Stop the running emulator process.",
            inputSchema={"type": "object", "properties": {}},
        ),
        types.Tool(
            name="get_status",
            description="Return the emulator's current status (running/stopped, PID, matrix dimensions, uptime).",
            inputSchema={"type": "object", "properties": {}},
        ),
        types.Tool(
            name="get_frame",
            description=(
                "Capture and return the latest rendered frame from the emulator as a PNG image. "
                "Each LED pixel is scaled up and a subtle grid overlay is added. "
                "The emulator must be running with frame export enabled (headless or --led-emulator-frame-export)."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "scale": {
                        "type": "integer",
                        "description": "Scale factor for the output PNG (each LED pixel → NxN PNG pixels). Default: 6",
                        "default": 6,
                    },
                    "show_grid": {
                        "type": "boolean",
                        "description": "Overlay a subtle grid to mark LED boundaries. Default: true",
                        "default": True,
                    },
                    "frame_path": {
                        "type": "string",
                        "description": f"Path to the frame binary file. Default: {DEFAULT_FRAME_PATH}",
                        "default": DEFAULT_FRAME_PATH,
                    },
                },
            },
        ),
        types.Tool(
            name="list_scenes",
            description="List all registered scenes available in the running emulator (via HTTP API).",
            inputSchema={
                "type": "object",
                "properties": {
                    "port": {
                        "type": "integer",
                        "description": "HTTP API port. Default: 8080",
                        "default": DEFAULT_PORT,
                    },
                },
            },
        ),
        types.Tool(
            name="list_presets",
            description="List all named presets (scene playlists) via the HTTP API.",
            inputSchema={
                "type": "object",
                "properties": {
                    "port": {
                        "type": "integer",
                        "description": "HTTP API port. Default: 8080",
                        "default": DEFAULT_PORT,
                    },
                },
            },
        ),
        types.Tool(
            name="set_preset",
            description="Switch the active scene preset on the running emulator.",
            inputSchema={
                "type": "object",
                "properties": {
                    "preset": {
                        "type": "string",
                        "description": "Name or ID of the preset to activate.",
                    },
                    "port": {
                        "type": "integer",
                        "description": "HTTP API port. Default: 8080",
                        "default": DEFAULT_PORT,
                    },
                },
                "required": ["preset"],
            },
        ),
        types.Tool(
            name="http_api",
            description=(
                "Make a raw HTTP request to the emulator's REST API. "
                "Useful for any endpoint not exposed by a dedicated tool."
            ),
            inputSchema={
                "type": "object",
                "properties": {
                    "method": {
                        "type": "string",
                        "description": "HTTP method: GET or POST",
                        "enum": ["GET", "POST"],
                        "default": "GET",
                    },
                    "path": {
                        "type": "string",
                        "description": "API path, e.g. /list_scenes or /set_active?preset=default",
                    },
                    "body": {
                        "type": "object",
                        "description": "JSON body for POST requests.",
                    },
                    "port": {
                        "type": "integer",
                        "description": "HTTP API port. Default: 8080",
                        "default": DEFAULT_PORT,
                    },
                },
                "required": ["path"],
            },
        ),
    ]


@server.call_tool()
async def call_tool(name: str, arguments: dict | None) -> list[types.TextContent | types.ImageContent]:
    args = arguments or {}

    # ------------------------------------------------------------------
    if name == "start_emulator":
        global _emulator_proc, _emulator_start_time, _emulator_config

        if _is_running():
            return [types.TextContent(type="text", text=f"Emulator already running (PID {_emulator_proc.pid}).")]

        chain = int(args.get("chain", 2))
        parallel = int(args.get("parallel", 2))
        rows = int(args.get("rows", 64))
        cols = int(args.get("cols", 64))
        scale = int(args.get("scale", 4))
        headless = bool(args.get("headless", True))
        port = int(args.get("port", DEFAULT_PORT))
        frame_path = str(args.get("frame_path", DEFAULT_FRAME_PATH))
        binary_path = str(args.get("binary_path", DEFAULT_BINARY))

        if not os.path.isfile(binary_path):
            return [
                types.TextContent(
                    type="text",
                    text=(
                        f"Emulator binary not found at: {binary_path}\n"
                        "Build it first with:\n"
                        f"  cd {PROJECT_ROOT} && cmake --build emulator_build --target install"
                    ),
                )
            ]

        cmd = [
            binary_path,
            f"--led-chain={chain}",
            f"--led-parallel={parallel}",
            f"--led-rows={rows}",
            f"--led-cols={cols}",
            "--led-emulator",
            f"--led-emulator-scale={scale}",
            f"--led-emulator-frame-export={frame_path}",
        ]
        if headless:
            cmd.append("--led-emulator-headless")

        env = dict(os.environ)
        env["PORT"] = str(port)

        _emulator_proc = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            env=env,
            cwd=str(PROJECT_ROOT),
        )
        _emulator_start_time = time.time()
        _emulator_config = {
            "chain": chain,
            "parallel": parallel,
            "rows": rows,
            "cols": cols,
            "width": cols * chain,
            "height": rows * parallel,
            "scale": scale,
            "headless": headless,
            "port": port,
            "frame_path": frame_path,
            "binary_path": binary_path,
        }

        # Brief wait to let the process die immediately on startup errors
        await asyncio.sleep(1.0)
        if not _is_running():
            rc = _emulator_proc.returncode
            _emulator_proc = None
            return [types.TextContent(type="text", text=f"Emulator exited immediately (return code {rc}).")]

        return [
            types.TextContent(
                type="text",
                text=(
                    f"Emulator started (PID {_emulator_proc.pid})\n"
                    f"Matrix: {cols * chain}×{rows * parallel} pixels "
                    f"({cols}×{rows} per panel, {chain} chain, {parallel} parallel)\n"
                    f"Frame export: {frame_path}\n"
                    f"HTTP API: http://localhost:{port}\n"
                    f"Headless: {headless}"
                ),
            )
        ]

    # ------------------------------------------------------------------
    elif name == "stop_emulator":
        global _emulator_proc, _emulator_start_time, _emulator_config

        if not _is_running():
            _emulator_proc = None
            return [types.TextContent(type="text", text="Emulator is not running.")]

        pid = _emulator_proc.pid
        try:
            _emulator_proc.send_signal(signal.SIGTERM)
            try:
                _emulator_proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                _emulator_proc.kill()
                _emulator_proc.wait(timeout=2)
        except ProcessLookupError:
            pass

        _emulator_proc = None
        _emulator_start_time = None
        _emulator_config = {}
        return [types.TextContent(type="text", text=f"Emulator stopped (was PID {pid}).")]

    # ------------------------------------------------------------------
    elif name == "get_status":
        if not _is_running():
            return [types.TextContent(type="text", text="Emulator is not running.")]

        uptime = ""
        if _emulator_start_time is not None:
            elapsed = int(time.time() - _emulator_start_time)
            uptime = f"{elapsed // 60}m {elapsed % 60}s"

        lines = [
            f"Status:  running",
            f"PID:     {_emulator_proc.pid}",
            f"Uptime:  {uptime}",
        ]
        if _emulator_config:
            c = _emulator_config
            lines += [
                f"Matrix:  {c.get('width')}×{c.get('height')} pixels",
                f"Port:    {c.get('port')}",
                f"Frame:   {c.get('frame_path')}",
                f"Binary:  {c.get('binary_path')}",
            ]

        # Check if frame file exists
        fp = _emulator_config.get("frame_path", DEFAULT_FRAME_PATH)
        if os.path.exists(fp):
            age = time.time() - os.path.getmtime(fp)
            lines.append(f"Frame:   available (last updated {age:.1f}s ago)")
        else:
            lines.append(f"Frame:   not yet exported (emulator may still be initialising)")

        return [types.TextContent(type="text", text="\n".join(lines))]

    # ------------------------------------------------------------------
    elif name == "get_frame":
        scale = int(args.get("scale", 6))
        show_grid = bool(args.get("show_grid", True))
        frame_path = str(args.get("frame_path", DEFAULT_FRAME_PATH))

        if not os.path.exists(frame_path):
            msg = f"Frame file not found at {frame_path}."
            if not _is_running():
                msg += " The emulator is not running — start it first with start_emulator."
            else:
                msg += " The emulator is running but hasn't exported a frame yet; try again in a moment."
            return [types.TextContent(type="text", text=msg)]

        try:
            png_b64 = _read_frame_as_png(frame_path, scale=scale, show_grid=show_grid)
        except Exception as e:
            return [types.TextContent(type="text", text=f"Failed to decode frame: {e}")]

        return [
            types.ImageContent(
                type="image",
                data=png_b64,
                mimeType="image/png",
            )
        ]

    # ------------------------------------------------------------------
    elif name == "list_scenes":
        port = int(args.get("port", DEFAULT_PORT))
        try:
            result = _http_get("/list_scenes", port=port)
        except RuntimeError as e:
            return [types.TextContent(type="text", text=str(e))]
        return [types.TextContent(type="text", text=json.dumps(result, indent=2))]

    # ------------------------------------------------------------------
    elif name == "list_presets":
        port = int(args.get("port", DEFAULT_PORT))
        try:
            result = _http_get("/list_presets", port=port)
        except RuntimeError as e:
            return [types.TextContent(type="text", text=str(e))]
        return [types.TextContent(type="text", text=json.dumps(result, indent=2))]

    # ------------------------------------------------------------------
    elif name == "set_preset":
        preset = str(args["preset"])
        port = int(args.get("port", DEFAULT_PORT))
        try:
            result = _http_get(f"/set_active?preset={urllib.parse.quote(preset)}", port=port)
        except RuntimeError as e:
            return [types.TextContent(type="text", text=str(e))]
        return [types.TextContent(type="text", text=json.dumps(result, indent=2) if isinstance(result, (dict, list)) else str(result))]

    # ------------------------------------------------------------------
    elif name == "http_api":
        method = str(args.get("method", "GET")).upper()
        path = str(args["path"])
        port = int(args.get("port", DEFAULT_PORT))
        body = args.get("body")

        try:
            if method == "POST":
                result = _http_post(path, body or {}, port=port)
            else:
                result = _http_get(path, port=port)
        except RuntimeError as e:
            return [types.TextContent(type="text", text=str(e))]

        return [types.TextContent(type="text", text=json.dumps(result, indent=2) if isinstance(result, (dict, list)) else str(result))]

    # ------------------------------------------------------------------
    else:
        return [types.TextContent(type="text", text=f"Unknown tool: {name}")]


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

async def main():
    async with stdio_server() as (read_stream, write_stream):
        await server.run(
            read_stream,
            write_stream,
            server.create_initialization_options(),
        )


if __name__ == "__main__":
    asyncio.run(main())
