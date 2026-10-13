#!/usr/bin/env python3
"""Public native speech, owner controls and retained audio across CLI restarts."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

cli, fixture_tool, library = map(os.path.abspath, sys.argv[1:])
package = json.loads(subprocess.check_output([fixture_tool, "--export-package", library]))
root = Path(package["base"])
checks = 0


def check(value, detail=None):
    global checks
    checks += 1
    assert value, (checks, detail)


def config(owner="alice"):
    path = root / (owner + ".json")
    path.write_text(json.dumps({"contract_version": 1, "owner": owner, "providers": [
        {"provider_id": identity, "kind": "speech_output", "state": str(root / identity),
         "package": package["package"], "manifest_sha256": package["manifest_sha256"]}
        for identity in ("voice", "second")]}))
    path.chmod(0o600)
    return path


cfg = config()


def request(op, owner="alice", provider="voice", **fields):
    body = {"contract_version": 1, "op": op, **fields}
    if provider is not None:
        body["provider_id"] = provider
    result = subprocess.run([cli, "--config", str(cfg if owner == "alice" else config(owner))],
                            input=json.dumps(body), text=True, capture_output=True, timeout=15)
    parsed = json.loads(result.stdout)
    check(result.returncode == (0 if "error" not in parsed else 1))
    check("AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr)
    return parsed


def error(code, result):
    check(result.get("error", {}).get("code") == code, (code, result))


try:
    discovery = request("describe", provider=None)
    check(len(discovery["providers"]) == 2)
    check(all(p["kind"] == "speech_output" for p in discovery["providers"]))
    check(not (root / "voice").exists())
    check("speech_output" in request("provider_describe")["capabilities"])
    result = request("speech_generate", operation_id="one", text="hello", voice="caro")
    check(result["operation_id"] == "one" and result["sample_rate"] == 24000)
    check(result["samples"] == 4 and result["bytes"] == 8)
    audio = request("audio_read", audio_id=result["audio_id"], offset=0, length=8)
    pcm = base64.b64decode(audio["pcm_base64"], validate=True)
    check(pcm == bytes((0, 0, 102, 38, 154, 217, 0, 0)))
    check(hashlib.sha256(pcm).hexdigest() == result["pcm_sha256"])
    part = request("audio_read", audio_id=result["audio_id"], offset=2, length=4)
    check(base64.b64decode(part["pcm_base64"]) == pcm[2:6])
    check(request("operation_get", operation_id="one")["status"] == "completed")
    error("unknown_audio", request("audio_read", owner="bob", audio_id=result["audio_id"], offset=0, length=8))
    error("unknown_audio", request("audio_read", provider="second", audio_id=result["audio_id"], offset=0, length=8))
    error("unknown_audio", request("audio_delete", owner="bob", audio_id=result["audio_id"]))
    error("unknown_operation", request("operation_cancel", provider="second", operation_id="one"))
    error("idempotency_conflict", request("speech_generate", operation_id="one", text="different", voice="caro"))
    error("invalid_request", request("speech_generate", operation_id="unknown", text="hello", path="/tmp/untrusted"))
    error("invalid_request", request("speech_generate", text="hello"))
    error("invalid_audio_range", request("audio_read", audio_id=result["audio_id"], offset=1, length=2))
    error("invalid_request", request("audio_read", audio_id=result["audio_id"], offset=0, length=32770))
    failed = request("speech_generate", operation_id="bad-voice", text="hello", voice="unknown")
    error("engine_failed", failed)
    check(request("operation_get", operation_id="bad-voice")["status"] == "failed")
    error("deadline_exceeded", request("speech_generate", operation_id="deadline", text="partial-wait", timeout_ms=200))
    body = {"contract_version": 1, "op": "speech_generate", "provider_id": "voice", "operation_id": "cancel", "text": "partial-wait"}
    pending = subprocess.Popen([cli, "--config", str(cfg)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    pending.stdin.write(json.dumps(body))
    pending.stdin.close()
    until = time.monotonic() + 5
    while time.monotonic() < until:
        status = request("operation_get", operation_id="cancel")
        if status.get("status") == "running":
            break
        time.sleep(0.01)
    else:
        raise AssertionError("operation did not run")
    check(request("operation_cancel", operation_id="cancel")["cancel_requested"])
    pending.wait(timeout=5)
    error("cancelled", json.loads(pending.stdout.read()))
    check(pending.returncode == 1)
    check(request("operation_get", operation_id="cancel")["status"] == "cancelled")
    fresh = request("speech_generate", operation_id="fresh", text="final-only", voice="stuart")
    check(fresh["bytes"] == 8)
    # Saved operation and stored PCM survive a separate CLI process with no model.
    old = package["package"]
    os.rename(old, old + ".offline")
    try:
        check(request("speech_generate", operation_id="one", text="hello", voice="caro") == result)
        check(request("audio_read", audio_id=result["audio_id"], offset=0, length=8) == audio)
    finally:
        os.rename(old + ".offline", old)
    check(request("audio_delete", audio_id=result["audio_id"])["deleted"])
    error("unknown_audio", request("audio_read", audio_id=result["audio_id"], offset=0, length=8))
    check(request("speech_generate", operation_id="one", text="hello", voice="caro") == result)
    error("unknown_audio", request("audio_delete", audio_id=result["audio_id"]))
    print(f"PASS public speech registry: {checks} checks; native PCM, owner/range controls, replay, cancellation and recovery")
finally:
    shutil.rmtree(root)
