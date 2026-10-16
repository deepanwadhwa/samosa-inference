#!/usr/bin/env python3
"""QA only: actual image decisions through the existing framed native worker."""
import argparse
import json
import math
from pathlib import Path
import struct
import subprocess
import tempfile
from opendecision_export import digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['executable', 'model', 'tokenizer', 'reference']:
        parser.add_argument(name, type=Path)
    for name in ['model', 'tokenizer', 'reference']:
        parser.add_argument('--' + name + '-pin', required=True)
    args = parser.parse_args()
    for name in ['model', 'tokenizer', 'reference']:
        if digest(getattr(args, name)) != getattr(args, name + '_pin'):
            raise ValueError('Pinned input changed: ' + name)
    reference = json.loads(args.reference.read_bytes())
    groups = {}
    for case in reference['cases']:
        groups.setdefault(case['state']['file'], []).append(case)
    maximum = 0.0
    def compare(actual, expected):
        nonlocal maximum
        if isinstance(expected, dict):
            assert isinstance(actual, dict) and list(actual) == list(expected)
            for key in expected:
                compare(actual[key], expected[key])
        elif isinstance(expected, (int, float)):
            assert isinstance(actual, (int, float)) and math.isfinite(actual)
            difference = abs(actual - expected)
            assert difference <= 0.005, (actual, expected)
            maximum = max(maximum, difference)
        else:
            assert actual == expected
    with tempfile.TemporaryDirectory(prefix='samosa-image-worker-') as folder:
        folder = Path(folder)
        for filename, cases in groups.items():
            state = cases[0]['state']
            assert Path(filename).name == filename and state['format'] == 'rgb8'
            original = args.reference.parent / filename
            assert digest(original) == state['sha256']
            rgb = original.read_bytes()
            assert len(rgb) == state['width'] * state['height'] * 3
            rgba = bytearray()
            for index in range(0, len(rgb), 3):
                rgba.extend(rgb[index:index+3])
                rgba.append((index // 3) % 256)
            pixels = folder / 'input.rgba'
            pixels.write_bytes(rgba)
            pixels.chmod(0o600)
            wire = bytearray()
            for case in cases:
                request = dict(method=case['method'], state='prepared-frame', **case['arguments'])
                encoded = json.dumps(request, ensure_ascii=False).encode()
                wire.extend(struct.pack('>I', len(encoded)) + encoded)
            command = [str(args.executable.resolve()), '--model', str(args.model.resolve()),
                       '--tokenizer', str(args.tokenizer.resolve()), '--image-rgba', str(pixels),
                       '--width', str(state['width']), '--height', str(state['height'])]
            result = subprocess.run(command, input=wire, capture_output=True, timeout=180)
            assert result.returncode == 0, (result.returncode, result.stderr[-4096:])
            offset = 0
            for case in cases:
                assert len(result.stdout) - offset >= 4
                length, = struct.unpack_from('>I', result.stdout, offset)
                offset += 4
                assert 0 < length <= 1048576 and len(result.stdout) - offset >= length
                compare(json.loads(result.stdout[offset:offset+length]), case['result'])
                offset += length
                print('PASS native image worker ' + case['name'], flush=True)
            assert offset == len(result.stdout)
            # Reject invalid dimensions, missing mode and truncated raw pixels
            # before attempting a model load.
            for bad in [command[:-1] + ['513'], command[:-4]]:
                assert subprocess.run(bad, input=b'', capture_output=True, timeout=10).returncode == 2
            pixels.write_bytes(b'bad')
            assert subprocess.run(command, input=b'', capture_output=True, timeout=10).returncode == 3
    for name in ['model', 'tokenizer', 'reference']:
        assert digest(getattr(args, name)) == getattr(args, name + '_pin')
    print(f'PASS image worker: {len(reference["cases"])} actual-model cases, repeated framing, EOF and raw-input refusals; max_result_error={maximum:.9g}')


if __name__ == '__main__':
    main()
