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
https = "--https" in sys.argv[4:]
assert sys.argv[4:] in ([], ["--https"]) or (https and len(sys.argv) == 7 and sys.argv[5] == "--setup")
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
    expected_downloads = len(served)
    if "--setup" in sys.argv:
        setup = os.path.abspath(sys.argv[sys.argv.index("--setup") + 1])
        package_dir = destination / "setup-package"
        pool = destination / "setup-reservations"
        package_dir.mkdir(mode=0o700)
        pool.mkdir(mode=0o700)
        config = [setup, "--catalog-file", str(destination / "catalog.json"),
                  "--state", str(destination / "setup-artifacts"),
                  "--byte-ceiling", str(len(body) + sum(f["bytes"] for f in files)),
                  "--disk-ceiling", str(2 * (len(body) + sum(f["bytes"] for f in files))),
                  "--artifact-capacity", "513", "--package-directory", str(package_dir)]
        before_setup = len(requests)
        for extra in ({"package_directory": str(package_dir)}, {"digest_endpoint": "https://override.invalid"}):
            invalid = subprocess.run(config, input=json.dumps({"contract_version": 1, "op": "provider_install", "provider_id": "speech-fixture", **extra}),
                                     text=True, capture_output=True, timeout=60)
            assert invalid.returncode == 1 and json.loads(invalid.stdout)["error"]["code"] == "invalid_request", invalid
        package_dir.chmod(0o755)
        unsafe = subprocess.run(config, input=json.dumps({"contract_version": 1, "op": "provider_status", "provider_id": "speech-fixture"}),
                                text=True, capture_output=True, timeout=60)
        assert unsafe.returncode == 1 and json.loads(unsafe.stdout)["error"]["code"] == "invalid_package_directory", unsafe
        package_dir.chmod(0o700)
        assert len(requests) == before_setup and not (destination / "setup-artifacts").exists()
        for op in ("provider_install", "provider_status", "provider_resume"):
            invocation = config.copy()
            if op != "provider_status":
                invocation += ["--reservation-directory", str(pool), "--copy-ceiling", str(sum(f["bytes"] for f in files)),
                               "--reservation-ceiling", str(sum(f["bytes"] for f in files)), "--reservation-capacity", "1"]
            if op == "provider_install":
                # Exercise a genuinely cold cache through public ID selection.
                invocation += ["--digest-endpoint", f"https://127.0.0.1:{server.server_port}", "--ca-file", str(cert)]
                untrusted = subprocess.run(invocation[:-2], input=json.dumps({"contract_version": 1, "op": op, "provider_id": "speech-fixture"}),
                                           text=True, capture_output=True, timeout=60)
                assert untrusted.returncode == 1 and json.loads(untrusted.stdout)["error"]["code"] == "artifact_tls_verification_failed", untrusted
                assert not (package_dir / "payload").exists()
            response = subprocess.run(invocation, input=json.dumps({"contract_version": 1, "op": op, "provider_id": "speech-fixture"}),
                                      text=True, capture_output=True, timeout=60)
            assert response.returncode == 0, response.stdout + response.stderr
            assert "AddressSanitizer" not in response.stderr and "runtime error:" not in response.stderr
            receipt = json.loads(response.stdout)
            assert receipt["status"] == "installed_verified" and receipt["readiness"] == "unqualified", receipt
            assert len(requests) - before_setup == len(served), requests
        expected_downloads += len(served)
        release_config = config.copy()
        release_config[release_config.index("--state") + 1] = str(destination / "remote-artifacts")
        release_config[-1] = str(destination / "empty-package")
        release_config += ["--reservation-directory", str(destination / "empty-reservations")]
        for expected in ("reservation_released", "reservation_absent"):
            released = subprocess.run(release_config, input=json.dumps({"contract_version": 1, "op": "provider_release_empty", "provider_id": "speech-fixture"}),
                                      text=True, capture_output=True, timeout=60)
            assert released.returncode == 0 and json.loads(released.stdout)["status"] == expected, released
        installed_release = subprocess.run(config + ["--reservation-directory", str(pool)],
                                           input=json.dumps({"contract_version": 1, "op": "provider_release_empty", "provider_id": "speech-fixture"}), text=True, capture_output=True, timeout=60)
        assert installed_release.returncode == 1 and json.loads(installed_release.stdout)["error"]["code"] == "package_reservation_not_empty", installed_release
        assert (package_dir / "complete").is_file()
        print("PASS complete setup CLI: cold HTTPS index/payload acquisition, TLS refusal and offline status/resume")
    if https:
        assert served == {f["sha256"] for f in files} | {hashlib.sha256(body).hexdigest()}, served
        assert len(requests) == expected_downloads, requests
    print(result.stdout, end="")
finally:
    if server is not None:
        if thread is not None and thread.is_alive():
            server.shutdown()
            thread.join(timeout=5)
        server.server_close()
    shutil.rmtree(base)
