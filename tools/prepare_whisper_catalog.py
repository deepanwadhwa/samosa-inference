#!/usr/bin/env python3
"""Prepare a pinned Whisper distribution candidate without copying model weights.

Release/build tool only. Native setup and actual installed inference remain
required acceptance gates before publishing the generated catalog entry.
"""
import argparse
import json
from pathlib import Path
from distribution_candidate import prepare

REVISION = 'f049fff95a089aa9969deb009cdd4892b3e74916'
ENGINE_PIN = 'aa28f124f97b13c9315734db912844d6c9b7489aefdf1ad49ea0888ce9da27d5'
MODEL_PIN = 'a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002'
MODEL_REVISION = '5359861c739e955e79d9a303bcbc70fb988958b1'
MODEL_URL = f'https://huggingface.co/ggerganov/whisper.cpp/resolve/{MODEL_REVISION}/ggml-base.en.bin'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'model', 'runtime-license', 'model-license', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    paths = {'whisper-cli': args.engine, 'ggml-base.en.bin': args.model,
             'LICENSE.whisper.cpp': args.runtime_license, 'LICENSE.whisper-model': args.model_license}
    expected = {'whisper-cli': ENGINE_PIN, 'ggml-base.en.bin': MODEL_PIN,
                'LICENSE.whisper.cpp': '94f29bbed6a22c35b992c5c6ebf0e7c92f13b836b90f36f461c9cf2f0f1d010d',
                'LICENSE.whisper-model': 'b5d65a59060e68c4ff940e1eddfa6f94b2d68fdf58ed7f4dd57721c997e35e9d'}
    descriptor = dict(format='samosa.inference.whisper-package.v1', runtime_revision=REVISION,
                      profile='base.en-f16', engine='whisper-cli', model='ggml-base.en.bin')
    provenance = dict(format='samosa.inference.whisper-distribution-provenance.v1',
              runtime_revision=REVISION, runtime_source_sha256='d8cd961352377b1cc612224016a9ebdfe0ae508dc2b2f9ef514b341d672e3fdc',
              model_revision=MODEL_REVISION, model_url=MODEL_URL, asset_sha256=expected,
              minimum_os='14.0', execution_qualified_os='26.5.1', platform='macos-arm64',
              license_sources=dict(runtime=f'https://raw.githubusercontent.com/ggml-org/whisper.cpp/{REVISION}/LICENSE',
                  model='https://raw.githubusercontent.com/openai/whisper/86098128c0b4f24f0e2aa2994de830614b474227/LICENSE'),
              build_profile='static CPU; Metal/BLAS/OpenMP/backend plugins disabled',
              qualification='distribution candidate; requires exact installed package acceptance')
    policy = dict(format='samosa.inference.whisper-distribution-host-policy.v1',
              runtime_revision=REVISION, profile='base.en-f16', engine_sha256=ENGINE_PIN, model_sha256=MODEL_PIN,
              redirect_hosts=['release-assets.githubusercontent.com', 'us.aws.cdn.hf.co'],
              qualification='macOS26.5.1 arm64 execution;14.0 observed deployment metadata only')
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected,
                     engine='whisper-cli', model='ggml-base.en.bin', recipe='whisper-ggml-package-v1',
                     provider='whisper-base-en-cpu', license_notice='MIT; ggml authors and OpenAI notices included in package',
                     metadata={'whisper-package.json': descriptor, 'package-provenance.json': provenance},
                     policy=policy, upstream={'ggml-base.en.bin': MODEL_URL})
    print(json.dumps(result))


if __name__ == '__main__':
    main()
