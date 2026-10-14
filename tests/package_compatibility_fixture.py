#!/usr/bin/env python3
"""Qualified native-format fixture package through concrete recipe dispatch."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

binary, exporter, library = map(os.path.abspath, sys.argv[1:])
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
    result = subprocess.run([binary, str(root), str(index), hashlib.sha256(body).hexdigest()],
                            text=True, capture_output=True, timeout=30)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert "AddressSanitizer" not in result.stderr and "runtime error:" not in result.stderr
    print(result.stdout, end="")
finally:
    shutil.rmtree(base)
