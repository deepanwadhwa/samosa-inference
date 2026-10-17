#!/usr/bin/env python3
"""Collect speech dependency notices from separately pinned retained sources.

Release tooling only. Does not extract archives, download files or modify source
files. The caller supplies an independently pinned inventory of source archives;
this collector does not establish that the inventory is dependency-complete.
"""
import argparse
import hashlib
import gzip
import io
import json
from pathlib import Path
import tarfile
import zipfile
from distribution_candidate import digest, write


def notices(archive):
    def wanted(name):
        path = Path(name)
        assert not path.is_absolute() and '..' not in path.parts
        return path.name.upper().startswith(('LICENSE', 'COPYING', 'NOTICE'))
    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as source:
            for file in sorted(source.infolist(), key=lambda file: file.filename):
                if not file.is_dir() and wanted(file.filename):
                    assert file.file_size <= 16777216
                    yield file.filename, source.read(file)
    else:
        with tarfile.open(archive) as source:
            for file in sorted(source.getmembers(), key=lambda file: file.name):
                if file.isfile() and wanted(file.name):
                    assert file.size <= 16777216
                    yield file.name, source.extractfile(file).read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', required=True, type=Path)
    parser.add_argument('--inventory-pin', required=True)
    parser.add_argument('--sources', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    body = args.inventory.read_bytes()
    assert hashlib.sha256(body).hexdigest() == args.inventory_pin
    entries = json.loads(body)
    rows, output, names = [], bytearray(b'Speech source dependency notices\n\n'), set()
    for entry in entries:
        name = entry['archive']
        assert Path(name).name == name and not name.startswith('.') and name not in names
        names.add(name)
        archive = args.sources / name
        assert digest(archive) == entry['sha256']
        row = dict(entry, notices=[])
        output.extend(('Source: ' + name + '\nSHA-256: ' + entry['sha256'] + '\nURL: ' + entry['url'] + '\n').encode())
        for relative, text in notices(archive):
            pin = hashlib.sha256(text).hexdigest()
            row['notices'].append(dict(name=relative, sha256=pin, bytes=len(text)))
            output.extend(('\nFile: ' + relative + '\nSHA-256: ' + pin + '\n\n').encode() + text + b'\n')
        assert row['notices'], 'Missing notices: ' + name
        assert digest(archive) == entry['sha256'], 'Source changed: ' + name
        rows.append(row)
    assert rows
    args.output.mkdir(mode=0o700, exist_ok=False)
    (args.output / 'SPEECH-SOURCE-NOTICES.txt').write_bytes(output)
    (args.output / 'SPEECH-SOURCE-NOTICES.txt').chmod(0o400)
    write(args.output / 'SPEECH-SOURCE-NOTICES.json', dict(
        format='samosa.inference.speech-source-notices.v1', inventory_sha256=args.inventory_pin, sources=rows))
    instructions = (
        "Corresponding sources for the retained Sherpa speech runtime\n\n"
        "sources.json pins the enclosed unmodified source archives. Sherpa's full\n"
        "source includes its CMake build rules, new-release.sh and the macOS build\n"
        "workflow at .github/workflows/macos.yaml. That workflow configures Release,\n"
        "BUILD_SHARED_LIBS=ON and SHERPA_ONNX_ENABLE_TTS=ON, builds and installs with\n"
        "CMake, then thins the universal output to arm64 using lipo. Refer to the\n"
        "enclosed dependency CMake files for their expected archive/cache names or\n"
        "FetchContent source-directory overrides. The archives include original\n"
        "licenses and build scripts; no model weights are included.\n\n"
        "The retained runtime reports Sherpa 1.13.4, Git 14280725. Its C API imports\n"
        "the colocated libonnxruntime.1.27.0.dylib plus system libraries. ORT is a\n"
        "separate MIT-licensed shared dependency; its source and build instructions\n"
        "are at https://github.com/microsoft/onnxruntime/tree/v1.27.0 .\n"
        "No Sherpa or eSpeak source modifications were made by this packaging step.\n"
        "Existing Mach-O load commands and architecture are retained. This source\n"
        "collection does not establish byte-identical reproduction of upstream\n"
        "release binaries or independently certify dependency completeness.\n"
    ).encode()
    bundle = args.output / 'speech-corresponding-sources-v1.tar.gz'
    with bundle.open('xb') as destination:
        with gzip.GzipFile(filename='', mode='wb', fileobj=destination, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode='w') as archive:
                def add(name, content):
                    info = tarfile.TarInfo(name)
                    info.size, info.mode, info.mtime = len(content), 0o444, 0
                    archive.addfile(info, io.BytesIO(content))
                add('sources.json', body)
                add('BUILDING.txt', instructions)
                add('NOTICES.txt', output)
                for entry in entries:
                    source = args.sources / entry['archive']
                    assert digest(source) == entry['sha256']
                    info = tarfile.TarInfo('sources/' + entry['archive'])
                    info.size, info.mode, info.mtime = source.stat().st_size, 0o444, 0
                    with source.open('rb') as retained:
                        archive.addfile(info, retained)
                    assert digest(source) == entry['sha256']
    bundle.chmod(0o400)
    print(json.dumps(dict(sources=len(rows), notice_bytes=len(output),
                         notices_sha256=digest(args.output / 'SPEECH-SOURCE-NOTICES.txt'),
                         inventory_sha256=digest(args.output / 'SPEECH-SOURCE-NOTICES.json'),
                         source_bundle_sha256=digest(bundle))))


if __name__ == '__main__':
    main()
