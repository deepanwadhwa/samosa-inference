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
        assert name not in expected and source.is_file() and 0 < source.stat().st_size < 2147483648
        assert digest(source) == checksum
        expected[name] = dict(source=str(source), sha256=checksum, bytes=source.stat().st_size)
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
        rename(asset, name)
        present.add(name)
    for name, artifact in expected.items():
        if name in present:
            continue
        subprocess.run(['gh', 'release', 'upload', args.release, artifact['source'] + '#' + name], check=True)
        matches = [asset for asset in observe() if asset['label'] == name]
        assert len(matches) == 1 and matches[0]['state'] == 'uploaded' and matches[0]['size'] == artifact['bytes']
        rename(matches[0], name)
        present.add(name)

    staged = observe()
    assert {asset['name'] for asset in staged} == set(expected)
    for number, asset in enumerate(staged, 1):
        artifact = expected[asset['name']]
        endpoint = asset['apiUrl'].removeprefix('https://api.github.com/')
        child = subprocess.Popen(['gh', 'api', '-H', 'Accept: application/octet-stream', endpoint], stdout=subprocess.PIPE)
        checksum = hashlib.sha256()
        size = 0
        for chunk in iter(lambda: child.stdout.read(65536), b''):
            checksum.update(chunk)
            size += len(chunk)
        assert child.wait() == 0 and size == artifact['bytes'] and checksum.hexdigest() == artifact['sha256']
        print('PASS staged asset ' + str(number) + '/' + str(len(staged)), flush=True)
    print('PASS complete draft names, sizes and downloaded SHA-256 hashes; release remains draft', flush=True)


if __name__ == '__main__':
    main()
