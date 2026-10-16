#!/usr/bin/env python3
"""Public decision adapter controls using the existing model-free worker fixture."""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import tempfile
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    assert __debug__
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('public', type=Path)
    parser.add_argument('worker', type=Path)
    parser.add_argument('runtime', type=Path)
    parser.add_argument('--image', action='store_true')
    args = parser.parse_args()
    checks = 0
    def check(value, detail=None):
        nonlocal checks
        checks += 1
        assert value, detail
    with tempfile.TemporaryDirectory(prefix='samosa-decision-controls-') as folder:
        base = Path(folder)
        assets = base/'assets'
        assets.mkdir(mode=0o700)
        shutil.copyfile(args.worker, assets/'decision-engine')
        (assets/'decision-engine').chmod(0o500)
        shutil.copyfile(args.runtime, assets/args.runtime.name)
        (assets/args.runtime.name).chmod(0o400)
        model, tokenizer = assets/'model.onnx', assets/'tokenizer.json'
        tokenizer.write_text('fixture initializer')
        model.write_text('normal')
        marker = base/'ready'
        config = base/'host.json'
        env = dict(os.environ, SAMOSA_DECISION_QA_MARKER=str(marker))
        provider = dict(provider_id='decision', kind='image_decisions' if args.image else 'text_decisions', state=str(base/'state'),
                        assets=str(assets), engine_sha256=sha(assets/'decision-engine'),
                        runtime_sha256=sha(assets/args.runtime.name), model_sha256=sha(model),
                        tokenizer_sha256=sha(tokenizer))
        peer = dict(provider, provider_id='peer', state=str(base/'peer'))
        owner = 'alice'
        def configure(mode=None):
            if mode:
                model.write_text(mode)
                for p in [marker, Path(str(marker)+'.release')]:
                    p.unlink(missing_ok=True)
                provider['model_sha256'] = peer['model_sha256'] = sha(model)
                provider['tokenizer_sha256'] = peer['tokenizer_sha256'] = sha(tokenizer)
            config.write_text(json.dumps(dict(contract_version=1, owner=owner, providers=[provider, peer])))
            config.chmod(0o600)
        def start(request):
            configure()
            p = subprocess.Popen([str(args.public), '--config', str(config)], stdin=subprocess.PIPE,
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
            p.stdin.write(json.dumps(request, ensure_ascii=False).encode())
            p.stdin.close()
            return p
        def finish(p, error=None):
            try:
                p.wait(timeout=15)
                out, err = p.stdout.read(), p.stderr.read()
                check(len(out) <= 1048576 and len(err) <= 262144)
                result = json.loads(out)
                check((p.returncode != 0) == bool(error), (p.returncode, result, err.decode(errors="replace")))
                if error:
                    check(result['error']['code'] == error, (error, result))
                return result
            finally:
                if p.poll() is None:
                    p.kill()
                    p.wait()
                p.stdout.close()
                p.stderr.close()
        def run(request, error=None):
            return finish(start(request), error)
        def control(op, identity, selected='decision'):
            return dict(contract_version=1, op=op, provider_id=selected, operation_id=identity)
        def work(identity, request=None, timeout=10000):
            result = dict(contract_version=1, op='decide_image' if args.image else 'decide_text', provider_id='decision', operation_id=identity,
                        timeout_ms=timeout, request=request or dict(method='noul', state='s', instructions='q'))
            if args.image:
                result.update(width=1, height=1, image_rgba_base64=base64.b64encode(bytes([255,0,0,17])).decode())
            return result
        def ready(p):
            deadline = time.monotonic()+10
            while not marker.exists():
                check(p.poll() is None)
                if time.monotonic() >= deadline:
                    raise AssertionError('worker never published ready marker')
                time.sleep(0.005)
            lines = marker.read_text().splitlines()
            check(len(lines) == 2)
            return int(lines[0]), Path(lines[1])
        def removed(path):
            deadline = time.monotonic()+10
            # Unlinking the result and removing its directory are separate
            # guardian syscalls. Observe both, without extending the deadline.
            while (path.exists() or path.parent.exists()) and time.monotonic() < deadline:
                time.sleep(0.005)
            check(not path.exists() and not path.parent.exists(), dict(path=str(path), file_exists=path.exists(), directory_exists=path.parent.exists(), entries=[p.name for p in path.parent.iterdir()] if path.parent.exists() else []))
        configure('normal')
        request = work('nul-choice', dict(method='choice', state='hi\0x', instructions='q',
                                         criteria={'z\0x':'last','a':'first'}))
        response = run(request)
        check(response['result']['choice'] == 'z\0x')
        check(list(response['result']['probabilities']) == ['z\0x', 'a'])
        check(run(request) == response)
        provider['assets'] = str(base/'absent-engine')
        check(run(request) == response)
        provider['assets'] = str(assets)
        if args.image:
            check(response['image_sha256'] == hashlib.sha256(bytes([255,0,0,17])).hexdigest())
            for change in [dict(width=2,height=1,image_rgba_base64=base64.b64encode(bytes([255,0,0,17])*2).decode()),
                           dict(image_rgba_base64=base64.b64encode(bytes([0,0,0,17])).decode())]:
                altered = dict(request, **change)
                run(altered, 'idempotency_conflict')
            shape = work('shape-binding')
            shape.update(width=1,height=2,image_rgba_base64=base64.b64encode(bytes([255,0,0,17])*2).decode())
            run(shape)
            run(dict(shape,width=2,height=1), 'idempotency_conflict')
            maximum = work('max-frame')
            maximum.update(width=512,height=512,image_rgba_base64=base64.b64encode(bytes([0,0,0,255])*(512*512)).decode())
            check(run(maximum)['result']['noul'] == 0.5)
            run(work('wrong-relation-backend',dict(method='relation',state='s',proposition='p',contradiction='c')), 'invalid_provider_response')
            bad = work('bad-image')
            bad['width'] = 513
            run(bad, 'invalid_request')
            bad = work('bad-b64')
            bad['image_rgba_base64'] = '/wAAER=='  # noncanonical padding bits
            run(bad, 'invalid_request')
            run(work('unsupported-batch', dict(method='relations',items=[])), 'invalid_request')
        changed = json.loads(json.dumps(request))
        changed['request']['criteria'] = {'a':'first','z\0x':'last'}
        run(changed, 'idempotency_conflict')
        owner = 'bob'
        run(control('operation_get', 'nul-choice'), 'unknown_operation')
        run(control('operation_cancel', 'nul-choice'), 'unknown_operation')
        owner = 'alice'
        run(control('operation_cancel', 'nul-choice', 'peer'), 'unknown_operation')
        for mode, error in [('invalid','invalid_provider_response'), ('wrong-type','invalid_provider_response'),
                            ('malformed','invalid_provider_response'), ('die','provider_process_failed'),
                            ('input-limit','context_limit')]:
            configure(mode)
            run(work(mode), error)
        configure('wait')
        active = start(work('cancel'))
        _, path = ready(active)
        check(run(control('operation_get', 'cancel'))['status'] == 'running')
        run(control('operation_cancel', 'cancel'))
        finish(active, 'cancelled')
        removed(path)
        configure('wait')
        active = start(work('deadline', timeout=1000))
        _, path = ready(active)
        finish(active, 'deadline_exceeded')
        removed(path)
        configure('wait')
        active = start(work('mutation'))
        _, path = ready(active)
        tokenizer.write_text('changed during execution')
        Path(str(marker)+'.release').touch()
        finish(active, 'artifact_changed')
        removed(path)
        configure('wait')
        active = start(work('requester-death'))
        _, path = ready(active)
        active.kill()
        check(active.wait(timeout=10) == -signal.SIGKILL)
        active.stdout.close()
        active.stderr.close()
        removed(path)
        configure('normal')
        check(run(work('healthy-after-death'))['result']['noul'] == 0.5)
    print(f'PASS public decision controls: {checks} checks; NUL/order, offline replay, typed result refusal, owner isolation, cancellation/deadline, mutation and requester-death cleanup; model-free fixture')


if __name__ == '__main__':
    main()
