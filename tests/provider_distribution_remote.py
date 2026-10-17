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
import sys
import os
import signal
import time


def exercise_install_recovery(common, online, base, index, provider, call):
    """Observe/kill only this test's live installer, then reuse native resume."""
    model = max((file for file in index['files'] if file['role'] == 'model'), key=lambda file: file['bytes'])
    checkpoint = base / 'cache' / model['sha256'] / 'remote-checkpoint'
    stage = checkpoint.with_name('stage')
    child = subprocess.Popen(common + online, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE, text=True, start_new_session=True)
    child.stdin.write(json.dumps(dict(contract_version=1, op='provider_install', provider_id=provider)))
    child.stdin.close()
    child.stdin = None
    try:
        deadline = time.monotonic() + 300
        retained = 0
        while child.poll() is None and time.monotonic() < deadline:
            try:
                records = checkpoint.read_bytes()
                retained = stage.stat().st_size
            except FileNotFoundError:
                time.sleep(0.01)
                continue
            valid = []
            if len(records) == 288:
                for offset in (0, 144):
                    record = records[offset:offset + 144]
                    if hashlib.sha256(record[:80]).hexdigest().encode() == record[80:]:
                        length = int(record[:16], 16)
                        if 1048576 <= length <= retained < model['bytes']:
                            valid.append((length, record[16:80].decode()))
            if valid:
                child.send_signal(signal.SIGSTOP)
                pid, status = os.waitpid(child.pid, os.WUNTRACED)
                assert pid == child.pid and os.WIFSTOPPED(status)
                break
            time.sleep(0.01)
        else:
            raise AssertionError('No live partial production checkpoint observed before installer exit/deadline')
        prefix, expected_prefix = max(valid)
        identity = stage.stat()
        assert prefix <= identity.st_size < model['bytes'] and identity.st_nlink == 1
        checksum = hashlib.sha256()
        with stage.open('rb') as source:
            remaining = prefix
            while remaining:
                chunk = source.read(min(65536, remaining))
                assert chunk
                checksum.update(chunk)
                remaining -= len(chunk)
        assert checksum.hexdigest() == expected_prefix
        busy = call('provider_install', online, errors={'artifact_busy', 'package_assembly_busy'})
        child.kill()
        assert child.wait(timeout=15) == -signal.SIGKILL
        child.communicate(timeout=15)
        assert (stage.stat().st_dev, stage.stat().st_ino) == (identity.st_dev, identity.st_ino)
        with os.fdopen(os.open(stage, os.O_RDWR | os.O_NOFOLLOW), 'r+b') as source:
            held = os.fstat(source.fileno())
            assert (held.st_dev, held.st_ino) == (identity.st_dev, identity.st_ino)
            original = source.read(1)
            assert original
            altered = bytes([original[0] ^ 1])
            source.seek(0)
            source.write(altered)
            source.flush()
            os.fsync(source.fileno())
        corrupt = call('provider_resume', online, errors={'artifact_checkpoint_mismatch'})
        assert (stage.stat().st_dev, stage.stat().st_ino, stage.stat().st_size) == (identity.st_dev, identity.st_ino, identity.st_size)
        with os.fdopen(os.open(stage, os.O_RDWR | os.O_NOFOLLOW), 'r+b') as source:
            held = os.fstat(source.fileno())
            assert (held.st_dev, held.st_ino) == (identity.st_dev, identity.st_ino)
            assert source.read(1) == altered
            source.seek(0)
            source.write(original)
            source.flush()
            os.fsync(source.fileno())
        call('provider_resume', online)
        return dict(killed_owned_installer=True, verified_checkpoint_bytes=prefix,
                    retained_bytes=identity.st_size, concurrent_refusal=busy['error']['code'],
                    corrupt_prefix_refusal=corrupt['error']['code'], resumed=True)
    finally:
        if child.poll() is None:
            child.kill()
        child.communicate(timeout=15)


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
    parser.add_argument('--profile', choices=('whisper', 't5', 'embedding', 'decision-text', 'decision-image', 'speech-pocket', 'speech-kokoro', 'vision', 'molmo', 'text'), default='whisper')
    parser.add_argument('--speech-quality-profile', type=Path)
    parser.add_argument('--sources', type=Path)
    parser.add_argument('--audio', type=Path)
    parser.add_argument('--image', type=Path)
    parser.add_argument('--managed', type=Path)
    parser.add_argument('--offline', type=Path)
    parser.add_argument('--decision-reference', type=Path)
    parser.add_argument('--fetch-decision-reference', action='store_true', help='Fetch pinned QA reference/frames from the same release; never model installation')
    parser.add_argument('--exercise-resume', action='store_true', help='Kill this owned installer at a production checkpoint and test contention/corrupt-prefix recovery')
    args = parser.parse_args()
    if args.report.exists():
        parser.error('report must not already exist')
    if args.profile == 'whisper' and (not args.sources or not args.audio):
        parser.error('Whisper requires Sources and the pinned audio fixture')
    if args.profile != 'whisper' and (args.sources or args.audio):
        parser.error('only Whisper uses Sources/audio fixtures')
    if args.profile in ('embedding', 'text') and (not args.managed or not args.offline):
        parser.error('text and embedding require managed and offline native adapters')
    if args.profile not in ('embedding', 'text') and (args.managed or args.offline):
        parser.error('managed/offline adapters are text or embedding configuration')
    speech = args.profile.startswith('speech-')
    if bool(args.speech_quality_profile) != (args.profile == 'speech-kokoro'):
        parser.error('Kokoro requires its pinned quality profile; other profiles do not use it')
    molmo = args.profile == 'molmo'
    text = args.profile == 'text'
    vision = args.profile == 'vision'
    if bool(args.image) != vision:
        parser.error('VisionPsy requires its pinned image fixture; other profiles do not use it')
    decision = args.profile.startswith('decision-')
    if (decision and bool(args.decision_reference) == args.fetch_decision_reference) or (
            not decision and (args.decision_reference or args.fetch_decision_reference)):
        parser.error('decision profiles require exactly one pinned reference report')
    candidate = args.candidate.resolve()
    index = json.loads((candidate / 'index.json').read_text())
    catalog = json.loads((candidate / 'catalog.json').read_text())
    policy = json.loads((candidate / 'host-policy.json').read_text())
    if molmo:
        assert index['recipe'] == 'molmo2-mlx-package-v1'
    if text:
        assert index['recipe'] == 'llama-gguf-package-v1' and policy['kind'] == 'text'
    if vision:
        assert index['recipe'] == 'visionpsy-package-v1'
    if speech:
        assert args.profile == 'speech-' + policy['kind'] and index['recipe'] == 'sherpa-speech-package-v1'
    if decision:
        assert args.profile == 'decision-' + policy['profile'] and index['recipe'] == 'opendecision-package-v1'
    entry, = catalog['entries']
    assert digest(candidate / 'index.json') == entry['sha256']
    binaries = ('setup', 'public', 'harness', 'sources') if args.profile == 'whisper' else ('setup', 'public', 'harness')
    if args.profile in ('embedding', 'text'):
        binaries += ('managed', 'offline')
    if args.image:
        assert digest(args.image) == '1c4497cbf9e9b55b61d2f2103814adfb37d64d16083945f4b7f880f46e762c43'
    if args.audio:
        assert digest(args.audio) == '927b006bef03f0c5b2430f5ddfa20629ea8fcabf1f0b422403f5143159e8eeb5'
    result = dict(format='samosa.inference.' + args.profile + '-remote-qualification.v1', index_sha256=entry['sha256'],
                  catalog_sha256=digest(candidate / 'catalog.json'),
                  sources_sha256=digest(candidate / 'sources.json'),
                  executables={key: digest(getattr(args, key)) for key in binaries},
                  status='not_executed')
    if args.speech_quality_profile:
        quality = json.loads(args.speech_quality_profile.read_text())
        assert quality['manifest_sha256'] == policy['manifest_sha256']
        assert quality['kind'].lower() == policy['kind']
        result['quality_profile_sha256'] = digest(args.speech_quality_profile)
    if args.image:
        result['input_sha256'] = digest(args.image)
    if args.audio:
        result['input_sha256'] = digest(args.audio)
    if decision:
        if args.decision_reference:
            assert digest(args.decision_reference) == policy['reference_sha256']
        result['reference_sha256'] = policy['reference_sha256']
    with tempfile.TemporaryDirectory(prefix='samosa-' + args.profile + '-remote-') as temporary:
        # The existing speech verifier requires a canonical owned root. macOS
        # exposes TemporaryDirectory through /var -> /private/var. Normalize
        # only this newly created QA directory; retain all native path guards.
        base = Path(temporary).resolve()
        image = None
        if args.image:
            image = base / 'input.ppm'
            shutil.copyfile(args.image, image)
            image.chmod(0o400)
            assert digest(image) == result['input_sha256']
        speech_quality_profile = None
        if args.speech_quality_profile:
            speech_quality_profile = base / 'speech-quality-profile.json'
            shutil.copyfile(args.speech_quality_profile, speech_quality_profile)
            speech_quality_profile.chmod(0o400)
            assert digest(speech_quality_profile) == result['quality_profile_sha256']
        decision_reference = args.decision_reference
        if decision and args.fetch_decision_reference:
            # Only small QA fixtures use curl. The package, engine, library,
            # tokenizer and model still go through native setup below.
            artifacts = json.loads((candidate / 'qa-artifacts.json').read_text())
            assert artifacts['reference']['sha256'] == policy['reference_sha256']
            reference_root = base / 'reference'
            reference_root.mkdir(mode=0o700)
            for artifact in [artifacts['reference'], *artifacts['files']]:
                name = artifact['name']
                assert Path(name).name == name and not name.startswith('.') and 0 < artifact['bytes'] <= 1048576
                destination = reference_root / name
                with destination.open('xb'):
                    pass
                subprocess.run(['curl', '--fail', '--silent', '--show-error', '--location', '--proto', '=https',
                                '--proto-redir', '=https', '--max-time', '60', '--max-filesize', str(artifact['bytes']),
                                '--output', str(destination), entry['url'].rsplit('/', 1)[0] + '/' + name], check=True)
                assert destination.stat().st_size == artifact['bytes'] and digest(destination) == artifact['sha256']
            decision_reference = reference_root / artifacts['reference']['name']
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
        elif molmo:
            common += ['--molmo-engine-sha256', policy['engine_sha256'], '--molmo-kernels-sha256', policy['kernels_sha256'],
                       '--molmo-manifest-sha256', policy['manifest_sha256']]
        elif vision:
            common += ['--vision-engine-sha256', policy['engine_sha256'], '--vision-config-sha256', policy['config_sha256'],
                       '--vision-tokenizer-sha256', policy['tokenizer_sha256'], '--vision-weights-sha256', policy['weights_sha256'],
                       '--vision-kernels-sha256', policy['kernels_sha256']]
        elif decision:
            common += ['--decision-profile', policy['profile'], '--decision-runtime-revision', policy['runtime_revision'],
                       '--decision-engine-sha256', policy['engine_sha256'], '--decision-runtime-sha256', policy['runtime_sha256'],
                       '--decision-model-sha256', policy['model_sha256'], '--decision-tokenizer-sha256', policy['tokenizer_sha256']]
        elif not speech:
            common += ['--llama-runtime-revision', policy['runtime_revision'], '--llama-kind', policy['kind'],
                       '--llama-architecture', policy['architecture'], '--llama-engine-sha256', policy['engine_sha256']]

        def call(op, flags=(), expected='installed_verified', errors=None):
            print('BEGIN remote ' + op, flush=True)
            completed = subprocess.run(common + list(flags), input=json.dumps(dict(contract_version=1, op=op, provider_id=entry['id'])),
                                       text=True, capture_output=True, timeout=900)
            assert 'AddressSanitizer' not in completed.stderr and 'runtime error:' not in completed.stderr, completed.stderr
            receipt = json.loads(completed.stdout)
            if errors:
                assert completed.returncode == 1 and receipt['error']['code'] in errors, receipt
            else:
                assert completed.returncode == 0 and receipt['status'] == expected and receipt['readiness'] == 'unqualified', receipt
            print('PASS remote ' + op, flush=True)
            return receipt

        online = ['--file-sources', str(base / 'sources.json')]
        for host in policy['redirect_hosts']:
            online += ['--redirect-host', host]
        if args.exercise_resume:
            result['recovery'] = exercise_install_recovery(common, online, base, index, entry['id'], call)
        else:
            call('provider_install', online)
        call('provider_status')
        payload = base / 'package' / 'payload'
        for file in index['files']:
            path = payload / file['name']
            assert path.stat().st_nlink == 1 and path.stat().st_size == file['bytes'] and digest(path) == file['sha256']
        engine = None if speech else next(file for file in index['files'] if file['role'] == 'engine')
        model = next(file for file in index['files'] if file['role'] == 'model')
        environment = os.environ.copy()
        if text:
            environment['SAMOSA_TEST_REGISTRY_BINARY'] = str(snapshots['public'])
            environment['SAMOSA_TEST_OFFLINE_BINARY'] = str(snapshots['offline'])
            command = ['sh', str(snapshots['harness']), str(snapshots['managed']), str(payload / engine['name']),
                       engine['sha256'], str(engine['bytes']), str(payload / model['name']), model['sha256'], str(model['bytes'])]
        elif molmo:
            environment['SAMOSA_TEST_USE_REGISTRY'] = '1'
            command = [str(snapshots['harness']), str(snapshots['public']), str(payload / engine['name']),
                       str(payload / 'model'), str(base / 'temporal-quality')]
        elif speech:
            assert digest(payload / 'manifest.json') == policy['manifest_sha256']
            command = [sys.executable, str(snapshots['harness']), str(snapshots['public']), str(payload), policy['manifest_sha256']]
            if args.speech_quality_profile:
                command.append(str(speech_quality_profile))
            command.append('--stream')
        elif vision:
            command = [str(snapshots['harness']), str(snapshots['public']), str(payload / engine['name']),
                       str(payload / 'model'), str(image)]
        elif decision:
            names = ['decision-engine', 'libonnxruntime.1.27.0.dylib', 'model.onnx', 'tokenizer.json']
            entries = {file['name']: file for file in index['files']}
            inventory = dict(root=str(payload / 'assets'), files=[dict(name=name,
                             bytes=entries['assets/' + name]['bytes'], sha256=entries['assets/' + name]['sha256']) for name in names])
            inventory_path = base / 'decision-inventory.json'
            inventory_path.write_text(json.dumps(inventory, sort_keys=True) + '\n')
            command = [sys.executable, str(snapshots['harness']), str(snapshots['public']), str(inventory_path),
                       str(decision_reference.resolve()), '--inventory-pin', digest(inventory_path),
                       '--reference-pin', policy['reference_sha256']]
            if policy['profile'] == 'image':
                command += ['--image']
        elif args.profile == 'embedding':
            command = [sys.executable, str(snapshots['harness']), '--registry-binary', str(snapshots['public']),
                       '--binary', str(snapshots['managed']), '--offline-binary', str(snapshots['offline']),
                       '--engine', str(payload / engine['name']), '--model', str(payload / model['name']),
                       '--model-sha256', model['sha256'], '--runtime-revision', policy['runtime_revision'],
                       '--output', str(base / 'embedding-quality')]
        else:
            command = [str(snapshots['harness']), str(snapshots['public'])]
            if args.profile == 'whisper':
                command += [str(snapshots['sources'])]
            command += [str(payload / engine['name']), str(payload / model['name'])]
            if args.audio:
                command += [str(args.audio.resolve())]
        subprocess.run(command, env=environment, check=True, timeout=1200)
        if args.profile == 'embedding':
            assert json.loads((base / 'embedding-quality' / 'embedding.json').read_text()) == json.loads(
                (payload / 'embedding-recipe.json').read_text())
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
