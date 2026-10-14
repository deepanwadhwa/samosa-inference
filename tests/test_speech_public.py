#!/usr/bin/env python3
"""Public native speech, owner controls and retained audio across CLI restarts."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import select
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


def stream_start(operation, text="hello", **fields):
    body = dict(contract_version=1, op="speech_generate", provider_id="voice",
                operation_id=operation, text=text, **fields)
    process = subprocess.Popen([cli, "--config", str(cfg), "--stream"],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    process.stdin.write(json.dumps(body).encode())
    process.stdin.close()
    process.stdin = None
    return process


def stream_event(process):
    ready, _, _ = select.select([process.stdout], [], [], 5)
    check(ready, "stream event timed out")
    return json.loads(process.stdout.readline())


try:
    discovery = request("describe", provider=None)
    check(len(discovery["providers"]) == 2)
    check(all(p["kind"] == "speech_output" for p in discovery["providers"]))
    check(not (root / "voice").exists())
    check("speech_output" in request("provider_describe")["capabilities"])
    check("speech_pcm_stream" in request("provider_describe")["capabilities"])
    result = request("speech_generate", operation_id="one", text="hello", voice="caro")
    check(result["operation_id"] == "one" and result["sample_rate"] == 24000)
    check(result["samples"] == 4 and result["bytes"] == 8)
    audio = request("audio_read", audio_id=result["audio_id"], offset=0, length=8)
    pcm = base64.b64decode(audio["pcm_base64"], validate=True)
    check(pcm == bytes((0, 0, 102, 38, 154, 217, 0, 0)))
    check(hashlib.sha256(pcm).hexdigest() == result["pcm_sha256"])
    streamed = stream_start("stream-one", voice="caro")
    output, diagnostics = streamed.communicate(timeout=15)
    check(streamed.returncode == 0 and not diagnostics)
    events = [json.loads(line) for line in output.splitlines()]
    check(events[-1]["event"] == "completed" and not events[-1]["replayed"])
    chunks = events[:-1]
    check(chunks and all(e["event"] == "speech_pcm" and e["provisional"] for e in chunks))
    assembled = b""
    for i, event in enumerate(chunks):
        check(event["sequence"] == i and event["offset"] == len(assembled))
        check(event["provider_id"] == "voice" and event["operation_id"] == "stream-one")
        part_pcm = base64.b64decode(event["pcm_base64"], validate=True)
        check(len(part_pcm) == event["bytes"] and len(part_pcm) <= 8192)
        assembled += part_pcm
    terminal = events[-1]
    check(terminal["sequence"] == len(chunks) and assembled == pcm)
    check(hashlib.sha256(assembled).hexdigest() == terminal["response"]["pcm_sha256"])
    check(request("speech_generate", operation_id="stream-one", text="hello", voice="caro") == terminal["response"])
    replay = stream_start("stream-one", voice="caro")
    replay_output, diagnostics = replay.communicate(timeout=15)
    replay_events = [json.loads(line) for line in replay_output.splitlines()]
    check(replay.returncode == 0 and not diagnostics and len(replay_events) == 1)
    check(replay_events[0]["event"] == "completed" and replay_events[0]["replayed"])
    check(replay_events[0]["response"] == terminal["response"])
    # The fixture stalls inside a native callback after the first two samples.
    # Receiving PCM here proves incremental delivery before terminal readiness.
    live = stream_start("stream-cancel", "partial-wait")
    first = stream_event(live)
    check(first["event"] == "speech_pcm" and first["sequence"] == 0)
    check(base64.b64decode(first["pcm_base64"]) == pcm[:4] and live.poll() is None)
    check(request("operation_cancel", operation_id="stream-cancel")["cancel_requested"])
    cancelled = stream_event(live)
    check(live.wait(timeout=5) == 1)
    check(cancelled["event"] == "failed" and cancelled["sequence"] == 1)
    error("cancelled", cancelled["response"])
    check(not live.stdout.read())
    disconnected = stream_start("stream-disconnect", "partial-wait")
    check(stream_event(disconnected)["event"] == "speech_pcm")
    disconnected.stdout.close()
    check(disconnected.wait(timeout=5) == 2)
    check(request("operation_get", operation_id="stream-disconnect")["status"] == "cancelled")
    check(not disconnected.stderr.read())
    timed = stream_start("stream-deadline", "partial-wait", timeout_ms=500)
    timed_output, diagnostics = timed.communicate(timeout=8)
    timed_events = [json.loads(line) for line in timed_output.splitlines()]
    check(timed.returncode == 1 and not diagnostics)
    check(timed_events[0]["event"] == "speech_pcm" and timed_events[-1]["event"] == "failed")
    error("deadline_exceeded", timed_events[-1]["response"])
    for malformed, code in (("{", "invalid_json"),
                            ('{"contract_version":1,"op":"describe"}', "unsupported_stream_operation")):
        invalid = subprocess.run([cli, "--config", str(cfg), "--stream"], input=malformed,
                                 text=True, capture_output=True, timeout=5)
        event = json.loads(invalid.stdout)
        check(invalid.returncode == 1 and event["event"] == "failed")
        error(code, event["response"])
    # A consumer that stays connected but never drains PCM must not defeat the
    # operation deadline or retain the guardian/admission lease indefinitely.
    blocked = stream_start("stream-blocked", "stream-long", timeout_ms=500)
    check(blocked.wait(timeout=6) == 2)
    check(request("operation_get", operation_id="stream-blocked")["status"] == "failed")
    error("deadline_exceeded", request("speech_generate", operation_id="stream-blocked",
                                       text="stream-long", timeout_ms=500))
    check(request("speech_generate", operation_id="stream-survivor", text="hello")["samples"] == 4)
    blocked.stdout.close()
    check(not blocked.stderr.read())
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
