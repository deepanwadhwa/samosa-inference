#!/usr/bin/env python3
"""Create an owned Molmo2 qualification package from pinned assets.

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
parser.add_argument("--kernels", type=Path, required=True)
for name in ("manifest", "kernels"):
    parser.add_argument(f"--{name}-pin", required=True)
args = parser.parse_args()
assert platform.system() == "Darwin" and platform.machine() == "arm64"
assert args.model.is_dir() and not args.model.is_symlink()
manifest = args.model / "manifest.json"
assert manifest.is_file() and not manifest.is_symlink() and digest(manifest) == args.manifest_pin
native = json.loads(manifest.read_bytes())
assert native["format"] == "samosa.molmo2.mlx.v1" and native["package_id"] == "molmo2-4b-mlx-q4-v1"
assert 4 <= len(native["files"]) <= 64
pins = {"samosa-molmo2": args.engine_pin, "mlx.metallib": args.kernels_pin, "model/manifest.json": args.manifest_pin}
sources = {"samosa-molmo2": args.engine, "mlx.metallib": args.kernels, "model/manifest.json": manifest}
roles = {"samosa-molmo2": "engine", "mlx.metallib": "data", "model/manifest.json": "metadata"}
for file in native["files"]:
    assert re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", file["name"])
    name = "model/" + file["name"]
    assert name not in sources and file["role"] in ("weights", "tokenizer", "metadata")
    sources[name] = args.model / file["name"]
    pins[name] = file["sha256"]
    roles[name] = {"weights": "model", "tokenizer": "data", "metadata": "metadata"}[file["role"]]
    assert sources[name].stat().st_size == file["bytes"]
assert all(re.fullmatch("[0-9a-f]{64}", pin) for pin in pins.values())
assert all(path.is_file() and not path.is_symlink() for path in sources.values())
assert pins == {name: digest(path) for name, path in sources.items()}
commands = subprocess.check_output(["otool", "-l", str(args.engine)], text=True)
versions = re.findall(r"^\s*minos (\d+\.\d+(?:\.\d+)?)$", commands, re.MULTILINE)
assert len(versions) == 1
minimum = versions[0]
base = Path(tempfile.mkdtemp(prefix="samosa-molmo-candidate-", dir="/private/tmp")).resolve()
try:
    root = base / "payload"
    root.mkdir(mode=0o700)
    (root / "model").mkdir(mode=0o700)
    for name, source in sources.items():
        destination = root / name
        shutil.copyfile(source, destination)
        destination.chmod(0o500 if name == "samosa-molmo2" else 0o400)
        assert digest(destination) == pins[name]
    write_json(root / "molmo-package.json", dict(format="samosa.inference.molmo-package.v1",
               profile="molmo2-4b-mlx-q4-v1", engine="samosa-molmo2", model_directory="model"))
    write_json(root / "package-provenance.json", dict(format="samosa.inference.molmo-candidate-provenance.v1",
               source_sha256=pins, minimum_os=minimum, qualification="candidate; no distribution or readiness claim"))
    files = []
    for path in sorted(root.rglob("*")):
        if not path.is_file():
            continue
        name = path.relative_to(root).as_posix()
        role = roles.get(name, "metadata")
        files.append(dict(name=name, role=role, bytes=path.stat().st_size, sha256=digest(path)))
    assert pins == {name: digest(path) for name, path in sources.items()}
    index = base / "index.json"
    write_json(index, dict(format="samosa.inference.package.v1", platform="macos-arm64",
               minimum_os=minimum, recipe="molmo2-mlx-package-v1", files=files))
    print(json.dumps(dict(root=str(root), index=str(index), pin=digest(index), engine_pin=args.engine_pin,
                         manifest_pin=args.manifest_pin, kernels_pin=args.kernels_pin, minimum_os=minimum)))
except BaseException:
    shutil.rmtree(base)
    raise
