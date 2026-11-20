#!/usr/bin/env python3
"""Write a Raspberry Pi Imager repository file (os_list) for one panel image.

usage: imager_repo.py --image led-matrix-<v>-arm64.img.xz --url <download URL>
                      --version <v> [--icon <URL>] [--date YYYY-MM-DD] --out <json>

Imager needs the sizes and SHA-256 of both the download and the extracted image;
the latter is computed by decompressing the image once (streamed, nothing is
written). "init_format": "cloudinit-rpi" makes Imager offer its customisation
(user, Wi-Fi, SSH) and write it as cloud-init files, which the image applies
on the first boot (docs/PI_IMAGE.md).
"""
import argparse
import datetime
import hashlib
import json
import lzma
from pathlib import Path

CHUNK = 4 * 1024 * 1024


def digest(path: Path, opener):
    h, size = hashlib.sha256(), 0
    with opener(path) as f:
        while chunk := f.read(CHUNK):
            h.update(chunk)
            size += len(chunk)
    return size, h.hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--image', required=True, type=Path)
    p.add_argument('--url', required=True)
    p.add_argument('--version', required=True)
    p.add_argument('--icon', default='https://raw.githubusercontent.com/sshcrack/led-matrix/master/react-web/public/icon-192.png')
    p.add_argument('--date', default=datetime.date.today().isoformat())
    p.add_argument('--out', required=True, type=Path)
    a = p.parse_args()

    download_size, download_sha = digest(a.image, lambda x: open(x, 'rb'))
    extract_size, extract_sha = digest(a.image, lzma.open)
    entry = {
        'name': f'LED Matrix {a.version}',
        'description': 'Ready-to-run LED matrix panel: read-only system, safe to unplug. '
                       'Open http://led-matrix.local:8080 after the first boot.',
        'url': a.url,
        'icon': a.icon,
        'website': 'https://github.com/sshcrack/led-matrix',
        'release_date': a.date,
        'extract_size': extract_size,
        'extract_sha256': extract_sha,
        'image_download_size': download_size,
        'image_download_sha256': download_sha,
        # The LED library drives the Pi 3 and 4 (and Zero 2 W); the Pi 5 has a different GPIO block.
        'devices': ['pi4-64bit', 'pi3-64bit'],
        'init_format': 'cloudinit-rpi',
        'architecture': 'armv8',
    }
    a.out.write_text(json.dumps({'os_list': [entry]}, indent=2) + '\n')


if __name__ == '__main__':
    main()
