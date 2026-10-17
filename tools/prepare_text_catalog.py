#!/usr/bin/env python3
"""Prepare retained Ornith/llama.cpp text assets with the existing package recipe."""
import argparse
import json
from pathlib import Path
from distribution_candidate import prepare

REVISION = '4fbc76dec51d0add466f0210855c0596589b60d4'
ENGINE = '958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97'
MODEL = '5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106'
MODEL_REVISION = '3296bc7a404871a72ac3f1903f561459c09b5c17'
MODEL_URL = f'https://huggingface.co/deepreinforce-ai/Ornith-1.0-9B-GGUF/resolve/{MODEL_REVISION}/ornith-1.0-9b-Q4_K_M.gguf'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'model', 'runtime-license', 'model-card', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    paths = {'llama-server': args.engine.resolve(), 'model.gguf': args.model.resolve(),
             'LICENSE.llama.cpp': args.runtime_license.resolve(), 'MODEL-CARD.md': args.model_card.resolve()}
    expected = {'llama-server': ENGINE, 'model.gguf': MODEL,
                'LICENSE.llama.cpp': '94f29bbed6a22c35b992c5c6ebf0e7c92f13b836b90f36f461c9cf2f0f1d010d',
                'MODEL-CARD.md': '72c50b4ff9dfc3cc4b5ea27f7cf1ba6b0700a1213d9fd7b52eb7e31a3005bbd8'}
    descriptor = dict(format='samosa.inference.llama-package.v1', runtime_revision=REVISION,
                      kind='text', architecture='qwen35', engine='llama-server', model='model.gguf')
    provenance = dict(format='samosa.inference.text-distribution-provenance.v1', runtime_revision=REVISION,
                      runtime_source=f'https://github.com/ggml-org/llama.cpp/tree/{REVISION}',
                      model_revision=MODEL_REVISION, model_url=MODEL_URL, asset_sha256=expected,
                      conversion='Retained qualified Q4_K_M GGUF; no new download or conversion for staging',
                      license_evidence='Pinned upstream model card declares MIT; pinned repository has no separate LICENSE file',
                      namespace='Original App URL redirects to ornith-ai; immutable revision and content pin are retained',
                      build_profile='Existing static CPU runtime already qualified for text and EmbeddingGemma2',
                      minimum_os='26.0', execution_qualified_os='26.5.1', platform='macos-arm64',
                      qualification='candidate; exact installed and published-URL acceptance required')
    notice = ('Ornith1.0 9B Q4_K_M, original App catalog revision ' + MODEL_REVISION + '.\n'
              'The complete pinned upstream model card is retained, including its explicit MIT declaration.\n'
              'The card links a separate LICENSE file that is absent at that pinned revision; none is fabricated.\n'
              'Weights are acquired directly from the original immutable upstream URL, not rehosted by this package.\n'
              'The independently qualified llama.cpp CPU engine and its full MIT notice are reused unchanged.\n')
    policy = dict(format='samosa.inference.text-distribution-host-policy.v1', runtime_revision=REVISION,
                  kind='text', architecture='qwen35', engine_sha256=ENGINE, model_sha256=MODEL,
                  context_tokens=4096, redirect_hosts=['release-assets.githubusercontent.com', 'us.aws.cdn.hf.co'],
                  qualification='macOS26.5.1 arm64 execution;26.0 observed engine deployment minimum')
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected,
                     engine='llama-server', model='model.gguf', recipe='llama-gguf-package-v1', provider='ornith-text-cpu',
                     license_notice='MIT model declaration retained in pinned upstream card; MIT native runtime notice included',
                     metadata={'llama-package.json': descriptor, 'package-provenance.json': provenance, 'NOTICE.model': notice},
                     policy=policy, upstream={'model.gguf': MODEL_URL}, minimum_os='26.0')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
