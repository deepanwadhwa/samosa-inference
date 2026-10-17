#!/usr/bin/env python3
"""Resume content-addressed staging in an existing owned GitHub draft release.

Streams retained source files through gh; never copies weights or publishes a
release. Downloads each staged asset back to a streaming hash before success.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from distribution_candidate import digest


def gh_json(*args):
    return json.loads(subprocess.check_output(['gh', *args]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--release', required=True)
    parser.add_argument('--assets', required=True, type=Path)
    args = parser.parse_args()
    manifest = json.loads(args.assets.read_text())['assets']
    expected = {}
    for artifact in manifest:
        name = artifact['name']
        checksum = artifact.get('sha256', name)
        source = Path(artifact['source'])
        assert name == Path(name).name and not name.startswith('.') and len(name) <= 255
        assert len(checksum) == 64 and all(c in '0123456789abcdef' for c in checksum)
        assert source.is_file() and 0 < source.stat().st_size < 2147483648
        assert digest(source) == checksum
        artifact = dict(source=str(source), sha256=checksum, bytes=source.stat().st_size)
        if name in expected:
            assert (expected[name]['sha256'], expected[name]['bytes']) == (checksum, artifact['bytes'])
        else:
            expected[name] = artifact
    assert expected and len(expected) <= 1000

    def observe():
        release = gh_json('release', 'view', args.release, '--json', 'isDraft,assets')
        assert release['isDraft'], 'Only draft releases may be changed'
        return release['assets']

    def rename(asset, name):
        endpoint = asset['apiUrl'].removeprefix('https://api.github.com/')
        subprocess.run(['gh', 'api', '--method', 'PATCH', endpoint, '-f', 'name=' + name,
                        '-f', 'label=SHA-256 pinned runtime or QA artifact'], check=True, stdout=subprocess.DEVNULL)

    # gh's #suffix selects a display label, not the remote filename. Normalize
    # existing partial uploads before reusing any basename for another profile.
    present = set()
    for asset in observe():
        name = asset['name'] if asset['name'] in expected else asset['label']
        assert name in expected and name not in present, 'Foreign or ambiguous draft asset'
        endpoint = asset['apiUrl'].removeprefix('https://api.github.com/')
        if asset['state'] != 'uploaded':
            subprocess.run(['gh', 'api', '--method', 'DELETE', endpoint], check=True)
            continue
        assert asset['size'] == expected[name]['bytes']
        if asset["name"] != name:
            rename(asset, name)
        present.add(name)
    missing = [name for name in expected if name not in present]
    if missing:
        # A symlink supplies the exact remote basename without copying model
        # bytes or creating another hardlink to the retained source inode.
        with tempfile.TemporaryDirectory(prefix='samosa-release-aliases-') as temporary:
            aliases = {}
            for name in missing:
                alias = Path(temporary) / name
                alias.symlink_to(Path(expected[name]['source']).resolve())
                aliases[name] = str(alias) + '#' + name
            # Verify gh's basename handling on one small real asset before
            # batching. The label still permits recovery of a partial upload.
            first = min(missing, key=lambda name: expected[name]['bytes'])
            subprocess.run(['gh', 'release', 'upload', args.release, aliases[first]], check=True)
            probe = [asset for asset in observe() if asset['name'] == first or asset['label'] == first]
            assert len(probe) == 1 and probe[0]['state'] == 'uploaded'
            assert probe[0]['name'] == first and probe[0]['size'] == expected[first]['bytes']
            remaining = [aliases[name] for name in missing if name != first]
            if remaining:
                subprocess.run(['gh', 'release', 'upload', args.release, *remaining], check=True)

    staged = observe()
    assert {asset['name'] for asset in staged} == set(expected)
    def verify(asset):
        artifact = expected[asset['name']]
        assert asset['state'] == 'uploaded' and asset['size'] == artifact['bytes']
        endpoint = asset['apiUrl'].removeprefix('https://api.github.com/')
        child = subprocess.Popen(['gh', 'api', '-H', 'Accept: application/octet-stream', endpoint], stdout=subprocess.PIPE)
        checksum = hashlib.sha256()
        size = 0
        try:
            for chunk in iter(lambda: child.stdout.read(65536), b''):
                checksum.update(chunk)
                size += len(chunk)
                assert size <= artifact['bytes']
            assert child.wait() == 0 and size == artifact['bytes'] and checksum.hexdigest() == artifact['sha256']
        finally:
            child.stdout.close()
            if child.poll() is None:
                child.kill()
            child.wait()
        return asset['name']

    with ThreadPoolExecutor(max_workers=4) as workers:
        futures = [workers.submit(verify, asset) for asset in staged]
        for number, future in enumerate(as_completed(futures), 1):
            name = future.result()
            print('PASS staged asset ' + str(number) + '/' + str(len(staged)) + ' ' + name, flush=True)
    assert {asset['name'] for asset in observe()} == set(expected)
    print('PASS complete draft names, sizes and downloaded SHA-256 hashes; release remains draft', flush=True)


if __name__ == '__main__':
    main()
