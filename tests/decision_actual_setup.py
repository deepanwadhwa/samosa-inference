#!/usr/bin/env python3
"""QA: install retained decision assets and qualify fresh installed model inodes.

Source mapping streams existing weights directly; only the actual cache/payload
copies required by cold installation are made, and the exclusive QA tree is removed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import shutil
import subprocess
import sys
import tempfile
from opendecision_export import digest


def main():
    assert __debug__
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ['setup', 'public', 'inventory', 'reference', 'output']:
        parser.add_argument(name, type=Path)
    parser.add_argument('--inventory-pin', required=True)
    parser.add_argument('--reference-pin', required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--worker', type=Path)
    parser.add_argument('--worker-pin')
    parser.add_argument('--image', action='store_true')
    args = parser.parse_args()
    if args.output.exists() or args.output.is_symlink():
        raise ValueError('Qualification report already exists')
    assert digest(args.inventory) == args.inventory_pin and digest(args.reference) == args.reference_pin
    assert len(args.revision) == 40 and all(c in '0123456789abcdef' for c in args.revision)
    assert bool(args.worker) == bool(args.worker_pin)
    inventory = json.loads(args.inventory.read_bytes())
    files = inventory['files']
    assert len(files) == 4 and [f['name'] for f in files][::2] == ['decision-engine', 'model.onnx']
    assert files[1]['name'] in ['libonnxruntime.1.27.0.dylib','libonnxruntime.so.1.27.0'] and files[3]['name'] == 'tokenizer.json'
    sources = {f['name']:(Path(inventory['root'])/f['name']).resolve() for f in files}
    pins = {f['name']:f['sha256'] for f in files}
    for f in files:
        assert sources[f['name']].stat().st_size == f['bytes'] and digest(sources[f['name']]) == f['sha256']
    if args.worker:
        assert digest(args.worker) == args.worker_pin
        sources['decision-engine'] = args.worker.resolve()
        pins['decision-engine'] = args.worker_pin
    profile = 'image' if args.image else 'text'
    result = dict(format='samosa.inference.decision-installed-qualification.v1',profile=profile,
                  source_inventory_sha256=args.inventory_pin,reference_sha256=args.reference_pin,
                  runtime_revision=args.revision,asset_pins=pins,
                  setup_sha256=digest(args.setup),public_sha256=digest(args.public),status='not_executed')
    with tempfile.TemporaryDirectory(prefix='samosa-decision-actual-') as temporary:
        base = Path(temporary).resolve()
        snapshots = {}
        for name in ['setup','public']:
            snapshot = base/(name+'-qualified')
            shutil.copyfile(getattr(args,name),snapshot)
            snapshot.chmod(0o500)
            assert digest(snapshot) == result[name+'_sha256']
            snapshots[name] = snapshot
        descriptor = base/'decision-package.json'
        descriptor.write_text(json.dumps(dict(format='samosa.inference.decision-package.v1',profile=profile,
                    protocol='opendecision-framed-v1',assets='assets',runtime_revision=args.revision),separators=(',',':')))
        roles = ['engine','library','model','data']
        entries = [dict(name='assets/'+f['name'],role=roles[i],bytes=sources[f['name']].stat().st_size,sha256=pins[f['name']])
                   for i,f in enumerate(files)]
        entries.append(dict(name='decision-package.json',role='metadata',bytes=descriptor.stat().st_size,sha256=digest(descriptor)))
        # Generate a local-host QA index; native setup remains authoritative
        # for platform, minimum OS and actual executable/dependency admission.
        import platform
        system = {'Darwin':'macos','Linux':'linux'}[platform.system()]
        architecture = {'arm64':'arm64','aarch64':'arm64','x86_64':'x86_64'}[platform.machine()]
        index = base/'index.json'
        index.write_text(json.dumps(dict(format='samosa.inference.package.v1',platform=system+'-'+architecture,
                    minimum_os=platform.mac_ver()[0] if system=='macos' else platform.release().split('-')[0],
                    recipe='opendecision-package-v1',files=entries),separators=(',',':')))
        mapped = {'assets/'+name:str(path) for name,path in sources.items()}
        mapped['decision-package.json'] = str(descriptor)
        source_map = base/'sources.json'
        source_map.write_text(json.dumps(mapped,sort_keys=True))
        result.update(index_sha256=digest(index),files=len(entries),payload_bytes=sum(f['bytes'] for f in entries))
        command = [sys.executable,str(Path(__file__).with_name('llama_setup_https.py')),str(snapshots['setup']),
                   inventory['root'],str(index),digest(index),'--decision-profile',profile,'--revision',args.revision,
                   '--engine-pin',pins[files[0]['name']],'--runtime-pin',pins[files[1]['name']],
                   '--model-pin',pins[files[2]['name']],'--tokenizer-pin',pins[files[3]['name']],
                   '--source-map',str(source_map),'--source-map-pin',digest(source_map),'--workspace',str(base/'installation'),
                   '--decision-cli',str(snapshots['public']),'--decision-reference',str(args.reference.resolve()),
                   '--decision-reference-pin',args.reference_pin]
        child = subprocess.Popen(command,start_new_session=True)
        try:
            code = child.wait(timeout=5400)
        except BaseException:
            if child.poll() is None:
                os.killpg(child.pid,signal.SIGKILL)
            child.wait()
            raise
        result['status'] = 'passed' if code == 0 else 'failed'
        for name,path in sources.items():
            assert digest(path) == pins[name], 'Original source changed: '+name
        with args.output.open('x') as destination:
            json.dump(result,destination,indent=2,sort_keys=True)
            destination.write('\n')
        if code:
            raise subprocess.CalledProcessError(code,command)
    print('PASS actual '+profile+' package installation and fresh-inode public inference; temporary installation copies removed',flush=True)


if __name__ == '__main__':
    main()
