#!/usr/bin/env python3
"""Collect notices from existing checksum-verified locked tokenizer sources.

Release tooling only, Python3.11+. Uses offline Cargo metadata, creates an exclusive output,
and does not install tools, modify dependencies or download model weights.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tomllib
import tarfile
from distribution_candidate import digest, write


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    module = Path(__file__).resolve().parent.parent
    manifest = module / 'engines/decision/tokenizer/Cargo.toml'
    lock = manifest.with_name('Cargo.lock')
    if digest(lock) != '0acd561bb716bf51a086ae8afcb96bacd646e49b5a47caee904f49b7dc997d4f':
        raise ValueError('tokenizer lock differs from the qualified worker')
    packages = json.loads(subprocess.check_output(['cargo', 'metadata', '--locked', '--offline', '--format-version', '1',
                         '--filter-platform', 'aarch64-apple-darwin', '--manifest-path', str(manifest)]))['packages']
    locked = {(p['name'], p['version']): p.get('checksum') for p in tomllib.loads(lock.read_text())['package']}
    notice = bytearray(b'Locked tokenizer dependency notices\nTarget: aarch64-apple-darwin\n\n')
    inventory = []
    for package in sorted(packages, key=lambda p: (p['name'], p['version'])):
        if package['name'] == 'samosa-decision-tokenizer':
            continue
        root = Path(package['manifest_path']).parent
        pin = locked[(package['name'], package['version'])]
        archive = root.parent.parent.parent / 'cache' / root.parent.name / (root.name + '.crate')
        assert pin and digest(archive) == pin
        row = dict(name=package['name'], version=package['version'], license=package['license'], source_checksum=pin, files=[])
        notice.extend(('Package: ' + package['name'] + ' ' + package['version'] + '\nLicense: ' + str(package['license']) + '\n').encode())
        with tarfile.open(archive, 'r:gz') as source:
            manifest_bytes = source.extractfile(root.name + '/Cargo.toml').read()
            assert tomllib.loads(manifest_bytes.decode())['package'].get('license') == package['license']
            files = sorted((file for file in source.getmembers() if file.isfile() and
                            Path(file.name).name.upper().startswith(('LICENSE', 'NOTICE', 'COPYING'))), key=lambda file: file.name)
            assert files, 'Missing source notice: ' + package['name']
            for file in files:
                relative = Path(file.name).relative_to(root.name).as_posix()
                assert '..' not in Path(relative).parts
                body = source.extractfile(file).read()
                actual = hashlib.sha256(body).hexdigest()
                row['files'].append(dict(name=relative, sha256=actual))
                notice.extend(('\nFile: ' + relative + '\nSHA-256: ' + actual + '\n\n').encode() + body + b'\n')
        assert digest(archive) == pin, 'Source archive changed'
        inventory.append(row)
        notice.extend(b'\n' + b'=' * 72 + b'\n\n')
    args.output.mkdir(mode=0o700)
    (args.output / 'RUST-NOTICES.txt').write_bytes(notice)
    (args.output / 'RUST-NOTICES.txt').chmod(0o400)
    write(args.output / 'RUST-NOTICES.json', dict(format='samosa.inference.tokenizer-source-notices.v1',
          target='aarch64-apple-darwin', cargo_lock_sha256=digest(lock), packages=inventory))
    print(json.dumps(dict(packages=len(inventory), notices_sha256=digest(args.output / 'RUST-NOTICES.txt'),
                          inventory_sha256=digest(args.output / 'RUST-NOTICES.json'))))


if __name__ == '__main__':
    main()
