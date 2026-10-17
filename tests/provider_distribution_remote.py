#!/usr/bin/env python3
"""Qualify published CPU provider catalogs through existing setup/inference.

QA only. Cold cache and installed weights exist only inside an exclusive temporary
workspace; publication URLs and all expected bytes come from the candidate.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def digest(path):
    checksum = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(65536), b''):
            checksum.update(chunk)
    return checksum.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('candidate', 'setup', 'public', 'harness', 'report'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--profile', choices=('whisper', 't5'), default='whisper')
    parser.add_argument('--sources', type=Path)
    parser.add_argument('--audio', type=Path)
    args = parser.parse_args()
    if args.report.exists():
        parser.error('report must not already exist')
    if args.profile == 'whisper' and (not args.sources or not args.audio):
        parser.error('Whisper requires Sources and the pinned audio fixture')
    if args.profile == 't5' and (args.sources or args.audio):
        parser.error('T5 uses its native summary fixtures')
    candidate = args.candidate.resolve()
    index = json.loads((candidate / 'index.json').read_text())
    catalog = json.loads((candidate / 'catalog.json').read_text())
    policy = json.loads((candidate / 'host-policy.json').read_text())
    entry, = catalog['entries']
    assert digest(candidate / 'index.json') == entry['sha256']
    binaries = ('setup', 'public', 'harness', 'sources') if args.profile == 'whisper' else ('setup', 'public', 'harness')
    if args.audio:
        assert digest(args.audio) == '927b006bef03f0c5b2430f5ddfa20629ea8fcabf1f0b422403f5143159e8eeb5'
    result = dict(format='samosa.inference.' + args.profile + '-remote-qualification.v1', index_sha256=entry['sha256'],
                  catalog_sha256=digest(candidate / 'catalog.json'),
                  sources_sha256=digest(candidate / 'sources.json'),
                  executables={key: digest(getattr(args, key)) for key in binaries},
                  status='not_executed')
    if args.audio:
        result['input_sha256'] = digest(args.audio)
    with tempfile.TemporaryDirectory(prefix='samosa-' + args.profile + '-remote-') as temporary:
        base = Path(temporary)
        snapshots = {}
        for key in binaries:
            snapshots[key] = base / key
            shutil.copyfile(getattr(args, key), snapshots[key])
            snapshots[key].chmod(0o500)
            assert digest(snapshots[key]) == result['executables'][key]
        for name in ('catalog.json', 'sources.json'):
            shutil.copyfile(candidate / name, base / name)
            (base / name).chmod(0o600)
        for name in ('package', 'pool'):
            (base / name).mkdir(mode=0o700)
        total = sum(file['bytes'] for file in index['files']) + entry['size']
        common = [str(snapshots['setup']), '--catalog-file', str(base / 'catalog.json'), '--state', str(base / 'cache'),
                  '--byte-ceiling', str(total), '--disk-ceiling', str(2 * total), '--artifact-capacity', '513',
                  '--package-directory', str(base / 'package'), '--reservation-directory', str(base / 'pool'),
                  '--copy-ceiling', str(total), '--reservation-ceiling', str(total), '--reservation-capacity', '1']
        if args.profile == 'whisper':
            common += ['--whisper-runtime-revision', policy['runtime_revision'], '--whisper-profile', policy['profile'],
                       '--whisper-engine-sha256', policy['engine_sha256'], '--whisper-model-sha256', policy['model_sha256']]
        else:
            common += ['--llama-runtime-revision', policy['runtime_revision'], '--llama-kind', policy['kind'],
                       '--llama-architecture', policy['architecture'], '--llama-engine-sha256', policy['engine_sha256']]

        def call(op, flags=(), expected='installed_verified'):
            print('BEGIN remote ' + op, flush=True)
            completed = subprocess.run(common + list(flags), input=json.dumps(dict(contract_version=1, op=op, provider_id=entry['id'])),
                                       text=True, capture_output=True, timeout=900)
            assert 'AddressSanitizer' not in completed.stderr and 'runtime error:' not in completed.stderr, completed.stderr
            receipt = json.loads(completed.stdout)
            assert completed.returncode == 0 and receipt['status'] == expected and receipt['readiness'] == 'unqualified', receipt
            print('PASS remote ' + op, flush=True)

        online = ['--file-sources', str(base / 'sources.json')]
        for host in policy['redirect_hosts']:
            online += ['--redirect-host', host]
        call('provider_install', online)
        call('provider_status')
        payload = base / 'package' / 'payload'
        for file in index['files']:
            path = payload / file['name']
            assert path.stat().st_nlink == 1 and path.stat().st_size == file['bytes'] and digest(path) == file['sha256']
        engine = next(file for file in index['files'] if file['role'] == 'engine')
        model = next(file for file in index['files'] if file['role'] == 'model')
        command = [str(snapshots['harness']), str(snapshots['public'])]
        if args.profile == 'whisper':
            command += [str(snapshots['sources'])]
        command += [str(payload / engine['name']), str(payload / model['name'])]
        if args.audio:
            command += [str(args.audio.resolve())]
        subprocess.run(command, check=True, timeout=600)
        # Remove source configuration: subsequent operations must use the verified
        # native cache and cannot acquire a missing payload from a remote source.
        (base / 'sources.json').unlink()
        call('provider_resume')
        call('provider_remove', expected='removed')
        call('provider_install')
        call('provider_remove', expected='removed')
        result['status'] = 'passed'
    # Record successful cleanup as well as successful execution.
    result['temporary_installation_removed'] = not base.exists()
    with args.report.open('x') as report:
        json.dump(result, report, indent=2, sort_keys=True)
        report.write('\n')
    print('PASS published ' + args.profile + ' catalog: cold remote install, actual inference, cache-only reuse and cleanup', flush=True)


if __name__ == '__main__':
    main()
