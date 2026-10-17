#!/usr/bin/env python3
"""Prepare qualified native text/image decisions using the shared release path.

No conversion, model download or execution-weight copy. Native installer and
existing actual public decision gates remain authoritative acceptance steps.
"""
import argparse
import json
from pathlib import Path
from distribution_candidate import digest, prepare

REVISION = '0a65512ed366e84f9f52b4f62de7ef0f879dd80e'
ENGINE_PIN = 'cc2266a25773068d462628de69a53b8e1f8ddd7c3735b5ea0ebab940bef362b4'
ORT_PIN = '5b4c4df69e6d1bc10eb0d08da525b11faf8365a0a78cc597a5cc57926330a9ca'
PROFILES = {
    'text': dict(model='15f64c4741ace31bb4130c241bdfdcd48b1d76910210d96f4ee86dacddd54398',
                 tokenizer='7caa16d3d730231fc7b9c2db4716e3e38736880bd4f2e016bb37987645e4900a',
                 card='424801aa7cc4e93e6639a504e47867d4ec537c94e2437b9fddb3b6bf80c26628',
                 license='c2cfccb812fe482101a8f04597dfc5a9991a6b2748266c47ac91b6a5aae15383',
                 reference='f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11'),
    'image': dict(model='dd3301b3310117bc09f6e8e1e10af9bab98fdc38d6c41e907ba148d1938fb73b',
                  tokenizer='9846872af4d6df792f3931d68b65394370948d3cf80fa8974743a006facd351e',
                  card='39ac3705d62af9ffa1a14675b8ccb220a75f2d81acd530e564a3b1e3dfe418d8',
                  license='cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30',
                  reference='ca1a8222ba9eb10b33648f5f1cca18b1aa61e0d493daf006dca530603fdc2ee6')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PROFILES, required=True)
    for name in ('engine', 'runtime', 'model', 'tokenizer', 'runtime-license', 'runtime-notices', 'rust-notices',
                 'rust-inventory', 'sidecar-license', 'model-license', 'model-card', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    profile = PROFILES[args.profile]
    module = Path(__file__).resolve().parent.parent
    source_assets = module / ('tests/opendecision_' + args.profile + '_assets.json')
    paths = {'assets/decision-engine': args.engine, 'assets/libonnxruntime.1.27.0.dylib': args.runtime,
             'assets/model.onnx': args.model, 'assets/tokenizer.json': args.tokenizer,
             'LICENSE.Samosa-OpenDecision': args.sidecar_license, 'LICENSE.model': args.model_license,
             'LICENSE.onnxruntime': args.runtime_license, 'ORT-THIRD-PARTY-NOTICES.txt': args.runtime_notices,
             'RUST-NOTICES.txt': args.rust_notices, 'RUST-NOTICES.json': args.rust_inventory,
             'Cargo.lock': module / 'engines/decision/tokenizer/Cargo.lock', 'MODEL-CARD.md': args.model_card}
    expected = {'assets/decision-engine': ENGINE_PIN, 'assets/libonnxruntime.1.27.0.dylib': ORT_PIN,
                'assets/model.onnx': profile['model'], 'assets/tokenizer.json': profile['tokenizer'],
                'LICENSE.Samosa-OpenDecision': 'c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4',
                'LICENSE.onnxruntime': '2f07c72751aed99790b8a4869cf2311df85a860b22ded05fa22803587a48922c',
                'ORT-THIRD-PARTY-NOTICES.txt': '0e07b95f3a8d6230037707c5c4a2b554d12c4cb67369669ac255635528ffcee2',
                'RUST-NOTICES.txt': '9d7263d9a8fcfa48ba5811b939fc4ec5221311828c1568de63e61fd95ea287a7',
                'RUST-NOTICES.json': '83e3f96d1c0eeefae2daf68830bfa35385f49a10c05748a8c5ba2fad0c1d6861',
                'Cargo.lock': '0acd561bb716bf51a086ae8afcb96bacd646e49b5a47caee904f49b7dc997d4f',
                'LICENSE.model': profile['license'], 'MODEL-CARD.md': profile['card']}
    descriptor = dict(format='samosa.inference.decision-package.v1', profile=args.profile,
                      protocol='opendecision-framed-v1', assets='assets', runtime_revision=REVISION)
    provenance = dict(format='samosa.inference.decision-distribution-provenance.v1', profile=args.profile,
                      worker_revision=REVISION, reference_revision='cbd0b50db42bba6975f342de1789fb3c8f2dc670',
                      ort_version='1.27.0', source_assets_sha256=digest(source_assets), asset_sha256=expected,
                      model_conversion='Previously qualified float32 ONNX export; no new conversion, quantization or retraining',
                      tokenizer_conversion='Previously qualified prepared native tokenizer from pinned source assets',
                      reference_sha256=profile['reference'], minimum_os='26.0', execution_qualified_os='26.5.1',
                      platform='macos-arm64', qualification='distribution candidate; exact installed package acceptance required')
    notice = ('OpenDecision native ' + args.profile + ' decisions; original reference revision cbd0b50db42bba6975f342de1789fb3c8f2dc670.\n'
              'The Samosa sidecar and OpenDecision reference use Apache-2.0; full license included.\n'
              'The original model card, licensing and training-data notes are preserved in MODEL-CARD.md.\n'
              'Modified deployment files: model exported to float32 ONNX and tokenizer prepared for native execution.\n'
              'These already-qualified converted files are reused without new conversion or retraining.\n'
              'ONNX Runtime MIT license and upstream third-party notices are included.\n'
              'Locked Rust dependency source notices include vendored COPYING/LICENSE/NOTICE files.\n'
              'The image implementation consumes prepared RGB/RGBA; file decoding belongs to Sources.\n')
    policy = dict(format='samosa.inference.decision-distribution-host-policy.v1', profile=args.profile,
                  runtime_revision=REVISION, engine_sha256=ENGINE_PIN, runtime_sha256=ORT_PIN,
                  model_sha256=profile['model'], tokenizer_sha256=profile['tokenizer'],
                  reference_sha256=profile['reference'], redirect_hosts=['release-assets.githubusercontent.com'],
                  qualification='macOS26.5.1 arm64 execution;26.0 observed worker deployment minimum')
    license_notice = ('Upstream model card declares MIT and preserves training-data license notes; ' if args.profile == 'text' else 'Apache-2.0 model; ')
    license_notice += 'Apache-2.0 native/reference; MIT ORT and complete runtime/tokenizer dependency notices included'
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected,
                     engine='assets/decision-engine', model='assets/model.onnx', recipe='opendecision-package-v1',
                     provider='opendecision-' + args.profile + '-cpu', license_notice=license_notice,
                     metadata={'decision-package.json': descriptor, 'package-provenance.json': provenance,
                               'SOURCE-ASSETS.json': json.loads(source_assets.read_text()), 'NOTICE.model': notice},
                     policy=policy, minimum_os='26.0',
                     roles={'assets/libonnxruntime.1.27.0.dylib': 'library', 'assets/tokenizer.json': 'data'})
    print(json.dumps(result))


if __name__ == '__main__':
    main()
