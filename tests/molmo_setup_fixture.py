#!/usr/bin/env python3
"""Exercise the shared public setup gate with a controlled invalid Molmo2 model package."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

setup, exporter = sys.argv[1:]
package = json.loads(subprocess.check_output([exporter, "--export"], text=True))
try:
    subprocess.run([sys.executable, str(Path(__file__).with_name("llama_setup_https.py")), setup,
                    package["root"], package["index"], package["pin"],
                    "--engine-pin", package["engine_pin"], "--kernels-pin", package["kernels_pin"],
                    "--manifest-pin", package["manifest_pin"], "--reject-model"], check=True, timeout=120)
finally:
    shutil.rmtree(package["root"])
    Path(package["index"]).unlink()
