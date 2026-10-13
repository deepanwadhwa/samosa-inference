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

cli, package, pin = sys.argv[1:]
root = Path(tempfile.mkdtemp(prefix="samosa-public-pocket-"))
checks = 0


def check(value):
    global checks
    checks += 1
    assert value, checks


def request(op, **fields):
    result = subprocess.run([os.path.abspath(cli), "--config", str(root / "config.json")],
                            input=json.dumps({"contract_version": 1, "provider_id": "pocket", "op": op, **fields}),
                            text=True, capture_output=True, timeout=120)
    check(result.returncode == 0)
    check("AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr)
    return json.loads(result.stdout)


try:
    config = root / "config.json"
    config.write_text(json.dumps({"contract_version": 1, "owner": "actual-model-test", "providers": [
        {"provider_id": "pocket", "kind": "speech_output", "state": str(root / "state"),
         "package": package, "manifest_sha256": pin}]}))
    config.chmod(0o600)
    for voice, samples, digest in (
        ("caro", 78720, "5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4"),
        ("stuart", 90240, "0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc"),
    ):
        fields = dict(operation_id=voice, text="The meeting starts at nine. Please bring your notes.", voice=voice)
        receipt = request("speech_generate", **fields)
        check(receipt["sample_rate"] == 24000 and receipt["samples"] == samples)
        check(receipt["pcm_sha256"] == digest and receipt["bytes"] == samples * 2)
        pcm = bytearray()
        while len(pcm) < receipt["bytes"]:
            offset = len(pcm)
            length = min(32768, receipt["bytes"] - offset)
            part = request("audio_read", audio_id=receipt["audio_id"], offset=offset, length=length)
            check(part["offset"] == offset and part["length"] == length)
            pcm.extend(base64.b64decode(part["pcm_base64"], validate=True))
        check(hashlib.sha256(pcm).hexdigest() == digest)
        check(request("speech_generate", **fields) == receipt)
        check(request("operation_get", operation_id=voice)["status"] == "completed")
        print(f"PASS actual public Pocket {voice}: {samples} samples, ranged PCM hash {digest}", flush=True)
    print(f"PASS actual public speech: {checks} checks; public model execution, retained audio ranges and replay")
finally:
    shutil.rmtree(root)
