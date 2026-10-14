#!/usr/bin/env python3
"""Run public llama setup against an owned native-format controlled fixture."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

setup, exporter = sys.argv[1:]
package = json.loads(subprocess.check_output([exporter, "--export-llama"], text=True))
try:
    subprocess.run([sys.executable, str(Path(__file__).with_name("llama_setup_https.py")), setup,
                    package["root"], package["index"], package["pin"],
                    "--revision", "4fbc76dec51d0add466f0210855c0596589b60d4", "--kind", "embedding",
                    "--architecture", "gemma-embedding2", "--engine-pin", package["engine_pin"]], check=True, timeout=120)
finally:
    shutil.rmtree(package["root"])
    Path(package["index"]).unlink()
