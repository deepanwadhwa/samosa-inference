#!/usr/bin/env python3
"""Create an owned Whisper base.en qualification package from pinned assets.

Build/QA utility only. Native setup independently checks package contents and
host engine/model policy; this script does not grant installation/readiness.
"""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile

REVISION = "f049fff95a089aa9969deb009cdd4892b3e74916"


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(65536), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, sort_keys=True, separators=(",", ":")))
    path.chmod(0o400)


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("engine", type=Path)
parser.add_argument("model", type=Path)
parser.add_argument("--engine-pin", required=True)
parser.add_argument("--model-pin", required=True)
args = parser.parse_args()
assert platform.system() == "Darwin" and platform.machine() == "arm64"
assert re.fullmatch("[0-9a-f]{64}", args.engine_pin) and re.fullmatch("[0-9a-f]{64}", args.model_pin)
sources = {"whisper-cli": args.engine, "ggml-base.en.bin": args.model}
assert all(path.is_file() and not path.is_symlink() for path in sources.values())
pins = {"whisper-cli": args.engine_pin, "ggml-base.en.bin": args.model_pin}
assert pins == {name: digest(path) for name, path in sources.items()}
commands = subprocess.check_output(["otool", "-l", str(args.engine)], text=True)
versions = re.findall(r"^\s*minos (\d+\.\d+(?:\.\d+)?)$", commands, re.MULTILINE)
assert len(versions) == 1
minimum = versions[0]
base = Path(tempfile.mkdtemp(prefix="samosa-whisper-candidate-", dir="/private/tmp")).resolve()
try:
    root = base / "payload"
    root.mkdir(mode=0o700)
    for name, source in sources.items():
        destination = root / name
        shutil.copyfile(source, destination)
        destination.chmod(0o500 if name == "whisper-cli" else 0o400)
        assert digest(destination) == pins[name]
    write_json(root / "whisper-package.json", dict(format="samosa.inference.whisper-package.v1",
               runtime_revision=REVISION, profile="base.en-f16", engine="whisper-cli", model="ggml-base.en.bin"))
    write_json(root / "package-provenance.json", dict(format="samosa.inference.whisper-candidate-provenance.v1",
               runtime_revision=REVISION, source_sha256=pins, minimum_os=minimum,
               qualification="candidate; no distribution or readiness claim"))
    files = []
    for path in sorted(root.iterdir()):
        role = "engine" if path.name == "whisper-cli" else "model" if path.name == "ggml-base.en.bin" else "metadata"
        files.append(dict(name=path.name, role=role, bytes=path.stat().st_size, sha256=digest(path)))
    assert pins == {name: digest(path) for name, path in sources.items()}
    index = base / "index.json"
    write_json(index, dict(format="samosa.inference.package.v1", platform="macos-arm64",
               minimum_os=minimum, recipe="whisper-ggml-package-v1", files=files))
    print(json.dumps(dict(root=str(root), index=str(index), pin=digest(index), engine_pin=args.engine_pin,
                         model_pin=args.model_pin, revision=REVISION, profile="base.en-f16", minimum_os=minimum)))
except BaseException:
    shutil.rmtree(base)
    raise
