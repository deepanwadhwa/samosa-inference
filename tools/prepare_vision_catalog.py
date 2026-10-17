#!/usr/bin/env python3
"""Prepare the qualified native VisionPsy distribution without copying weights."""
import argparse
import json
from pathlib import Path
from distribution_candidate import prepare

MODEL_REVISION = 'ca7a1bad19a08fe8e474bee24275a2e377bbabba'
SOURCE_CARD_REVISION = 'a779cb695f7627c36ded60a82a8c3cc73f03fa24'
PINS = {
    'samosa-visionpsy': '8547c6c629b28b48753ef88e114dbc1b6250be82edf88b1a982855a101f8b346',
    'mlx.metallib': '846add228d562bb224b11f18c4655b75a05f3336e543654ce49943577e26cd5a',
    'model/config.json': 'f25b85a4aef0df1867e0221310b86d87058de9ea6fadc50e993a2de5b08851a8',
    'model/tokenizer.json': '9da3f64c3f7b940520c48fc81dc203149080934d4c7f52201883d24d0cd390a8',
    'model/model.safetensors': '0cc93c1027a58aec26c4350115b5eca02a836aba06277fe700de854ff078572f',
    'LICENSE.MLX': 'ccfab7ccb2ea306f71531c8ca77bb55507606cd90768b1e32b8b52ab5b48cf01',
    'LICENSE.fmt': '07580f2a3b35709ce703d523f447b242f6dfec7582a8c0df102c7fa2849375f8',
    'LICENSE.metal-cpp': 'f4e92c7fe2aa066e294c0da9d08683dc9fb741ee6e6debbfad99a0c96e07c1f2',
    'LICENSE.nlohmann-json': '86b998c792894ccb911a1cb7994f7a9652894e7a094c0b5e45be2f553f45cf14',
    'LICENSE.model': 'c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4',
    'LICENSE.Samosa': 'c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4',
    'MODEL-CARD.md': '108204914183f8071e0b3aae7d12df124d37c374894371a9c7be4c8e4e47a3e4',
    'SOURCE-MODEL-CARD.md': '20797961c5977af60c77701e939e343cd7c29f769983ceed6cb983c26cb53690'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'model', 'model-license', 'model-card', 'source-model-card', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    module = Path(__file__).resolve().parent.parent
    paths = {name: args.runtime / 'bin' / name for name in ('samosa-visionpsy', 'mlx.metallib')}
    paths.update({name: args.runtime / name for name in PINS if name.startswith('LICENSE.') and name not in ('LICENSE.model', 'LICENSE.Samosa')})
    paths.update({name: args.model / Path(name).name for name in PINS if name.startswith('model/')})
    paths.update({'LICENSE.model': args.model_license, 'LICENSE.Samosa': module / 'LICENSE',
                  'MODEL-CARD.md': args.model_card, 'SOURCE-MODEL-CARD.md': args.source_model_card})
    paths = {name: path.resolve() for name, path in paths.items()}
    stb = (module / 'engines/stb_image.h').read_text()
    marker = '/*\n------------------------------------------------------------------------------\nThis software is available under 2 licenses'
    assert stb.count(marker) == 1
    stb_notice = stb[stb.index(marker):]
    descriptor = dict(format='samosa.inference.vision-package.v1', profile='visionpsy-v1',
                      engine='samosa-visionpsy', model_directory='model')
    policy = dict(format='samosa.inference.vision-distribution-host-policy.v1', profile='visionpsy-v1',
                  engine_sha256=PINS['samosa-visionpsy'], config_sha256=PINS['model/config.json'],
                  tokenizer_sha256=PINS['model/tokenizer.json'], weights_sha256=PINS['model/model.safetensors'],
                  kernels_sha256=PINS['mlx.metallib'], redirect_hosts=['release-assets.githubusercontent.com', 'us.aws.cdn.hf.co'],
                  qualification='macOS26.5.1 arm64 execution;26.5.1 observed independent engine deployment minimum')
    model_url = f'https://huggingface.co/KaedeTai/VisionPsy-Nano-460M-MLX/resolve/{MODEL_REVISION}/model.safetensors'
    provenance = dict(format='samosa.inference.vision-distribution-provenance.v1', model_revision=MODEL_REVISION,
                      model_url=model_url, source_model='qvac/VisionPsy-Nano-460M', source_card_revision=SOURCE_CARD_REVISION,
                      mlx_revision='7a1d4f5c12ac82f4b4d0a6e71538d89ca0605247',
                      fmt_revision='407c905e45ad75fc29bf0f9bb7c5c2fd3475976f', asset_sha256=PINS,
                      build='existing independent native engine; static MLX; colocated kernels; disabled fallback',
                      minimum_os='26.5.1', execution_qualified_os='26.5.1', platform='macos-arm64',
                      source_card_scope='Pinned source card attribution; not a claim that the conversion binds this original weight revision',
                      qualification='candidate; exact installed and published-URL acceptance required')
    notice = ('VisionPsy Nano460M MLX BF16: retained converted model, unchanged.\n'
              'Model source and converted cards declare Apache2.0; full cards and license are included.\n'
              'Existing independent native engine and colocated MLX kernels; dependency licenses and stb_image dual notice included.\n'
              'Actual quality evidence is the existing bounded heading-recognition fixture, not general visual accuracy.\n')
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=PINS,
                     engine='samosa-visionpsy', model='model/model.safetensors', recipe='visionpsy-package-v1',
                     provider='visionpsy-native', license_notice='Apache-2.0 model and engine; complete dependency licenses and model cards included',
                     metadata={'vision-package.json': descriptor, 'package-provenance.json': provenance,
                               'NOTICE.visionpsy': notice, 'LICENSE.stb_image': stb_notice}, policy=policy,
                     roles={'mlx.metallib': 'data', 'model/tokenizer.json': 'data'}, upstream={'model/model.safetensors': model_url}, minimum_os='26.5.1')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
