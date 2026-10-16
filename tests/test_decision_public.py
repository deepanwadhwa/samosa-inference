#!/usr/bin/env python3
"""QA: exercise configured native text decisions through the public registry.

Actual-model mode requires an independently pinned flat asset inventory and
reference report. No model download or Python inference runtime is used here.
"""
import argparse
import base64
import copy
import hashlib
import json
import math
from pathlib import Path
import subprocess
import tempfile


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(65536), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('inventory', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('--inventory-pin', required=True)
    parser.add_argument('--reference-pin', required=True)
    parser.add_argument('--image', action='store_true')
    parser.add_argument('--case', action='append', help='Run named cases from the pinned report; default is all cases')
    args = parser.parse_args()
    kind = 'image_decisions' if args.image else 'text_decisions'
    operation = 'decide_image' if args.image else 'decide_text'
    assert __debug__
    assert digest(args.inventory) == args.inventory_pin
    assert digest(args.reference) == args.reference_pin
    inventory = json.loads(args.inventory.read_bytes())
    root = Path(inventory['root'])
    pins = [f['sha256'] for f in inventory['files']]
    def verify():
        for f in inventory['files']:
            assert digest(root / f['name']) == f['sha256']
    verify()
    reference = json.loads(args.reference.read_bytes())
    cases = reference['cases']
    if args.case:
        selected = set(args.case)
        if len(selected) != len(args.case) or not selected.issubset({case['name'] for case in cases}):
            raise ValueError('Duplicate or unknown selected reference case')
        cases = [case for case in cases if case['name'] in selected]
    maximum = 0.0
    def compare(actual, expected):
        nonlocal maximum
        if isinstance(expected, (int, float)) and not isinstance(expected, bool):
            assert isinstance(actual, (int, float)) and not isinstance(actual, bool)
            assert math.isfinite(actual)
            difference = abs(actual-expected)
            maximum = max(maximum, difference)
            assert difference <= 0.005, (actual, expected)
        elif isinstance(expected, dict):
            assert isinstance(actual, dict) and list(actual) == list(expected)
            for k in expected:
                compare(actual[k], expected[k])
        elif isinstance(expected, list):
            assert isinstance(actual, list) and len(actual) == len(expected)
            for a, e in zip(actual, expected):
                compare(a, e)
        else:
            assert actual == expected
    with tempfile.TemporaryDirectory(prefix='samosa-decision-public-') as folder:
        folder = Path(folder)
        config = folder / 'host.json'
        provider = dict(provider_id='decision', kind=kind, state=str(folder/'state'),
                        assets=str(root), engine_sha256=pins[0], runtime_sha256=pins[1],
                        model_sha256=pins[2], tokenizer_sha256=pins[3])
        config.write_text(json.dumps(dict(contract_version=1, owner='qa', providers=[provider,
            dict(provider_id='fake', kind='fake_text', state=str(folder/'fake'))])))
        config.chmod(0o600)
        def run(request, error=None):
            p = subprocess.run([str(args.executable.resolve()), '--config', str(config)],
                               input=json.dumps(request, ensure_ascii=False).encode(),
                               capture_output=True, timeout=150)
            assert len(p.stdout) <= 1048576 and len(p.stderr) <= 262144
            response = json.loads(p.stdout)
            if error:
                assert p.returncode != 0 and response['error']['code'] == error, response
            else:
                assert p.returncode == 0, response
            return response
        descriptor = run(dict(contract_version=1, op='describe'))
        assert descriptor['providers'][0]['kind'] == kind
        assert operation in descriptor['providers'][0]['operations']
        assert 'assets' not in json.dumps(descriptor)
        d = run(dict(contract_version=1, op='provider_describe', provider_id='decision'))
        assert d['context_tokens'] == (64 if args.image else 512) and kind in d['capabilities']
        for i, case in enumerate(cases):
            print('BEGIN public ' + case['name'], flush=True)
            request = dict(contract_version=1, op=operation, provider_id='decision',
                           operation_id=f'case-{i}', timeout_ms=120000, request=dict(method=case['method'], **case['arguments']))
            if args.image:
                state = case['state']
                image = args.reference.parent / state['file']
                assert image.parent == args.reference.parent and digest(image) == state['sha256']
                rgb = image.read_bytes()
                assert len(rgb) == state['width'] * state['height'] * 3
                rgba = b''.join(rgb[index:index+3] + bytes([(index//3) % 256]) for index in range(0, len(rgb), 3))
                request.update(width=state['width'], height=state['height'], image_rgba_base64=base64.b64encode(rgba).decode())
                request['request']['state'] = 'prepared-frame'
            response = run(request)
            compare(response['result'], case['result'])
            assert response['operation_id'] == f'case-{i}'
            assert response['provenance']['asset_sha256'] == pins
            assert run(request) == response
            status = run(dict(contract_version=1, op='operation_get', provider_id='decision', operation_id=f'case-{i}'))
            assert status['operation_id'] == f'case-{i}'
            changed = copy.deepcopy(request)
            changed['timeout_ms'] = 119999
            run(changed, 'idempotency_conflict')
            if args.image:
                assert response['image_sha256'] == hashlib.sha256(rgba).hexdigest()
                assert response['width'] == state['width'] and response['height'] == state['height']
                changed = copy.deepcopy(request)
                changed['image_rgba_base64'] = base64.b64encode(bytes([rgba[0] ^ 1]) + rgba[1:]).decode()
                run(changed, 'idempotency_conflict')
        bad = dict(contract_version=1, op=operation, provider_id='decision', request=dict(method='choice', state=0, instructions='q', criteria={}))
        run(bad, 'invalid_request')
        bad['request'] = dict(method='relations', items=[])
        bad['private_path'] = '/tmp/no'
        run(bad, 'invalid_request')
        run(dict(contract_version=1, op='generate', provider_id='decision', prompt='x', max_output_tokens=1), 'unsupported_operation')
        run(dict(contract_version=1, op=operation, provider_id='fake', request=dict(method='relations', items=[])), 'unsupported_operation')
    verify()
    print(f'PASS public native decision: {len(cases)} actual-model cases/replay/routing/refusal; max result error {maximum:.9g}')


if __name__ == '__main__':
    main()
