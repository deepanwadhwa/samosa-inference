#!/usr/bin/env python3
"""Qualify catalog-selected llama installation through the native setup CLI."""
import argparse
import hashlib
import http.server
import json
import os
from pathlib import Path
import shutil
import ssl
import subprocess
import tempfile
import threading

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("setup")
parser.add_argument("root")
parser.add_argument("index")
parser.add_argument("pin")
parser.add_argument("--revision", required=True)
parser.add_argument("--kind", required=True)
parser.add_argument("--architecture", required=True)
parser.add_argument("--engine-pin", required=True)
args = parser.parse_args()
body = Path(args.index).read_bytes()
assert hashlib.sha256(body).hexdigest() == args.pin
index = json.loads(body)
assert index["recipe"] == "llama-gguf-package-v1"
root = Path(args.root)
payloads = {f["sha256"]: root / f["name"] for f in index["files"]}
payloads[args.pin] = Path(args.index)
base = Path(tempfile.mkdtemp(prefix="samosa-llama-https-", dir="/private/tmp" if Path("/private/tmp").exists() else None)).resolve()
server = thread = None
requests = []
try:
    cert, key = base / "cert.pem", base / "key.pem"
    openssl = os.environ.get("OPENSSL_BIN", "openssl")
    if subprocess.check_output([openssl, "version"], text=True).startswith("LibreSSL") and Path("/opt/homebrew/bin/openssl").exists():
        openssl = "/opt/homebrew/bin/openssl"
    subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(key), "-out", str(cert),
                    "-days", "1", "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1"],
                   check=True, capture_output=True, timeout=30)
    cert.chmod(0o600)
    key.chmod(0o600)

    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *unused):
            pass

        def do_GET(self):
            digest = self.path[1:]
            path = payloads.get(digest)
            if path is None:
                self.send_error(404)
                return
            checksum = hashlib.sha256()
            with path.open("rb") as source:
                for chunk in iter(lambda: source.read(65536), b""):
                    checksum.update(chunk)
            assert checksum.hexdigest() == digest
            requests.append(digest)
            self.send_response(200)
            self.send_header("Content-Length", str(path.stat().st_size))
            self.end_headers()
            with path.open("rb") as source:
                for chunk in iter(lambda: source.read(65536), b""):
                    self.wfile.write(chunk)

    server = http.server.HTTPServer(("127.0.0.1", 0), Handler)
    tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    tls.load_cert_chain(cert, key)
    server.socket = tls.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    endpoint = f"https://127.0.0.1:{server.server_port}"
    catalog = base / "catalog.json"
    catalog.write_text(json.dumps(dict(catalog_version=1, revision="llama-setup-qa", entries=[dict(
        id="llama-setup-qa", version="1", url=f"{endpoint}/{args.pin}", license="Local qualification; no distribution catalog",
        platform=index["platform"], minimum_os=index["minimum_os"], runtime="system-runtime-v1", recipe=index["recipe"], sha256=args.pin, size=len(body))])))
    catalog.chmod(0o600)
    package, pool = base / "package", base / "pool"
    package.mkdir(mode=0o700)
    pool.mkdir(mode=0o700)
    total = sum(f["bytes"] for f in index["files"])
    config = [os.path.abspath(args.setup), "--catalog-file", str(catalog), "--state", str(base / "cache"),
              "--byte-ceiling", str(total + len(body)), "--disk-ceiling", str(2 * (total + len(body))),
              "--artifact-capacity", "513", "--package-directory", str(package)]
    reservations = ["--reservation-directory", str(pool), "--copy-ceiling", str(total), "--reservation-ceiling", str(total), "--reservation-capacity", "1"]
    policy = ["--llama-runtime-revision", args.revision, "--llama-kind", args.kind,
              "--llama-architecture", args.architecture, "--llama-engine-sha256", args.engine_pin]

    def call(op, flags, extra=None, error=None):
        request = dict(contract_version=1, op=op, provider_id="llama-setup-qa")
        if extra:
            request.update(extra)
        result = subprocess.run(config + flags, input=json.dumps(request), text=True, capture_output=True, timeout=900)
        assert "AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr, result.stderr
        receipt = json.loads(result.stdout)
        if error:
            assert result.returncode == 1 and receipt["error"]["code"] == error, receipt
        else:
            assert result.returncode == 0 and receipt["status"] == "installed_verified" and receipt["readiness"] == "unqualified", receipt
        return receipt

    call("provider_install", reservations, error="unqualified_llama_package_runtime")
    assert not requests and not (package / "assemble.lock").exists() and not (base / "cache").exists()
    call("provider_install", reservations + policy, extra={"llama_engine_sha256": args.engine_pin}, error="invalid_request")
    call("provider_install", reservations + policy + ["--digest-endpoint", endpoint], error="artifact_tls_verification_failed")
    assert not requests and not (package / "payload").exists()
    call("provider_install", reservations + policy + ["--digest-endpoint", endpoint, "--ca-file", str(cert)])
    assert sorted(requests) == sorted(payloads), requests
    installed_engine = package / "payload" / next(f["name"] for f in index["files"] if f["role"] == "engine")
    identity = installed_engine.stat()
    assert identity.st_mode & 0o777 == 0o500
    wrong = policy.copy()
    wrong[wrong.index("--llama-kind") + 1] = "text" if args.kind == "embedding" else "embedding"
    call("provider_resume", reservations + wrong, error="unqualified_llama_package_runtime")
    assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
    call("provider_status", policy)
    call("provider_resume", reservations + policy)
    assert sorted(requests) == sorted(payloads), requests
    assert installed_engine.stat().st_ino == identity.st_ino
    print(f"PASS llama setup HTTPS: {len(index['files'])} files; {total} bytes; index {args.pin}; missing policy/public override/TLS refusal, complete publication, last-good preservation and offline status/resume")
finally:
    if server is not None:
        if thread is not None and thread.is_alive():
            server.shutdown()
            thread.join(timeout=5)
        server.server_close()
    shutil.rmtree(base)
