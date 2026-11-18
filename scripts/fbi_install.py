"""Installs .cia files on a 3DS over the network with FBI (spec 009).

On the console: FBI -> Remote Install -> Receive URLs over the network (it shows the console's IP).
On the PC:      python -I scripts/fbi_install.py <3ds-ip> build/3ds-release/dist/<game>/<game>.cia [...]

The script serves only the given files over HTTP on the PC's address towards the console, sends their URLs
to FBI (port 5000, the protocol of FBI's own servefiles.py: a 4-byte big-endian length, then the URLs
separated by newlines), waits until FBI has downloaded them, and stops. Local network only.
"""

import http.server
import os
import socket
import struct
import sys
import threading
import urllib.parse


def local_address(console):
    """The PC's address on the route to the console (no packet is sent)."""
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.connect((console, 5000))
        return s.getsockname()[0]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    console, paths = sys.argv[1], [os.path.abspath(p) for p in sys.argv[2:]]
    for p in paths:
        if not os.path.isfile(p):
            print(f"not a file: {p}")
            return 1
    files = {"/" + urllib.parse.quote(os.path.basename(p)): p for p in paths}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_GET(self):  # noqa: N802 (http.server's naming)
            path = files.get(self.path)
            if path is None:
                self.send_error(404)
                return
            self.send_response(200)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(os.path.getsize(path)))
            self.end_headers()
            with open(path, "rb") as f:
                while chunk := f.read(64 * 1024):
                    self.wfile.write(chunk)
            print(f"  sent {os.path.basename(path)} to {self.client_address[0]}")

        def log_message(self, *args):
            pass

    address = local_address(console)
    server = http.server.ThreadingHTTPServer((address, 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    port = server.server_address[1]
    urls = [f"http://{address}:{port}{name}" for name in files]
    payload = "\n".join(urls).encode("ascii")
    print(f"serving on http://{address}:{port}; sending {len(urls)} URL(s) to FBI at {console}:5000")
    try:
        with socket.create_connection((console, 5000), timeout=10) as sock:
            sock.sendall(struct.pack("!L", len(payload)) + payload)
            sock.settimeout(600)
            sock.recv(1)  # FBI answers (or closes the connection) when it has finished
    except OSError as e:
        print(f"could not reach FBI at {console}:5000 ({e}); is it on 'Receive URLs over the network'?")
        return 1
    finally:
        server.shutdown()
    print("done: install or check it on the console")
    return 0


if __name__ == "__main__":
    sys.exit(main())
