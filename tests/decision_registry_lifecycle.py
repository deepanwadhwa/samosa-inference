#!/usr/bin/env python3
"""Existing persistent text decisions through owned registry activation."""
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
from test_decision_public import reference_difference


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('binary',type=Path)
    p.add_argument('--worker',type=Path)
    p.add_argument('--image',action='store_true')
    p.add_argument('--engine-pin')
    p.add_argument('--runtime',type=Path)
    p.add_argument('--assets',type=Path)
    p.add_argument('--reference',type=Path)
    p.add_argument('--reference-pin')
    p.add_argument('--host-policy',type=Path)
    p.add_argument('--output',type=Path)
    args=p.parse_args();binary_pin=digest(args.binary)
    root=args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='samosa-decision-registry-')).resolve()
    if args.output:root.mkdir(mode=0o700)
    actual=args.assets is not None
    if actual:
        assert args.reference and args.reference_pin and args.host_policy
        assert digest(args.reference)==args.reference_pin
        policy=json.loads(args.host_policy.read_text());assert policy['profile']==('image' if args.image else 'text') and policy['reference_sha256']==args.reference_pin
        assets=args.assets.resolve();runtime_name='libonnxruntime.1.27.0.dylib' if os.uname().sysname=='Darwin' else 'libonnxruntime.so.1.27.0'
    else:
        assert args.worker and args.runtime
        assets=root/'assets';assets.mkdir(mode=0o700);runtime_name=args.runtime.name
        for source,name in [(args.worker,'decision-engine'),(args.runtime,runtime_name)]:
            shutil.copyfile(source,assets/name);(assets/name).chmod(0o500 if name=='decision-engine' else 0o400)
        (assets/'model.onnx').write_text('normal');(assets/'tokenizer.json').write_text('fixture initializer')
        (assets/'model.onnx').chmod(0o600);(assets/'tokenizer.json').chmod(0o600)
    names=['decision-engine',runtime_name,'model.onnx','tokenizer.json'];pins=[digest(assets/n) for n in names]
    if actual:
        assert pins==[args.engine_pin or policy['engine_sha256']]+[policy[k+'_sha256'] for k in ['runtime','model','tokenizer']]
    kind='image_decisions' if args.image else 'text_decisions';operation='decide_image' if args.image else 'decide_text'
    provider=dict(kind=kind,provider_id='decision',assets=str(assets),state=str(root/'state'),
        lifecycle='resident-image-decision-v1' if args.image else 'resident-text-decision-v1',engine_size=(assets/names[0]).stat().st_size,model_size=(assets/names[2]).stat().st_size,
        **dict(zip(['engine_sha256','runtime_sha256','model_sha256','tokenizer_sha256'],pins)))
    state,config=root/'state',root/'registry.json';registry=dict(contract_version=1,owner='qa',providers=[provider,dict(kind='fake_text',provider_id='peer',state=str(root/'peer'))]);private(config,registry)
    mode,marker=root/'mode',root/'marker';images=root/'images.log';mode.write_text('normal');env=os.environ.copy()
    if not actual:env.update(SAMOSA_DECISION_QA_MODE_FILE=str(mode),SAMOSA_DECISION_QA_MARKER=str(marker),SAMOSA_DECISION_QA_IMAGES=str(images))
    records,checks=[],[];maximum=0.0
    payload=dict(method='choice',state='The sky is blue.',instructions='Choose the supported statement.',criteria={'blue':'The sky is blue.','red':'The sky is red.'})
    def request(op,**extra):
        body=dict(contract_version=1,op=op,**({} if op=='describe' else dict(provider_id='decision')))
        if args.image and op==operation:body.update(width=1,height=1,image_rgba_base64=base64.b64encode(bytes([255,0,0,255])).decode())
        body.update(extra);return body
    def call(op,expected=None,**extra):
        r=session.receive(session.submit(request(op,**extra)));records.append(dict(op=op,response=r))
        assert r.get('error',{}).get('code')==expected,r
        return r
    def activation():
        with sqlite3.connect(f'file:{state}/activation/activation.sqlite3?mode=ro',uri=True) as db:return db.execute('SELECT revision,config,digest FROM current WHERE id=1').fetchone()
    def released():
        with (state/'activation/residency.lock').open('rb') as fd:
            try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB);return True
            except BlockingIOError:return False
    def worker_pid():
        lines=subprocess.check_output(['ps','-axo','pid,args'],text=True).splitlines()
        ids=[int(x.split(None,1)[0]) for x in lines if len(x.split(None,1))==2 and x.split(None,1)[1].startswith(str(assets/'decision-engine')+' --model ')]
        assert len(ids)==1,ids
        return ids[0]
    def wait_marker():
        until=time.monotonic()+5
        while not marker.exists() and time.monotonic()<until:time.sleep(.01)
        assert marker.exists()
    with (root/'stderr.log').open('w') as log:
        session=Session(args.binary.resolve(),config,log,env)
        try:
            assert call('describe')['managed_lifecycle']['provider_ids']==['decision'] and not state.exists()
            assert not call('provider_lifecycle_status')['ready'];call('provider_start','unknown_field',assets='/foreign');assert not state.exists()
            assert call('provider_start')['ready'];good=activation();assert good[0]==1 and not released()
            admitted=json.loads(good[1]);assert admitted['runtime_policy']=='closed-native-bundle-v1'
            bundle=dict(profile=kind,asset_sha256=pins)
            identity=hashlib.sha256(json.dumps(bundle,separators=(',',':')).encode()).hexdigest();assert admitted['bundle_identity']==identity and admitted['bundle_config']==bundle
            assert call('provider_start')['ready'] and activation()==good;pid=worker_pid()
            checks.append('lazy discovery, strict lifecycle, closed native graph and actual typed warm request before readiness')
            saved=call(operation,request=payload,operation_id='saved');assert saved['provenance']['asset_sha256']==pins
            if actual:
                reference=json.loads(args.reference.read_text());assert len(reference['cases'])==(20 if args.image else 10)
                for i,case in enumerate(reference['cases']):
                    extra={};typed=dict(method=case['method'],**case['arguments'])
                    if args.image:
                        state_image=case['state'];source=args.reference.parent/state_image['file'];assert source.parent==args.reference.parent and digest(source)==state_image['sha256']
                        rgb=source.read_bytes();assert len(rgb)==state_image['width']*state_image['height']*3
                        rgba=b''.join(rgb[n:n+3]+bytes([(n//3)%256]) for n in range(0,len(rgb),3))
                        extra=dict(width=state_image['width'],height=state_image['height'],image_rgba_base64=base64.b64encode(rgba).decode());typed['state']='prepared-frame'
                    result=call(operation,request=typed,operation_id=f'case-{i}',timeout_ms=120000,**extra)
                    if args.image:assert result['image_sha256']==hashlib.sha256(rgba).hexdigest()
                    maximum=max(maximum,reference_difference(result['result'],case['result']))
                    assert worker_pid()==pid and activation()==good
            else:
                for i in range(3):
                    extra={}
                    if args.image:
                        w,h=[(2,1),(1,3),(512,512)][i];rgba=bytes([i*80,255-i*80,40,i])*w*h
                        extra=dict(width=w,height=h,image_rgba_base64=base64.b64encode(rgba).decode())
                    result=call(operation,request=payload,operation_id=f'repeat-{i}',**extra)
                    assert result['result']==saved['result'] and worker_pid()==pid and activation()==good
                    if args.image:assert result['image_sha256']==hashlib.sha256(rgba).hexdigest() and result['width']==w and result['height']==h
                if args.image:
                    observed=[line.split() for line in images.read_text().splitlines()];assert len({row[0] for row in observed})==1 and len({row[3] for row in observed})==4
                mode.write_text('input-limit');call(operation,'context_limit',request=payload,operation_id='semantic-refusal')
                assert call('provider_lifecycle_status')['ready'] and worker_pid()==pid
                mode.write_text('normal');assert call(operation,request=payload,operation_id='after-refusal')['result']==saved['result']
            checks.append('same loaded worker/revision, original typed results/provenance and reference corpus or complete semantic refusal recovery')
            changed=dict(payload,instructions='changed');call(operation,'idempotency_conflict',request=changed,operation_id='saved')
            assert not call('provider_stop')['ready'] and released();assert call(operation,request=payload,operation_id='saved')==saved
            offline=json.loads(json.dumps(registry));offline['providers'][0]['assets']='/missing/assets';offline_config=root/'offline.json';private(offline_config,offline)
            r=subprocess.run([str(args.binary.resolve()),'--config',str(offline_config)],input=json.dumps(request(operation,request=payload,operation_id='saved')),text=True,capture_output=True,timeout=15)
            assert r.returncode==0 and json.loads(r.stdout)==saved,r
            offline['owner']='bob';foreign=root/'foreign.json';private(foreign,offline)
            r=subprocess.run([str(args.binary.resolve()),'--config',str(foreign)],input=json.dumps(request('operation_get',operation_id='saved')),text=True,capture_output=True,timeout=15)
            assert r.returncode==1 and json.loads(r.stdout)['error']['code']=='unknown_operation'
            checks.append('exact stopped-worker/missing-assets replay, idempotency conflict and foreign-owner refusal')
            if not actual:
                for bad in ['invalid','wrong-type','malformed','die']:
                    mode.write_text(bad);r=session.receive(session.submit(request('provider_start')));assert 'error' in r,r
                    assert not call('provider_lifecycle_status')['ready'] and released() and activation()==good
                mode.write_text('normal');assert call('provider_start')['ready']
                checks.append('failed warm native/typed response qualification preserves exact last-good and releases ownership')
                mode.write_text('wait');active=session.submit(request(operation,request=payload,operation_id='cancel-active',timeout_ms=10000));wait_marker()
                active_pid=worker_pid()
                peer=session.receive(session.submit(dict(contract_version=1,op='generate',provider_id='peer',prompt='healthy peer',max_output_tokens=8,operation_id='peer-while-active')))
                assert 'error' not in peer,peer;records.append(dict(op='peer_generate',response=peer))
                call(operation,'deadline_exceeded',request=payload,operation_id='queued-deadline',timeout_ms=200)
                assert worker_pid()==active_pid
                call('provider_stop','provider_busy');call('provider_start','provider_busy');call('operation_cancel',operation_id='cancel-active')
                assert session.receive(active)['error']['code']=='cancelled' and released() and not call('provider_lifecycle_status')['ready'];marker.unlink()
                mode.write_text('normal');assert call('provider_start')['ready'];mode.write_text('wait')
                call(operation,'deadline_exceeded',request=payload,operation_id='deadline',timeout_ms=200)
                assert released() and not call('provider_lifecycle_status')['ready'];marker.unlink();mode.write_text('normal')
                checks.append('active/queued native cancellation/deadline isolation, healthy peer, busy lifecycle exclusion and confirmed lease release')
            assert call('provider_start')['ready']
            if not actual:
                good=activation();tokenizer=assets/'tokenizer.json';original=tokenizer.read_bytes();tokenizer.write_bytes(b'mutation')
                assert not call('provider_lifecycle_status')['ready'];r=session.receive(session.submit(request(operation,request=payload,operation_id='mutation')));assert 'error' in r and activation()==good
                call('provider_stop');tokenizer.write_bytes(original);assert call('provider_start')['ready'];good=activation();os.kill(worker_pid(),signal.SIGKILL)
                until=time.monotonic()+3
                while time.monotonic()<until:
                    if not call('provider_lifecycle_status')['ready']:break
                    time.sleep(.01)
                call('provider_start','owned_engine_exited');call('provider_stop','engine_failed');assert released() and activation()==good
                assert call('provider_start')['ready'];checks.append('non-model bundle mutation and engine crash refuse stale readiness; explicit restart preserves last-good')
            else:
                good=activation();os.kill(worker_pid(),signal.SIGKILL);until=time.monotonic()+5
                while time.monotonic()<until:
                    if not call('provider_lifecycle_status')['ready']:break
                    time.sleep(.01)
                call('provider_start','owned_engine_exited');call('provider_stop','engine_failed');assert released() and activation()==good
                assert call('provider_start')['ready'];call(operation,request=payload,operation_id='actual-crash-recovered')
                checks.append('actual engine death refuses stale readiness; stop/requalification and native inference recover without changing last-good')
            session.close();assert released();checks.append('EOF reaps loaded engine and releases residency')
            if not actual:
                survivor=subprocess.Popen(['sleep','30'])
                try:
                    session=Session(args.binary.resolve(),config,log,env);assert call('provider_start')['ready'];session.process.kill();session.process.wait(timeout=5);session.process.stdin.close();session.process.stdout.close()
                    until=time.monotonic()+3
                    while not released() and time.monotonic()<until:time.sleep(.01)
                    assert released() and survivor.poll() is None;checks.append('SIGKILL owner cleanup retains unrelated process')
                finally:survivor.terminate();survivor.wait(timeout=5)
        finally:
            if session.process.poll() is None:session.process.kill();session.process.wait(timeout=5)
    assert digest(args.binary)==binary_pin
    if actual:assert digest(args.reference)==args.reference_pin
    (root/'report.json').write_text(json.dumps(dict(checks=checks,records=records,provider=provider,actual_model=actual,binary_sha256=binary_pin,maximum_reference_error=maximum,image_profile=args.image,candidate_engine_pin=args.engine_pin)))
    print(f'PASS decision registry lifecycle: {len(checks)} scenarios; {root}')


if __name__=='__main__':main()
