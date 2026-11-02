"""Real loopback HTTP coverage; no external services or credentials required."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import base64
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib


def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))


png = (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', 8, 8, 8, 2, 0, 0, 0))
       + chunk(b'IDAT', zlib.compress((b'\0' + b'\xff\0\0' * 8) * 8)) + chunk(b'IEND', b''))


class Handler(BaseHTTPRequestHandler):
    retry_count = 0

    def log_message(self, *_):
        pass

    def do_GET(self):
        body = b'hello'
        if self.path in ('/image', '/retry'):
            body = png
        if self.path == '/retry':
            Handler.retry_count += 1
            if Handler.retry_count == 1:
                body = b'not an image'
        self.send_response(404 if self.path == '/missing' else 200)
        self.send_header('Content-Length', str(100000 if self.path == '/stall' else len(body)))
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()
        if self.path == '/stall':
            time.sleep(5)


server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
threading.Thread(target=server.serve_forever, daemon=True).start()
try:
    with tempfile.TemporaryDirectory(prefix='led-matrix-fetch-') as temporary:
        for route, mode in [('ok', 'download'), ('missing', 'download'), ('stall', 'cancel'),
                            ('image', 'post-cleanup'), ('retry', 'post-retry')]:
            output = Path(temporary) / (route + '.bin')
            start = time.monotonic()
            result = subprocess.run([sys.argv[1], f'http://127.0.0.1:{server.server_port}/{route}',
                                     str(output), mode], cwd=temporary, capture_output=True, text=True, timeout=6)
            if route == 'ok':
                assert result.returncode == 0 and output.read_bytes() == b'hello', result
            elif route in ('missing', 'stall'):
                assert result.returncode == 1 and not output.exists(), result
                if route == 'stall':
                    assert time.monotonic() - start < 4, 'cancellation did not interrupt download'
            else:
                assert result.returncode == 0, result
            print(f'{route}: passed')
finally:
    server.shutdown()
    server.server_close()
