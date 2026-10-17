#!/usr/bin/env python3
"""Prepare the already-qualified native Molmo2 shards without weight copies."""
import argparse
import json
from pathlib import Path
from distribution_candidate import digest, prepare

ENGINE = '9a6a718e31d73c006be15f9266996286214f7f492c380d1b09097006d61b87af'
KERNELS = '846add228d562bb224b11f18c4655b75a05f3336e543654ce49943577e26cd5a'
MANIFEST = 'a07230dd952e97969921223328e7812020bb58711728450cdfb2205e3a15b770'
REVISION = '042abfa7a38879a376cec03d949eff0aefaa0600'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('engine', 'kernels', 'licenses', 'model', 'model-card', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    manifest = args.model / 'manifest.json'
    assert digest(manifest) == MANIFEST
    native = json.loads(manifest.read_text())
    assert native['format'] == 'samosa.molmo2.mlx.v1' and native['upstream_revision'] == REVISION
    paths = {'samosa-molmo2': args.engine, 'mlx.metallib': args.kernels, 'model/manifest.json': manifest}
    expected = {'samosa-molmo2': ENGINE, 'mlx.metallib': KERNELS, 'model/manifest.json': MANIFEST}
    roles = {'mlx.metallib': 'data', 'model/manifest.json': 'metadata'}
    for file in native['files']:
        leaf = file['name']
        assert Path(leaf).name == leaf and not leaf.startswith('.')
        name = 'model/' + leaf
        assert name not in paths and file['role'] in ('weights', 'tokenizer', 'metadata')
        source = args.model / leaf
        assert source.stat().st_size == file['bytes']
        paths[name], expected[name] = source, file['sha256']
        roles[name] = {'weights': 'model', 'tokenizer': 'data', 'metadata': 'metadata'}[file['role']]
    licenses = {'LICENSE.MLX': 'ccfab7ccb2ea306f71531c8ca77bb55507606cd90768b1e32b8b52ab5b48cf01',
                'LICENSE.fmt': '07580f2a3b35709ce703d523f447b242f6dfec7582a8c0df102c7fa2849375f8',
                'LICENSE.metal-cpp': 'f4e92c7fe2aa066e294c0da9d08683dc9fb741ee6e6debbfad99a0c96e07c1f2',
                'LICENSE.nlohmann-json': '86b998c792894ccb911a1cb7994f7a9652894e7a094c0b5e45be2f553f45cf14'}
    for name, pin in licenses.items():
        paths[name], expected[name] = args.licenses / name, pin
    module = Path(__file__).resolve().parent.parent
    paths.update({'LICENSE.Apache-2.0': module / 'LICENSE', 'MODEL-CARD.md': args.model_card})
    expected.update({'LICENSE.Apache-2.0': 'c71d239df91726fc519c6eb72d318ec65820627232b2f796219e87dcf35d0ab4',
                     'MODEL-CARD.md': 'cf666804da6505e06eab884ad45e57c23e1107347d0fedd64148798a5fbf844b'})
    paths = {name: path.resolve() for name, path in paths.items()}
    header = (module / 'engines/stb_image.h').read_text()
    marker = '/*\n------------------------------------------------------------------------------\nThis software is available under 2 licenses'
    assert header.count(marker) == 1
    descriptor = dict(format='samosa.inference.molmo-package.v1', profile='molmo2-4b-mlx-q4-v1',
                      engine='samosa-molmo2', model_directory='model')
    policy = dict(format='samosa.inference.molmo-distribution-host-policy.v1', engine_sha256=ENGINE,
                  kernels_sha256=KERNELS, manifest_sha256=MANIFEST,
                  redirect_hosts=['release-assets.githubusercontent.com'],
                  qualification='macOS26.5.1 arm64 execution;26.5.1 observed independent engine deployment minimum')
    provenance = dict(format='samosa.inference.molmo-distribution-provenance.v1', upstream_revision=REVISION,
                      upstream_source=f'https://huggingface.co/allenai/Molmo2-4B/tree/{REVISION}',
                      native_manifest_sha256=MANIFEST, asset_sha256=expected,
                      conversion='Existing native molmo2-pack affine Q4/group64 and BF16 package; unchanged retained shards, no new conversion',
                      mlx_revision='7a1d4f5c12ac82f4b4d0a6e71538d89ca0605247',
                      fmt_revision='407c905e45ad75fc29bf0f9bb7c5c2fd3475976f',
                      estimated_resident_bytes=native['estimated_resident_bytes'], minimum_os='26.5.1',
                      execution_qualified_os='26.5.1', qualification='candidate; exact installed and published-URL acceptance required')
    notice = ('Molmo2 4B by Allen Institute for AI; original pinned model card declares Apache2.0.\n'
              'Full original model card, standard Apache2.0 terms and native dependency licenses are included.\n'
              'The native PROVENANCE.md and manifest retain the existing Q4/group64 conversion and upstream revision.\n'
              'No new model conversion or shard copies are performed. Existing native engine and colocated MLX kernels are reused.\n')
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected,
                     engine='samosa-molmo2', model='model/weights-00001.safetensors', recipe='molmo2-mlx-package-v1',
                     provider='molmo2-native', license_notice='Apache-2.0 model and engine; full model card, conversion provenance and dependency notices included',
                     metadata={'molmo-package.json': descriptor, 'package-provenance.json': provenance,
                               'NOTICE.molmo2': notice, 'LICENSE.stb_image': header[header.index(marker):]},
                     roles=roles, policy=policy, minimum_os='26.5.1')
    print(json.dumps(result))


if __name__ == '__main__':
    main()
