#!/usr/bin/env python3
"""Qualify catalog-selected native package installation through the setup CLI."""
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
import time


def run_stage(label, command, **kwargs):
    # Report long model hashing/execution without logging private flags or credentials.
    print(f"BEGIN {label}", flush=True)
    started = time.monotonic()
    try:
        return subprocess.run(command, **kwargs)
    finally:
        # END reports timing only; assertions below determine qualification.
        print(f"END {label}: {time.monotonic() - started:.3f}s", flush=True)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("setup")
parser.add_argument("root")
parser.add_argument("index")
parser.add_argument("pin")
parser.add_argument("--revision")
parser.add_argument("--decision-profile", choices=["text", "image"])
parser.add_argument("--runtime-pin")
parser.add_argument("--kind")
parser.add_argument("--architecture")
parser.add_argument("--engine-pin", required=True)
parser.add_argument("--config-pin")
parser.add_argument("--tokenizer-pin")
parser.add_argument("--weights-pin")
parser.add_argument("--kernels-pin")
parser.add_argument("--manifest-pin")
parser.add_argument("--reject-model", action="store_true", help="Controlled Molmo fixture must refuse its invalid native manifest")
parser.add_argument("--profile", help="Whisper model profile")
parser.add_argument("--model-pin", help="Separate host-qualified Whisper model pin")
parser.add_argument("--temporal-cli")
parser.add_argument("--temporal-test")
parser.add_argument("--vision-cli")
parser.add_argument("--vision-test")
parser.add_argument("--image")
parser.add_argument("--whisper-cli")
parser.add_argument("--whisper-test")
parser.add_argument("--sources-cli")
parser.add_argument("--audio")
parser.add_argument("--summary-cli", help="Optional actual T5 provider CLI qualification")
parser.add_argument("--summary-test", help="Actual-model summary harness; used with --summary-cli")
args = parser.parse_args()
assert bool(args.summary_cli) == bool(args.summary_test)
assert not args.summary_cli or args.kind == "summary"
body = Path(args.index).read_bytes()
assert hashlib.sha256(body).hexdigest() == args.pin
index = json.loads(body)
assert index["recipe"] in ("llama-gguf-package-v1", "whisper-ggml-package-v1", "visionpsy-package-v1", "molmo2-mlx-package-v1", "opendecision-package-v1")
is_decision = index["recipe"] == "opendecision-package-v1"
is_whisper = index["recipe"] == "whisper-ggml-package-v1"
is_vision = index["recipe"] == "visionpsy-package-v1"
is_molmo = index["recipe"] == "molmo2-mlx-package-v1"
assert not args.reject_model or is_molmo
if is_decision:
    assert args.decision_profile and args.revision and args.runtime_pin and args.model_pin and args.tokenizer_pin
else:
    assert (args.manifest_pin and args.kernels_pin) if is_molmo else ((args.config_pin and args.tokenizer_pin and args.weights_pin and args.kernels_pin) if is_vision else (args.revision and ((args.profile and args.model_pin) if is_whisper else (args.kind and args.architecture))))
