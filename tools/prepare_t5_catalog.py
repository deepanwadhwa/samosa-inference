#!/usr/bin/env python3
"""Prepare the existing qualified native T5 engine/model for distribution.

Build/release tool only; shares metadata preparation with Whisper. Model weights
are read in place and never copied into the metadata output or source repository.
"""
import argparse
import json
from pathlib import Path
from distribution_candidate import prepare

REVISION = '9fcaed763ccda38ea81068ad9d7f991aaddca451'
ENGINE_PIN = 'f3748ba69924338abcabbcffea32e6f567c73b00c0c1f9d38def7a984e708ecf'
MODEL_PIN = '30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c'
MODEL_REVISION = '6e505f907968c4a9360773ff57885cdc6dca4bfd'
MODEL_SOURCE = f'https://huggingface.co/Falconsai/text_summarization/tree/{MODEL_REVISION}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'model', 'runtime-license', 'sidecar-license', 'model-license', 'model-card', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    paths = {'summary-engine': args.engine, 'model.gguf': args.model, 'LICENSE.Prism': args.runtime_license,
             'LICENSE.Samosa': args.sidecar_license, 'LICENSE.model': args.model_license, 'MODEL-CARD.md': args.model_card}
    expected = {'summary-engine': ENGINE_PIN, 'model.gguf': MODEL_PIN,
                'LICENSE.Prism': '94f29bbed6a22c35b992c5c6ebf0e7c92f13b836b90f36f461c9cf2f0f1d010d',
                'LICENSE.Samosa': 'c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4',
                'LICENSE.model': 'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30',
                'MODEL-CARD.md': '679677311bc9f98673bbbf4a469e1fec92ba472dcbe04ea0a78f1765451d8600'}
    descriptor = dict(format='samosa.inference.llama-package.v1', runtime_revision=REVISION,
                      kind='summary', architecture='t5', engine='summary-engine', model='model.gguf')
    provenance = dict(format='samosa.inference.t5-distribution-provenance.v1', runtime_revision=REVISION,
                      runtime_source=f'https://github.com/PrismML-Eng/llama.cpp/tree/{REVISION}',
                      engine_source_sha256='4d5f82a431c23364431c0bddb90c91b12f0b7d1cc14db5c7113cda6651375f21',
                      sidecar_license='Apache-2.0; LICENSE.Samosa included',
                      model_revision=MODEL_REVISION, model_source=MODEL_SOURCE, asset_sha256=expected,
                      conversion='Original safetensors converted to GGUF Q8_0; retained qualified bytes, no new conversion',
                      build_profile='static CPU; Metal/BLAS/OpenMP/backend plugins/OpenSSL disabled',
                      minimum_os='14.0', execution_qualified_os='26.5.1', platform='macos-arm64',
                      qualification='distribution candidate; requires exact installed package acceptance')
    notice = ('Falconsai/text_summarization, pinned revision ' + MODEL_REVISION + '.\n'
              'Upstream model card declares Apache-2.0; the complete card and license are included.\n'
              'Source: ' + MODEL_SOURCE + '\n'
              'Modified for Samosa: converted original safetensors to GGUF Q8_0.\n'
              'This distribution reuses the already-qualified converted model without additional conversion.\n'
              'The Samosa native sidecar is distributed under Apache-2.0; see LICENSE.Samosa.\n'
              'Native T5 sidecar adapts the MIT-licensed llama.cpp encoder/decoder example,\n'
              'linked against pinned Prism; the ggml authors notice is included in LICENSE.Prism.\n')
    policy = dict(format='samosa.inference.t5-distribution-host-policy.v1', runtime_revision=REVISION,
                  kind='summary', architecture='t5', engine_sha256=ENGINE_PIN, model_sha256=MODEL_PIN,
                  redirect_hosts=['release-assets.githubusercontent.com'],
                  qualification='macOS26.5.1 arm64 execution;14.0 observed deployment metadata only')
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected,
                     engine='summary-engine', model='model.gguf', recipe='llama-gguf-package-v1', provider='t5-summary-cpu',
                     license_notice='Apache-2.0 model and Samosa sidecar; MIT Prism dependencies; notices included',
                     metadata={'llama-package.json': descriptor, 'package-provenance.json': provenance, 'NOTICE.model': notice},
                     policy=policy)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
