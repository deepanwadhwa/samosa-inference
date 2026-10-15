#!/usr/bin/env python3
"""Build an owned CPU-only macOS T5 qualification candidate.

Build/QA utility only; end-user installation is native. The engine pin is a
separate host input, never authority derived from the package descriptor.
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

REVISION = "9fcaed763ccda38ea81068ad9d7f991aaddca451"
MODEL_PIN = "30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c"


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
parser.add_argument("runtime", type=Path, help="Output of tools/build_t5_runtime.sh")
parser.add_argument("model", type=Path)
parser.add_argument("--engine-pin", required=True)
args = parser.parse_args()
assert platform.system() == "Darwin" and platform.machine() == "arm64"
assert re.fullmatch("[0-9a-f]{64}", args.engine_pin)
assert (args.runtime / "prism-revision").read_text().strip() == REVISION
cache = {}
for line in (args.runtime / "build/CMakeCache.txt").read_text().splitlines():
    if not line.startswith(("#", "//")) and ":" in line and "=" in line:
        key, value = line.split("=", 1)
        cache[key.split(":")[0]] = value
profile = {name: cache[name] for name in ("BUILD_SHARED_LIBS", "GGML_BACKEND_DL", "GGML_NATIVE", "GGML_METAL", "GGML_BLAS", "GGML_OPENMP", "LLAMA_OPENSSL")}
assert set(profile.values()) == {"OFF"}
sources = {"summary-engine": args.runtime / "bin/samosa-summarizer", "model.gguf": args.model}
assert all(path.is_file() and not path.is_symlink() for path in sources.values())
source_pins = {name: digest(path) for name, path in sources.items()}
assert source_pins == {"summary-engine": args.engine_pin, "model.gguf": MODEL_PIN}
commands = subprocess.check_output(["otool", "-l", str(sources["summary-engine"])], text=True)
minima = re.findall(r"^\s*minos (\d+\.\d+(?:\.\d+)?)$", commands, re.MULTILINE)
assert minima == ["14.0"] and cache["CMAKE_OSX_DEPLOYMENT_TARGET"] == "14.0"
base = Path(tempfile.mkdtemp(prefix="samosa-t5-package-", dir="/private/tmp")).resolve()
try:
    root = base / "payload"
    root.mkdir(mode=0o700)
    for name, source in sources.items():
        destination = root / name
        shutil.copyfile(source, destination)
        destination.chmod(0o500 if name == "summary-engine" else 0o400)
        assert digest(destination) == source_pins[name]
    write_json(root / "llama-package.json", dict(format="samosa.inference.llama-package.v1",
               kind="summary", runtime_revision=REVISION, engine="summary-engine", model="model.gguf", architecture="t5"))
    write_json(root / "package-provenance.json", dict(format="samosa.inference.t5-candidate-provenance.v1",
               runtime_revision=REVISION, source_sha256=source_pins, build_profile=profile,
               engine_source_sha256=digest(Path(__file__).resolve().parents[1] / "engines/t5_summary.cpp"),
               minimum_os="14.0", qualification="candidate; no distribution or readiness claim"))
    files = []
    for path in sorted(root.iterdir()):
        role = "engine" if path.name == "summary-engine" else "model" if path.name == "model.gguf" else "metadata"
        files.append(dict(name=path.name, role=role, bytes=path.stat().st_size, sha256=digest(path)))
    assert source_pins == {name: digest(path) for name, path in sources.items()}
    index = base / "index.json"
    write_json(index, dict(format="samosa.inference.package.v1", platform="macos-arm64",
               minimum_os="14.0", recipe="llama-gguf-package-v1", files=files))
    print(json.dumps(dict(root=str(root), index=str(index), pin=digest(index), engine_pin=args.engine_pin,
                         revision=REVISION, kind="summary", architecture="t5", minimum_os="14.0")))
except BaseException:
    shutil.rmtree(base)
    raise
