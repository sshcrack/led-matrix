#!/usr/bin/env python3
"""Live preset-switch regression probe (requires Python websockets).

Run against an emulator or Pi with its desktop connected:
  python tests/video_preset_api_integration.py --api http://ledmat:8080 \
      --client-id <desktop-client-id> --preset 'Driving Monke' --preset Campfire

Temporarily activates the selected presets, checks that each URL is dispatched
and that real composed video frames arrive, then restores the original mode.
The observer joins as a scene worker of the existing desktop; it never becomes
an independent producer or sends frames. LED_MATRIX_API_TOKEN is supported.
Offline CTest integration cases cover delayed replies, overlap, cancellation,
cache returns and decoded pixel identity without YouTube/network dependencies.
"""
import argparse
import asyncio
import hashlib
import json
import os
import struct
import urllib.parse
import urllib.request

import websockets


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--api', required=True)
    parser.add_argument('--client-id', required=True)
    parser.add_argument('--preset', action='append', required=True)
    parser.add_argument('--timeout', type=float, default=35)
    parser.add_argument('--cycles', type=int, default=2)
    args = parser.parse_args()
    api = args.api.rstrip('/')
    token = os.environ.get('LED_MATRIX_API_TOKEN')
    headers = {'Authorization': 'Bearer ' + token} if token else {}

    def request(path):
        req = urllib.request.Request(api + path, headers=headers)
        with urllib.request.urlopen(req, timeout=5) as response:
            return json.load(response)

    presets = request('/list_presets')
    selected = []
    for name in args.preset:
        matches = [(key, value) for key, value in presets.items()
                   if key == name or value.get('display_name') == name]
        if len(matches) != 1:
            raise RuntimeError('Preset must match exactly one ID or name: ' + name)
        key, value = matches[0]
        scenes = value.get('scenes', [])
        if len(scenes) != 1 or scenes[0]['type'] != 'video':
            raise RuntimeError('Use presets containing one Video scene: ' + name)
        urls = scenes[0]['arguments'].get('video_urls', [])
        if len(urls) != 1:
            raise RuntimeError('Use a single-URL playlist for this probe: ' + name)
        selected.append((key, value.get('display_name', key), urls[0]))
    original = request('/status')
    base_ws = api.replace('https://', 'wss://', 1).replace('http://', 'ws://', 1)
    auth_query = '&token=' + urllib.parse.quote(token) if token else ''

    async def run():
        path = '/desktopWebsocket?role=scene-worker&client_id=' + urllib.parse.quote(args.client_id)
        commands = []
        async with websockets.connect(base_ws + path + auth_query) as observer:
            async def receive():
                async for message in observer:
                    if isinstance(message, str) and message.startswith('msg:Video:'):
                        commands.append(message.removeprefix('msg:Video:'))
            listener = asyncio.create_task(receive())
            try:
                await asyncio.sleep(.2)
                live_url = base_ws + '/live_frame' + ('?' + auth_query[1:] if token else '')
                async with websockets.connect(live_url) as live:
                    for _ in range(args.cycles):
                        for key, name, url in selected:
                            marker = len(commands)
                            await asyncio.to_thread(request, '/set_active?id=' + urllib.parse.quote(key))
                            deadline = asyncio.get_running_loop().time() + 3
                            while 'url:' + url not in commands[marker:]:
                                if listener.done():
                                    await listener
                                if asyncio.get_running_loop().time() >= deadline:
                                    raise AssertionError(name + ': new URL was never dispatched')
                                await asyncio.sleep(.02)
                            deadline = asyncio.get_running_loop().time() + args.timeout
                            consecutive_video = 0
                            signatures = set()
                            while asyncio.get_running_loop().time() < deadline:
                                await live.send('next')
                                frame = await asyncio.wait_for(live.recv(), 5)
                                magic, width, height, _ = struct.unpack('<4sHHI', frame[:12])
                                assert magic == b'LMF1'
                                pixels = frame[12:]
                                # LoadingAnimation has a completely black interior.
                                interior = b''.join(pixels[3 * (y * width + 1):3 * (y * width + width - 1)]
                                                    for y in range(1, height - 1))
                                lit = sum(any(interior[i:i + 3]) for i in range(0, len(interior), 3))
                                if lit > (width - 2) * (height - 2) // 8:
                                    consecutive_video += 1
                                    signatures.add(hashlib.sha256(interior).digest())
                                else:
                                    consecutive_video = 0
                                if consecutive_video >= 8 and len(signatures) >= 2:
                                    break
                                await asyncio.sleep(.5)
                            else:
                                raise AssertionError(name + ': no sustained, moving video after switching')
                            print('PASS:', name, 'URL dispatched and moving composed frames received', flush=True)
            finally:
                listener.cancel()

    try:
        asyncio.run(run())
    finally:
        request('/set_active?id=' + urllib.parse.quote(original['current']))
        request('/operation_mode?mode=' + original['operation_mode'])
        print('Restored original preset and operation mode', flush=True)


if __name__ == '__main__':
    main()
