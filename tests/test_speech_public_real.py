#!/usr/bin/env python3
"""Optional actual Pocket generation and ranged audio through public registry."""
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

arguments = sys.argv[1:]
streaming = arguments[-1] == "--stream"
if streaming:
    arguments = arguments[:-1]
cli, package, pin = arguments[:3]
profile = json.loads(Path(arguments[3]).read_text()) if len(arguments) == 4 else None
if profile and profile["manifest_sha256"] != pin:
    raise ValueError("actual-model profile does not match package pin")
kind = profile["kind"] if profile else "Pocket"
root = Path(tempfile.mkdtemp(prefix="samosa-public-pocket-"))
checks = 0
stream_audio = {}


def check(value):
    global checks
    checks += 1
    assert value, checks


def request(op, **fields):
    command = [os.path.abspath(cli), "--config", str(root / "config.json")]
    if streaming and op == "speech_generate":
        command.append("--stream")
    result = subprocess.run(command,
                            input=json.dumps({"contract_version": 1, "provider_id": "pocket", "op": op, **fields}),
                            text=True, capture_output=True, timeout=120)
    check(result.returncode == 0)
    check("AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr)
    if not streaming or op != "speech_generate":
        return json.loads(result.stdout)
    events = [json.loads(line) for line in result.stdout.splitlines()]
    check(events and events[-1]["event"] == "completed")
    pcm = bytearray()
    for sequence, event in enumerate(events[:-1]):
        check(event["event"] == "speech_pcm" and event["provisional"])
        check(event["sequence"] == sequence and event["offset"] == len(pcm))
        check(event["operation_id"] == fields["operation_id"] and event["sample_rate"] == 24000)
        chunk = base64.b64decode(event["pcm_base64"], validate=True)
        check(len(chunk) == event["bytes"] and 0 < len(chunk) <= 8192 and not len(chunk) % 2)
        pcm.extend(chunk)
    terminal = events[-1]
    check(terminal["sequence"] == len(events) - 1)
    receipt = terminal["response"]
    if pcm:
        check(not terminal["replayed"] and len(pcm) == receipt["bytes"])
        check(hashlib.sha256(pcm).hexdigest() == receipt["pcm_sha256"])
        stream_audio[fields["operation_id"]] = pcm
    else:
        check(terminal["replayed"])
    return receipt


try:
    config = root / "config.json"
    config.write_text(json.dumps({"contract_version": 1, "owner": "actual-model-test", "providers": [
        {"provider_id": "pocket", "kind": "speech_output", "state": str(root / "state"),
         "package": package, "manifest_sha256": pin}]}))
    config.chmod(0o600)
    pocket_cases = (
        ("caro", 78720, "5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4"),
        ("stuart", 90240, "0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc"),
    )
    cases = [(v["voice"], v["samples"], v["pcm_sha256"]) for v in profile["voices"]] if profile else pocket_cases
    for voice, samples, digest in cases:
        fields = dict(operation_id=voice, text="The meeting starts at nine. Please bring your notes.", voice=voice)
        receipt = request("speech_generate", **fields)
        check(receipt["provenance"] == dict(validation="verified_before_and_after_generation",
              manifest_sha256=pin, package_identity=hashlib.sha256(("sherpa-speech-package-v1:" + pin).encode()).hexdigest(),
              input_sha256=hashlib.sha256(fields["text"].encode()).hexdigest(), requested_voice=voice))
        check(receipt["sample_rate"] == 24000)
        if profile and profile.get("fresh_generation") == "variable_pcm":
            # A pinned model still varies across fresh generations. Keep a
            # duration regression bound; verify each result against its own
            # receipt, then require exact retained replay. Reference hashes
            # are diagnostic evidence, not a promise of deterministic output.
            tolerance = profile["sample_count_relative_tolerance"]
            check(abs(receipt["samples"] - samples) <= samples * tolerance)
            digest = receipt["pcm_sha256"]
            samples = receipt["samples"]
        else:
            check(receipt["samples"] == samples and receipt["pcm_sha256"] == digest)
        check(receipt["bytes"] == samples * 2)
        pcm = bytearray()
        while len(pcm) < receipt["bytes"]:
            offset = len(pcm)
            length = min(32768, receipt["bytes"] - offset)
            part = request("audio_read", audio_id=receipt["audio_id"], offset=offset, length=length)
            check(part["offset"] == offset and part["length"] == length)
            pcm.extend(base64.b64decode(part["pcm_base64"], validate=True))
        check(hashlib.sha256(pcm).hexdigest() == digest)
        check(any(pcm))
        if streaming:
            check(stream_audio[voice] == pcm)
        check(request("speech_generate", **fields) == receipt)
        check(request("operation_get", operation_id=voice)["status"] == "completed")
        print(f"PASS actual public {kind} {voice}: {samples} samples, ranged PCM hash {digest}", flush=True)
    print(f"PASS actual public speech: {checks} checks; public model execution, retained audio ranges and replay")
finally:
    shutil.rmtree(root)
