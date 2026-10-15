#!/usr/bin/env python3
"""Run public llama setup against an owned native-format controlled fixture."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

setup, exporter = sys.argv[1:]
for kind, architecture, revision, export in [
        ("embedding", "gemma-embedding2", "4fbc76dec51d0add466f0210855c0596589b60d4", "--export-llama"),
        ("summary", "t5", "9fcaed763ccda38ea81068ad9d7f991aaddca451", "--export-summary")]:
    package = json.loads(subprocess.check_output([exporter, export], text=True))
    try:
        subprocess.run([sys.executable, str(Path(__file__).with_name("llama_setup_https.py")), setup,
                        package["root"], package["index"], package["pin"],
                        "--revision", revision, "--kind", kind,
                        "--architecture", architecture, "--engine-pin", package["engine_pin"]], check=True, timeout=120)
    finally:
        shutil.rmtree(package["root"])
        Path(package["index"]).unlink()
