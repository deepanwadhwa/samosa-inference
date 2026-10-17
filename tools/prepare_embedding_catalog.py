#!/usr/bin/env python3
"""Prepare the existing EmbeddingGemma2 CPU engine/model without weight copies."""
import argparse
import json
from pathlib import Path
from distribution_candidate import prepare

REVISION = '4fbc76dec51d0add466f0210855c0596589b60d4'
ENGINE_PIN = '958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97'
MODEL_PIN = '2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135'
MODEL_REVISION = 'bfcd298762cc34d0357ece5ebdd31791a3a374d8'
SOURCE_REVISION = '914f7f89142e33e77833254d9c9b90c3cef7303b'
MODEL_URL = f'https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/resolve/{MODEL_REVISION}/embeddinggemma-2-Q8_0.gguf'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'model', 'runtime-license', 'model-license', 'model-card', 'source-model-card', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    paths = {'llama-server': args.engine, 'model.gguf': args.model, 'LICENSE.llama.cpp': args.runtime_license,
             'LICENSE.model': args.model_license, 'MODEL-CARD.md': args.model_card, 'SOURCE-MODEL-CARD.md': args.source_model_card}
    expected = {'llama-server': ENGINE_PIN, 'model.gguf': MODEL_PIN,
                'LICENSE.llama.cpp': '94f29bbed6a22c35b992c5c6ebf0e7c92f13b836b90f36f461c9cf2f0f1d010d',
                'LICENSE.model': 'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30',
                'MODEL-CARD.md': '1e3a35cf706dbcecce8f20ef756c2dd6c14dfbe9a5d93e35edc19e683fd0bc60',
                'SOURCE-MODEL-CARD.md': 'b677a0ee2818c36f091b65fd3d9b4baea9f04d53b02b49cae916c74bf9682037'}
    descriptor = dict(format='samosa.inference.llama-package.v1', runtime_revision=REVISION,
                      kind='embedding', architecture='gemma-embedding2', engine='llama-server', model='model.gguf')
    recipe = dict(model_id='google/embeddinggemma-2-Q8_0', model_revision=REVISION,
                  model_sha256=MODEL_PIN, engine_sha256=ENGINE_PIN, tokenizer_hash=MODEL_PIN,
                  query_prefix='task: search result | query: ', document_prefix='',
                  recipe='embeddinggemma2-q8-mean-l2-f32-v1', dimensions=768)
    provenance = dict(format='samosa.inference.embedding-distribution-provenance.v1', runtime_revision=REVISION,
                      runtime_source=f'https://github.com/ggml-org/llama.cpp/tree/{REVISION}',
                      model_revision=MODEL_REVISION, model_url=MODEL_URL, source_model_revision=SOURCE_REVISION,
                      asset_sha256=expected, minimum_os='26.0', execution_qualified_os='26.5.1', platform='macos-arm64',
                      build_profile='static CPU; Metal/Accelerate/BLAS/OpenMP/OpenSSL disabled',
                      scope='text embeddings only; no media projector or multimodal embedding claim',
                      qualification='distribution candidate; requires exact installed package acceptance')
    notice = ('EmbeddingGemma 2 by Google DeepMind, converted to GGUF Q8_0 by ggml-org.\n'
              'Both pinned upstream model cards declare Apache-2.0 and are included unchanged.\n'
              'Converted repository revision: ' + MODEL_REVISION + '\n'
              'Original Google model revision: ' + SOURCE_REVISION + '\n'
              'The converter repository .src_sha binds the original model revision.\n'
              'Samosa uses the existing converted text model unchanged; no new quantization.\n'
              'Native llama.cpp engine is MIT licensed; full notice is included.\n'
              'The package does not contain vision/audio projectors or qualify media embeddings.\n')
    policy = dict(format='samosa.inference.embedding-distribution-host-policy.v1', runtime_revision=REVISION,
                  kind='embedding', architecture='gemma-embedding2', engine_sha256=ENGINE_PIN, model_sha256=MODEL_PIN,
                  context_tokens=8192, representation='embeddinggemma2-q8-mean-l2-f32-v1', dimensions=768,
                  redirect_hosts=['release-assets.githubusercontent.com', 'us.aws.cdn.hf.co'],
                  qualification='macOS26.5.1 arm64 execution;26.0 observed engine deployment minimum')
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected,
                     engine='llama-server', model='model.gguf', recipe='llama-gguf-package-v1', provider='embeddinggemma2-cpu',
                     license_notice='Apache-2.0 model; MIT native runtime; full pinned upstream cards and notices included',
                     metadata={'llama-package.json': descriptor, 'package-provenance.json': provenance,
                               'embedding-recipe.json': recipe, 'NOTICE.model': notice}, policy=policy,
                     upstream={'model.gguf': MODEL_URL}, minimum_os='26.0')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
