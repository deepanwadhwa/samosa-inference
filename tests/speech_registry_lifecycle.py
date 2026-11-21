#!/usr/bin/env python3
"""Qualify existing native speech through public owned lifecycle and receipts."""
import argparse
import base64
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import sqlite3
import subprocess
import tempfile
import time
from summary_registry_lifecycle import Session, private, digest
from speech_quality import qualify_voice


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--fixture-host', type=Path)
    parser.add_argument('--fixture-library', type=Path)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--manifest-pin')
    parser.add_argument('--quality-profile', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    binary_pin = digest(args.binary)
    root = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='samosa-speech-registry-')).resolve()
    if args.output:
        root.mkdir(mode=0o700)
    binary = root / 'providers'
    shutil.copyfile(args.binary, binary)
    binary.chmod(0o500)
    actual = args.package is not None
    if actual:
        package, pin = args.package.resolve(), args.manifest_pin
        assert digest(package / 'manifest.json') == pin
    else:
        assert args.fixture_host and args.fixture_library
        exported = subprocess.check_output([str(args.fixture_host.resolve()), '--export-package', str(args.fixture_library.resolve())], text=True)
        fixture = json.loads(exported)
        package, pin = Path(fixture['package']), fixture['manifest_sha256']
    provider = dict(kind='speech_output', provider_id='native-speech-v1', package=str(package), manifest_sha256=pin,
        state=str(root / 'state'), lifecycle='resident-speech-v1', worker_sha256=binary_pin,
        worker_size=binary.stat().st_size, manifest_size=(package / 'manifest.json').stat().st_size)
    state, config = root / 'state', root / 'registry.json'
    registry = dict(contract_version=1, owner='qa', providers=[provider,
        dict(kind='fake_text', provider_id='peer', state=str(root / 'peer'))])
    private(config, registry)
    marker, loads = root / 'partial', root / 'loads'
    environment = os.environ.copy()
    if not actual:
        environment.update(SAMOSA_TEST_SPEECH_PARTIAL_MARKER=str(marker), SAMOSA_TEST_SPEECH_LOAD_LOG=str(loads))
    profile_pin = digest(args.quality_profile) if args.quality_profile else None
    profile = json.loads(args.quality_profile.read_text()) if args.quality_profile else None
    if profile:
        assert actual and profile['manifest_sha256'] == pin
    cases = profile['voices'] if profile else [
        dict(voice='caro', samples=78720, pcm_sha256='5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4'),
        dict(voice='stuart', samples=90240, pcm_sha256='0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc')]
    first_voice = cases[0]['voice']
    def quality_check(value):
        assert value, result
    records, checks = [], []
    phrase = 'The meeting starts at nine. Please bring your notes.'

    def request(op, **extra):
        return dict(contract_version=1, op=op, **({} if op == 'describe' else dict(provider_id=provider['provider_id'])), **extra)

    def call(op, expected=None, **extra):
        response = session.receive(session.submit(request(op, **extra)))
        records.append(dict(op=op, response=response))
        assert response.get('error', {}).get('code') == expected, response
        return response

    def activation():
        with sqlite3.connect(f'file:{state}/activation/activation.sqlite3?mode=ro', uri=True) as db:
            return db.execute('SELECT revision,config,digest FROM current WHERE id=1').fetchone()

    def released():
        with (state / 'activation/residency.lock').open('rb') as fd:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return True
            except BlockingIOError:
                return False

    def worker_pid():
        lines = subprocess.check_output(['ps', '-axo', 'pid,args'], text=True).splitlines()
        pids = [int(line.split(None, 1)[0]) for line in lines if len(line.split(None, 1)) == 2
            and line.split(None, 1)[1].startswith(str(binary) + ' --inference-speech-resident-worker ')]
        assert len(pids) == 1, pids
        return pids[0]

    with (root / 'stderr.log').open('w') as log:
        session = Session(binary, config, log, environment)
        try:
            assert call('describe')['managed_lifecycle']['provider_ids'] == [provider['provider_id']]
            assert not state.exists()
            assert call('provider_lifecycle_status')['ready'] is False
            call('provider_start', 'unknown_field', package='/foreign')
            assert not state.exists()
            assert call('provider_start')['ready'] is True
            initial = activation()
            assert initial[0] == 1 and not released()
            admitted = json.loads(initial[1])
            assert admitted['argv'] == [str(binary), '--inference-speech-resident-worker', str(package), pin]
            assert admitted['runtime_policy'] == 'pinned-host-adapter-v1'
            assert admitted['engine_sha256'] == binary_pin and admitted['model_sha256'] == pin
            assert admitted['model_path'] == str(package / 'manifest.json')
            pid = worker_pid()
            assert call('provider_start')['ready'] is True and activation() == initial
            checks.append('lazy discovery, strict lifecycle, actual synthesis-qualified readiness and idempotent start')
            saved = None
            for case in cases:
                voice = case['voice']
                result = call('speech_generate', text=phrase if actual else 'hello', voice=voice, operation_id=voice)
                assert result['samples'] > 0 and result['provenance']['manifest_sha256'] == pin
                audio = bytearray()
                for offset in range(0, result['bytes'], 32768):
                    part = call('audio_read', audio_id=result['audio_id'], offset=offset, length=min(32768, result['bytes'] - offset))
                    audio.extend(base64.b64decode(part['pcm_base64']))
                assert hashlib.sha256(audio).hexdigest() == result['pcm_sha256']
                if actual:
                    qualify_voice(profile, case['samples'], case['pcm_sha256'], result, quality_check)
                    assert result['sample_rate'] == 24000 and result['bytes'] == result['samples'] * 2
                assert worker_pid() == pid and activation() == initial
                if voice == first_voice:
                    saved = result
            if not actual:
                assert loads.read_text() == 'C'
            checks.append('all configured voices, original quality/PCM/hash/range validation on the same loaded worker and activation')
            call('speech_generate', 'idempotency_conflict', text='changed', voice=first_voice, operation_id=first_voice)
            assert call('provider_stop')['ready'] is False and released()
            assert call('speech_generate', text=phrase if actual else 'hello', voice=first_voice, operation_id=first_voice) == saved
            assert call('audio_read', audio_id=saved['audio_id'], offset=0, length=min(8, saved['bytes']))['pcm_base64']
            offline = json.loads(json.dumps(registry))
            offline['providers'][0]['package'] = '/missing/package'
            offline_config = root / 'offline.json'
            private(offline_config, offline)
            replay = subprocess.run([str(binary), '--config', str(offline_config)], text=True, capture_output=True,
                input=json.dumps(request('speech_generate', text=phrase if actual else 'hello', voice=first_voice, operation_id=first_voice)), timeout=15)
            assert replay.returncode == 0 and json.loads(replay.stdout) == saved, replay
            foreign = json.loads(json.dumps(offline)); foreign['owner'] = 'bob'
            foreign_config = root / 'foreign.json'; private(foreign_config, foreign)
            replay = subprocess.run([str(binary), '--config', str(foreign_config)], text=True, capture_output=True,
                input=json.dumps(request('operation_get', operation_id=first_voice)), timeout=15)
            assert replay.returncode == 1 and json.loads(replay.stdout)['error']['code'] == 'unknown_operation'
            checks.append('exact stopped-worker/missing-package replay, retained audio reads and foreign-owner refusal')
            if not actual:
                session.close()
                for variable in ('SAMOSA_TEST_SPEECH_ABI', 'SAMOSA_TEST_SPEECH_FORCE_EMPTY'):
                    failed = dict(environment, **{variable: 'incompatible' if variable.endswith('ABI') else '1'})
                    session = Session(binary, config, log, failed)
                    failure = session.receive(session.submit(request('provider_start')))
                    assert 'error' in failure, failure
                    assert not call('provider_lifecycle_status')['ready'] and released() and activation() == initial
                    session.close()
                checks.append('ABI and native synthesis failures preserve exact last-good, refuse readiness and release ownership')
                session = Session(binary, config, log, environment)
                assert call('provider_start')['ready'] is True
                active = session.submit(request('speech_generate', text='partial-wait', operation_id='cancel-active', timeout_ms=10000))
                until = time.monotonic() + 5
                while not marker.exists() and time.monotonic() < until:
                    time.sleep(.01)
                assert marker.exists()
                call('provider_stop', 'provider_busy'); call('provider_start', 'provider_busy')
                call('operation_cancel', operation_id='cancel-active')
                assert session.receive(active)['error']['code'] == 'cancelled'
                assert released() and not call('provider_lifecycle_status')['ready']
                marker.unlink()
                assert call('provider_start')['ready'] is True
                call('speech_generate', 'deadline_exceeded', text='partial-wait', operation_id='deadline', timeout_ms=200)
                assert released() and not call('provider_lifecycle_status')['ready']
                marker.unlink()
                checks.append('public cancellation/deadline invalidate only their owned worker; lifecycle busy while active')
            if actual:
                assert call('provider_start')['ready'] is True
                active = session.submit(request('speech_generate', text=phrase * 20, operation_id='actual-cancel', timeout_ms=10000))
                until = time.monotonic() + 5
                while time.monotonic() < until:
                    status = session.receive(session.submit(request('operation_get', operation_id='actual-cancel')))
                    records.append(dict(op='operation_get', response=status))
                    assert status.get('error', {}).get('code') in (None, 'unknown_operation'), status
                    if status.get('status') == 'running':
                        break
                    time.sleep(.01)
                assert status.get('status') == 'running'
                call('provider_stop', 'provider_busy')
                call('operation_cancel', operation_id='actual-cancel')
                assert session.receive(active)['error']['code'] == 'cancelled'
                assert released() and not call('provider_lifecycle_status')['ready']
                assert call('provider_start')['ready'] is True
                call('speech_generate', 'deadline_exceeded', text=phrase * 20, operation_id='actual-deadline', timeout_ms=200)
                assert released() and not call('provider_lifecycle_status')['ready']
                checks.append('actual public cancellation/deadline refuse successful audio and recover through explicit activation')
            assert call('provider_start')['ready'] is True
            if not actual:
                good = activation()
                flow = package / 'flow'; flow.write_bytes(b'mutation')
                assert not call('provider_lifecycle_status')['ready']
                failure = session.receive(session.submit(request('speech_generate', text='hello', operation_id='mutation')))
                assert 'error' in failure and activation() == good
                call('provider_stop'); flow.write_bytes(b'artifact')
                assert call('provider_start')['ready'] is True
                good = activation(); os.kill(worker_pid(), signal.SIGKILL)
                until = time.monotonic() + 3
                while time.monotonic() < until:
                    if not call('provider_lifecycle_status')['ready']:
                        break
                    time.sleep(.01)
                call('provider_start', 'owned_engine_exited'); call('provider_stop', 'engine_failed')
                assert released() and activation() == good
                assert call('provider_start')['ready'] is True
                checks.append('bundle mutation and engine death refuse stale readiness/work; explicit stop/restart retains last-good')
            session.close(); assert released()
            checks.append('EOF reaps owned loaded worker and releases residency')
            if not actual:
                survivor = subprocess.Popen(['sleep', '30'])
                try:
                    session = Session(binary, config, log, environment)
                    assert call('provider_start')['ready'] is True
                    session.process.kill(); session.process.wait(timeout=5)
                    session.process.stdin.close(); session.process.stdout.close()
                    until = time.monotonic() + 3
                    while not released() and time.monotonic() < until:
                        time.sleep(.01)
                    assert released() and survivor.poll() is None
                    checks.append('SIGKILL owner releases residency after cleanup, unrelated process survives')
                finally:
                    survivor.terminate(); survivor.wait(timeout=5)
        finally:
            if session.process.poll() is None:
                session.process.kill(); session.process.wait(timeout=5)
    assert digest(args.binary) == binary_pin and digest(binary) == binary_pin
    if args.quality_profile:
        assert digest(args.quality_profile) == profile_pin
    report = dict(checks=checks, records=records, provider=provider, actual_model=actual, binary_sha256=binary_pin, quality_profile_sha256=profile_pin)
    (root / 'report.json').write_text(json.dumps(report))
    print(f'PASS speech registry lifecycle: {len(checks)} scenarios; {root}')


if __name__ == '__main__':
    main()
