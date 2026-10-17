#!/usr/bin/env python3
"""Prepare a pinned Whisper distribution candidate without copying model weights.

Release/build tool only. Native setup and actual installed inference remain
required acceptance gates before publishing the generated catalog entry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from urllib.parse import urlsplit

REVISION = 'f049fff95a089aa9969deb009cdd4892b3e74916'
ENGINE_PIN = 'aa28f124f97b13c9315734db912844d6c9b7489aefdf1ad49ea0888ce9da27d5'
MODEL_PIN = 'a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002'
MODEL_REVISION = '5359861c739e955e79d9a303bcbc70fb988958b1'
MODEL_URL = f'https://huggingface.co/ggerganov/whisper.cpp/resolve/{MODEL_REVISION}/ggml-base.en.bin'


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(65536), b''):
            result.update(block)
    return result.hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n')
    path.chmod(0o400)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'model', 'runtime-license', 'model-license', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    url = urlsplit(args.release_url)
    if url.scheme != 'https' or not url.hostname or url.username or url.password or url.query or url.fragment or args.release_url.endswith('/'):
        parser.error('release URL must be an HTTPS asset prefix without credentials, query, fragment or trailing slash')
    paths = {'whisper-cli': args.engine, 'ggml-base.en.bin': args.model,
             'LICENSE.whisper.cpp': args.runtime_license, 'LICENSE.whisper-model': args.model_license}
    expected = {'whisper-cli': ENGINE_PIN, 'ggml-base.en.bin': MODEL_PIN,
                'LICENSE.whisper.cpp': '94f29bbed6a22c35b992c5c6ebf0e7c92f13b836b90f36f461c9cf2f0f1d010d',
                'LICENSE.whisper-model': 'b5d65a59060e68c4ff940e1eddfa6f94b2d68fdf58ed7f4dd57721c997e35e9d'}
    for name, path in paths.items():
        if not path.is_absolute() or path.is_symlink() or not path.is_file():
            parser.error('inputs must be absolute regular files')
        if name in expected and digest(path) != expected[name]:
            parser.error('qualified input checksum mismatch: ' + name)
    original = {name: digest(path) for name, path in paths.items()}
    # Exclusive directory ownership: never adopt or remove an existing output.
    args.output.mkdir(mode=0o700)
    try:
        base = args.output.resolve()
        payload = base / 'payload'
        payload.mkdir(mode=0o700)
        for name in ('LICENSE.whisper.cpp', 'LICENSE.whisper-model'):
            shutil.copyfile(paths[name], payload / name)
            (payload / name).chmod(0o400)
            paths[name] = payload / name
        write(payload / 'whisper-package.json', dict(format='samosa.inference.whisper-package.v1',
              runtime_revision=REVISION, profile='base.en-f16', engine='whisper-cli', model='ggml-base.en.bin'))
        write(payload / 'package-provenance.json', dict(format='samosa.inference.whisper-distribution-provenance.v1',
              runtime_revision=REVISION, runtime_source_sha256='d8cd961352377b1cc612224016a9ebdfe0ae508dc2b2f9ef514b341d672e3fdc',
              model_revision=MODEL_REVISION, model_url=MODEL_URL, asset_sha256=original,
              minimum_os='14.0', execution_qualified_os='26.5.1', platform='macos-arm64',
              license_sources=dict(runtime=f'https://raw.githubusercontent.com/ggml-org/whisper.cpp/{REVISION}/LICENSE',
                  model='https://raw.githubusercontent.com/openai/whisper/86098128c0b4f24f0e2aa2994de830614b474227/LICENSE'),
              build_profile='static CPU; Metal/BLAS/OpenMP/backend plugins disabled',
              qualification='distribution candidate; requires exact installed package acceptance'))
        for path in payload.iterdir():
            paths[path.name] = path
        files = [dict(name=name, role='engine' if name == 'whisper-cli' else 'model' if name == 'ggml-base.en.bin' else 'metadata',
                      bytes=path.stat().st_size, sha256=digest(path)) for name, path in sorted(paths.items())]
        index = base / 'index.json'
        write(index, dict(format='samosa.inference.package.v1', platform='macos-arm64', minimum_os='14.0',
                          recipe='whisper-ggml-package-v1', files=files))
        pin = digest(index)
        entry = dict(id='whisper-base-en-cpu', version='1', url=args.release_url + '/' + pin,
                     license='MIT; ggml authors and OpenAI notices included in package', platform='macos-arm64',
                     minimum_os='14.0', runtime='system-runtime-v1', recipe='whisper-ggml-package-v1', sha256=pin, size=index.stat().st_size)
        write(base / 'catalog.json', dict(catalog_version=1, revision='whisper-base-en-cpu-v1', entries=[entry]))
        write(base / 'sources.json', dict(format='samosa.inference.package-sources.v1', index_sha256=pin,
              files=[dict(url=MODEL_URL if file['role'] == 'model' else args.release_url + '/' + file['sha256']) for file in files]))
        write(base / 'host-policy.json', dict(format='samosa.inference.whisper-distribution-host-policy.v1',
              runtime_revision=REVISION, profile='base.en-f16', engine_sha256=ENGINE_PIN, model_sha256=MODEL_PIN,
              redirect_hosts=['release-assets.githubusercontent.com', 'us.aws.cdn.hf.co'],
              qualification='macOS26.5.1 arm64 execution;14.0 observed deployment metadata only'))
        write(base / 'local-sources.json', {name: str(path.resolve()) for name, path in paths.items()})
        write(base / 'release-assets.json', dict(index_sha256=pin, assets=[dict(name=file['sha256'], source=str(paths[file['name']].resolve()))
              for file in files if file['role'] != 'model'] + [dict(name=pin, source=str(index))]))
        for name, checksum in original.items():
            source = {'whisper-cli': args.engine, 'ggml-base.en.bin': args.model,
                      'LICENSE.whisper.cpp': args.runtime_license, 'LICENSE.whisper-model': args.model_license}[name]
            if digest(source) != checksum:
                raise RuntimeError('source changed: ' + name)
        print(json.dumps(dict(root=str(payload), index=str(index), pin=pin,
                             source_map=str(base / 'local-sources.json'), files=len(files), payload_bytes=sum(f['bytes'] for f in files))))
    except BaseException:
        shutil.rmtree(args.output)
        raise


if __name__ == '__main__':
    main()
