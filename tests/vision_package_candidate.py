#!/usr/bin/env python3
"""Create an owned VisionPsy qualification package from pinned assets.

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
for name in ("config", "tokenizer", "weights"):
    parser.add_argument(f"--{name}-pin", required=True)
args = parser.parse_args()
assert platform.system() == "Darwin" and platform.machine() == "arm64"
pins = {"samosa-visionpsy": args.engine_pin, "model/config.json": args.config_pin,
        "model/tokenizer.json": args.tokenizer_pin, "model/model.safetensors": args.weights_pin}
assert all(re.fullmatch("[0-9a-f]{64}", pin) for pin in pins.values())
sources = {"samosa-visionpsy": args.engine, "model/config.json": args.model / "config.json",
           "model/tokenizer.json": args.model / "tokenizer.json", "model/model.safetensors": args.model / "model.safetensors"}
assert not args.model.is_symlink()
assert all(path.is_file() and not path.is_symlink() for path in sources.values())
assert pins == {name: digest(path) for name, path in sources.items()}
commands = subprocess.check_output(["otool", "-l", str(args.engine)], text=True)
versions = re.findall(r"^\s*minos (\d+\.\d+(?:\.\d+)?)$", commands, re.MULTILINE)
assert len(versions) == 1
minimum = versions[0]
base = Path(tempfile.mkdtemp(prefix="samosa-vision-candidate-", dir="/private/tmp")).resolve()
try:
    root = base / "payload"
    root.mkdir(mode=0o700)
    (root / "model").mkdir(mode=0o700)
    for name, source in sources.items():
        destination = root / name
        shutil.copyfile(source, destination)
        destination.chmod(0o500 if name == "samosa-visionpsy" else 0o400)
        assert digest(destination) == pins[name]
    write_json(root / "vision-package.json", dict(format="samosa.inference.vision-package.v1",
               profile="visionpsy-v1", engine="samosa-visionpsy", model_directory="model"))
    write_json(root / "package-provenance.json", dict(format="samosa.inference.vision-candidate-provenance.v1",
               source_sha256=pins, minimum_os=minimum, qualification="candidate; no distribution or readiness claim"))
    files = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        name = path.relative_to(root).as_posix()
        role = "engine" if name == "samosa-visionpsy" else "model" if name == "model/model.safetensors" else "data" if name == "model/tokenizer.json" else "metadata"
        files.append(dict(name=name, role=role, bytes=path.stat().st_size, sha256=digest(path)))
    assert pins == {name: digest(path) for name, path in sources.items()}
    index = base / "index.json"
    write_json(index, dict(format="samosa.inference.package.v1", platform="macos-arm64",
               minimum_os=minimum, recipe="visionpsy-package-v1", files=files))
    print(json.dumps(dict(root=str(root), index=str(index), pin=digest(index), engine_pin=args.engine_pin,
                         config_pin=args.config_pin, tokenizer_pin=args.tokenizer_pin,
                         weights_pin=args.weights_pin, minimum_os=minimum)))
except BaseException:
    shutil.rmtree(base)
    raise
