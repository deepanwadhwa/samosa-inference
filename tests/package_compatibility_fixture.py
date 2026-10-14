#!/usr/bin/env python3
"""Qualified native-format fixture package through concrete recipe dispatch."""
import hashlib
import http.server
import ssl
import threading
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

binary, exporter, library = map(os.path.abspath, sys.argv[1:4])
https = len(sys.argv) == 5 and sys.argv[4] == "--https"
assert len(sys.argv) == 4 or https
server = thread = None
package = json.loads(subprocess.check_output([exporter, "--export-package", library]))
base, root = Path(package["base"]), Path(package["package"])
try:
    raw = (root / "manifest.json").read_bytes()
    assert hashlib.sha256(raw).hexdigest() == package["manifest_sha256"]
    native = json.loads(raw)
    files = [dict(name=f["name"], role="library" if f["role"] in ("library", "dependency") else
                  "data" if f["role"].startswith("reference_") or f["role"] == "kokoro_data" else "model",
                  bytes=f["bytes"], sha256=f["sha256"]) for f in native["files"]]
    files.append(dict(name="manifest.json", role="metadata", bytes=len(raw), sha256=package["manifest_sha256"]))
    body = json.dumps(dict(format="samosa.inference.package.v1", platform=native["platform"],
                           minimum_os="1.0", recipe="sherpa-speech-package-v1", files=files)).encode()
    index = base / "index.json"
    index.write_bytes(body)
    index.chmod(0o600)
    destination = base / "publication"
    destination.mkdir(mode=0o700)
    arguments = [binary, str(root), str(index), hashlib.sha256(body).hexdigest(), str(destination)]
    served = set()
    requests = []
    if https:
        openssl = os.environ.get("OPENSSL_BIN", "openssl")
        version = subprocess.check_output([openssl, "version"], text=True)
        if version.startswith("LibreSSL") and Path("/opt/homebrew/bin/openssl").exists():
            openssl = "/opt/homebrew/bin/openssl"
        cert, key = base / "cert.pem", base / "key.pem"
        subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(key),
                        "-out", str(cert), "-days", "1", "-subj", "/CN=localhost", "-addext",
                        "subjectAltName=DNS:localhost,IP:127.0.0.1"], check=True, capture_output=True, timeout=30)
        cert.chmod(0o600)
        key.chmod(0o600)
        payloads = {f["sha256"]: root / f["name"] for f in files}
        payloads[hashlib.sha256(body).hexdigest()] = index
        class Handler(http.server.BaseHTTPRequestHandler):
            def log_message(self, *unused):
                pass
            def do_GET(self):
                digest = self.path[1:]
                path = payloads.get(digest)
                if path is None:
                    self.send_error(404)
                    return
                data = path.read_bytes()
                assert hashlib.sha256(data).hexdigest() == digest
                served.add(digest)
                requests.append(digest)
                self.send_response(200)
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)
        server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(cert, key)
        server.socket = tls.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        arguments += [f"https://127.0.0.1:{server.server_port}", str(cert)]
    result = subprocess.run(arguments, text=True, capture_output=True, timeout=60)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert "AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr
    if https:
        assert served == {f["sha256"] for f in files} | {hashlib.sha256(body).hexdigest()}, served
        assert len(requests) == len(served), requests
    print(result.stdout, end="")
finally:
    if server is not None:
        if thread is not None and thread.is_alive():
            server.shutdown()
            thread.join(timeout=5)
        server.server_close()
    shutil.rmtree(base)