temporal_test_args = (args.temporal_cli, args.temporal_test)
assert not any(temporal_test_args) or (is_molmo and all(temporal_test_args) and not args.reject_model)
vision_test_args = (args.vision_cli, args.vision_test, args.image)
assert not any(vision_test_args) or (is_vision and all(vision_test_args))
whisper_test_args = (args.whisper_cli, args.whisper_test, args.sources_cli, args.audio)
assert not any(whisper_test_args) or (is_whisper and all(whisper_test_args))
root = Path(args.root)
payloads = {f["sha256"]: root / f["name"] for f in index["files"]}
payloads[args.pin] = Path(args.index)
base = Path(tempfile.mkdtemp(prefix="samosa-llama-https-", dir="/private/tmp" if Path("/private/tmp").exists() else None)).resolve()
server = thread = None
requests = []
try:
    # A long actual-model gate must keep testing one executable even when the
    # development build directory is rebuilt between setup invocations.
    setup_snapshot = base / "setup"
    shutil.copyfile(os.path.abspath(args.setup), setup_snapshot)
    setup_snapshot.chmod(0o500)
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
    config = [str(setup_snapshot), "--catalog-file", str(catalog), "--state", str(base / "cache"),
              "--byte-ceiling", str(total + len(body)), "--disk-ceiling", str(2 * (total + len(body))),
              "--artifact-capacity", "513", "--package-directory", str(package)]
    reservations = ["--reservation-directory", str(pool), "--copy-ceiling", str(total), "--reservation-ceiling", str(total), "--reservation-capacity", "1"]
    if is_decision:
        policy = ["--decision-profile", args.decision_profile, "--decision-runtime-revision", args.revision,
                  "--decision-engine-sha256", args.engine_pin, "--decision-runtime-sha256", args.runtime_pin,
                  "--decision-model-sha256", args.model_pin, "--decision-tokenizer-sha256", args.tokenizer_pin]
        runtime_error = "unqualified_decision_package_runtime"
    elif is_molmo:
        policy = ["--molmo-engine-sha256", args.engine_pin, "--molmo-kernels-sha256", args.kernels_pin,
                  "--molmo-manifest-sha256", args.manifest_pin]
        runtime_error = "unqualified_molmo_package_runtime"
    elif is_vision:
        policy = ["--vision-engine-sha256", args.engine_pin, "--vision-config-sha256", args.config_pin,
                  "--vision-tokenizer-sha256", args.tokenizer_pin, "--vision-weights-sha256", args.weights_pin, "--vision-kernels-sha256", args.kernels_pin]
        runtime_error = "unqualified_vision_package_runtime"
    elif is_whisper:
        policy = ["--whisper-runtime-revision", args.revision, "--whisper-profile", args.profile,
                  "--whisper-engine-sha256", args.engine_pin, "--whisper-model-sha256", args.model_pin]
        runtime_error = "unqualified_whisper_package_runtime"
    else:
        policy = ["--llama-runtime-revision", args.revision, "--llama-kind", args.kind,
                  "--llama-architecture", args.architecture, "--llama-engine-sha256", args.engine_pin]
        runtime_error = "unqualified_llama_package_runtime"

    def call(op, flags, extra=None, error=None, expected_status="installed_verified"):
        request = dict(contract_version=1, op=op, provider_id="llama-setup-qa")
        if extra:
            request.update(extra)
        result = run_stage(f"{op} expects {error or expected_status}", config + flags,
                           input=json.dumps(request), text=True, capture_output=True, timeout=900)
        assert "AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr, result.stderr
        receipt = json.loads(result.stdout)
        if error:
            assert result.returncode == 1 and receipt["error"]["code"] == error, receipt
        else:
            assert result.returncode == 0 and receipt["status"] == expected_status and receipt["readiness"] == "unqualified", receipt
        return receipt

    for flags, supported in (([], False), (policy, True)):
        discovery = subprocess.run(config + flags,
                                   input=json.dumps(dict(contract_version=1, op="provider_catalog")),
                                   text=True, capture_output=True, timeout=15)
        assert discovery.returncode == 0
        provider = json.loads(discovery.stdout)["providers"][0]
        assert provider["supported"] == supported, provider
        if not supported:
            assert provider["unavailable_reason"] == runtime_error, provider
    if is_whisper or is_vision or is_molmo or is_decision:
        incomplete = subprocess.run(config + policy[:-2], input="{}", text=True, capture_output=True, timeout=15)
        assert incomplete.returncode == 2 and not incomplete.stdout
    if is_decision:
        duplicate = subprocess.run(config + policy + policy[-2:], input="{}", text=True, capture_output=True, timeout=15)
        assert duplicate.returncode == 2 and not duplicate.stdout
    call("provider_install", reservations, error=runtime_error)
    assert not requests and not (package / "assemble.lock").exists() and not (base / "cache").exists()
    call("provider_install", reservations + policy, extra={"decision_engine_sha256": args.engine_pin} if is_decision else ({"molmo_engine_sha256": args.engine_pin} if is_molmo else ({"vision_engine_sha256": args.engine_pin} if is_vision else ({"whisper_model_sha256": args.model_pin} if is_whisper else {"llama_engine_sha256": args.engine_pin}))), error="invalid_request")
    call("provider_install", reservations + policy + ["--digest-endpoint", endpoint], error="artifact_tls_verification_failed")
    assert not requests and not (package / "payload").exists()
    if args.reject_model:
        call("provider_install", reservations + policy + ["--digest-endpoint", endpoint, "--ca-file", str(cert)], error="invalid_model_package")
        assert sorted(requests) == sorted(payloads) and not (package / "complete").exists()
        engine = package / "payload" / next(f["name"] for f in index["files"] if f["role"] == "engine")
        identity = engine.stat()
        call("provider_status", policy, expected_status="incomplete_unverified")
        call("provider_resume", reservations + policy, error="invalid_model_package")
        assert not (package / "complete").exists() and engine.stat().st_ino == identity.st_ino
        assert sorted(requests) == sorted(payloads)
        call("provider_remove", reservations, expected_status="removed")
        call("provider_remove", reservations, expected_status="removed")
        assert not (package / "payload").exists() and sorted(requests) == sorted(payloads)
        print("PASS Molmo setup controlled refusal: policy/TLS checks, hash-consistent invalid native model refused, no completion, offline resume/status and owned incomplete removal")
        raise SystemExit(0)
    call("provider_install", reservations + policy + ["--digest-endpoint", endpoint, "--ca-file", str(cert)])
    assert sorted(requests) == sorted(payloads), requests
    installed_engine = package / "payload" / next(f["name"] for f in index["files"] if f["role"] == "engine")
    identity = installed_engine.stat()
    assert identity.st_mode & 0o777 == 0o500
    wrong = policy.copy()
    if is_decision:
        wrong[wrong.index("--decision-engine-sha256") + 1] = "0" * 64
    elif is_molmo:
        wrong[wrong.index("--molmo-engine-sha256") + 1] = "0" * 64
    elif is_vision:
        wrong[wrong.index("--vision-engine-sha256") + 1] = "0" * 64
    elif is_whisper:
        wrong[wrong.index("--whisper-runtime-revision") + 1] = "0" * 40
    else:
        wrong[wrong.index("--llama-kind") + 1] = "text" if args.kind == "embedding" else "embedding"
    call("provider_resume", reservations + wrong, error=runtime_error)
    assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
    if is_decision:
        for flag, code in (("--decision-runtime-sha256",runtime_error),("--decision-model-sha256","unqualified_decision_package_model"),("--decision-tokenizer-sha256","unqualified_decision_package_model")):
            wrong_model = policy.copy()
            wrong_model[wrong_model.index(flag) + 1] = "0" * 64
            call("provider_resume", reservations + wrong_model, error=code)
            assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
        other_profile = policy.copy()
        other_profile[other_profile.index("--decision-profile") + 1] = "text" if args.decision_profile == "image" else "image"
        call("provider_resume", reservations + other_profile, error="unqualified_decision_package_profile")
        assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
    if is_whisper:
        wrong_model = policy.copy()
        wrong_model[wrong_model.index("--whisper-model-sha256") + 1] = "0" * 64
        call("provider_resume", reservations + wrong_model, error="unqualified_whisper_package_model")
        assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
    if is_vision:
        for flag in ("--vision-config-sha256", "--vision-tokenizer-sha256", "--vision-weights-sha256", "--vision-kernels-sha256"):
            wrong_model = policy.copy()
            wrong_model[wrong_model.index(flag) + 1] = "0" * 64
            call("provider_resume", reservations + wrong_model, error="unqualified_vision_package_model")
            assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
    if is_molmo:
        for flag, code in (("--molmo-kernels-sha256", runtime_error), ("--molmo-manifest-sha256", "unqualified_molmo_package_model")):
            wrong_model = policy.copy()
            wrong_model[wrong_model.index(flag) + 1] = "0" * 64
            call("provider_resume", reservations + wrong_model, error=code)
            assert (package / "complete").is_file() and installed_engine.stat().st_ino == identity.st_ino
    call("provider_status", policy)
    call("provider_resume", reservations + policy)
    assert sorted(requests) == sorted(payloads), requests
    assert installed_engine.stat().st_ino == identity.st_ino
    # Removal is catalog-selected and works after runtime policy is revoked.
    call("provider_remove", reservations, expected_status="removed")
    assert not (package / "payload").exists() and not (package / "complete").exists() and not (package / "removal.record").exists()
    call("provider_remove", reservations, expected_status="removed")
    assert sorted(requests) == sorted(payloads), requests
    call("provider_install", reservations + policy)
    assert (package / "complete").is_file() and sorted(requests) == sorted(payloads)
    # This owned fixture models a crash after all file copies, before completion
    # publication. Removal must not require a completed-package marker.
    (package / "complete").unlink()
    call("provider_remove", reservations, expected_status="removed")
    call("provider_remove", reservations, expected_status="removed")
    call("provider_install", reservations + policy)
    assert (package / "complete").is_file() and sorted(requests) == sorted(payloads)
    if args.summary_cli:
        # Qualify fresh installed inodes, including the complete private library
        # package, rather than the candidate binaries edited by the QA packager.
        model = next(file for file in index["files"] if file["role"] == "model")
        run_stage("actual summary inference", [os.path.abspath(args.summary_test), os.path.abspath(args.summary_cli),
                        str(installed_engine), str(package / "payload" / model["name"])],
                       check=True, timeout=600)
    if args.temporal_cli:
        run_stage("actual temporal inference", [os.path.abspath(args.temporal_test), os.path.abspath(args.temporal_cli),
                        str(installed_engine), str(package / "payload" / "model"), str(base / "actual-temporal")],
                       check=True, timeout=600)
    if args.vision_cli:
        run_stage("actual vision inference", [os.path.abspath(args.vision_test), os.path.abspath(args.vision_cli),
                        str(installed_engine), str(package / "payload" / "model"), os.path.abspath(args.image)],
                       check=True, timeout=600)
    if args.whisper_cli:
        model = next(file for file in index["files"] if file["role"] == "model")
        run_stage("actual whisper inference", [os.path.abspath(args.whisper_test), os.path.abspath(args.whisper_cli),
                        os.path.abspath(args.sources_cli), str(installed_engine), str(package / "payload" / model["name"]),
                        os.path.abspath(args.audio)], check=True, timeout=600)
    print(f"PASS {'OpenDecision' if is_decision else ('Molmo' if is_molmo else ('VisionPsy' if is_vision else ('Whisper' if is_whisper else 'llama')))} setup HTTPS: {len(index['files'])} files; {total} bytes; index {args.pin}; policy/TLS refusal, publication, last-good preservation, offline status/resume, completed/incomplete catalog removal and cache-only reinstall")
finally:
    if server is not None:
        if thread is not None and thread.is_alive():
            server.shutdown()
            thread.join(timeout=5)
        server.server_close()
    shutil.rmtree(base)
