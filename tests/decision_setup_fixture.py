#!/usr/bin/env python3
"""Reuse the existing HTTPS setup gate for controlled native decision packages."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

setup, exporter = sys.argv[1:]
for profile in ['text', 'image']:
    package = json.loads(subprocess.check_output([exporter, '--export-' + profile], text=True))
    try:
        subprocess.run([sys.executable, str(Path(__file__).with_name('llama_setup_https.py')), setup,
                        package['root'], package['index'], package['pin'],
                        *(['--per-file-sources'] if profile == 'image' else []),
                        '--credential-isolation', '--decision-profile', profile, '--revision', '327d6911fc7f31e779d2fa673e83ba1f3410b468',
                        '--engine-pin', package['engine_pin'], '--runtime-pin', package['runtime_pin'],
                        '--model-pin', package['model_pin'], '--tokenizer-pin', package['tokenizer_pin']],
                       check=True, timeout=120)
    finally:
        shutil.rmtree(package['root'])
        Path(package['index']).unlink()
