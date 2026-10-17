#!/usr/bin/env python3
"""Prepare retained Sherpa speech packages with the existing native manifest.

Release tooling only. Removes unreferenced runtime/sample files from a new
candidate; retained sources are read-only and model/library bytes are not copied.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
from distribution_candidate import digest, prepare, write

PINS = {'pocket': '4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16',
        'kokoro': '8d3ce27844a7147201dbb0520d7407d861de52b9c729e7140175484a7a848aa7'}
SHERPA = '142807252687d81b40d6315f23470a1512a00de3'
ESPEAK = 'ed530aa113046142eb5115cf2fc9157854d0ffe1'


def encoded(value):
    return (value if isinstance(value, str) else json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n').encode()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PINS, required=True)
    for name in ('package', 'model-root', 'notices', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--release-url', required=True)
    args = parser.parse_args()
    original = args.package / 'manifest.json'
    assert digest(original) == PINS[args.profile]
    native = json.loads(original.read_text())
    assert native['kind'] == args.profile and native['abi_version'] == '1.13.4'
    paths, expected, roles = {}, {}, {}
    keep = []
    omitted = []
    for file in native['files']:
        name = file['name']
        if name.startswith('model/test_wavs/') or name in (
                'runtime/lib/libonnxruntime.dylib', 'runtime/lib/libsherpa-onnx-cxx-api.dylib'):
            omitted.append(name)
            continue
        source = args.package / name
        if not source.exists() and name.startswith('model/'):
            source = args.model_root / name.removeprefix('model/')
        source = source.resolve()
        assert source.is_file() and source.stat().st_size == file['bytes']
        paths[name], expected[name] = source, file['sha256']
        roles[name] = ('library' if file['role'] in ('library', 'dependency') else
                       'metadata' if file['role'] == 'metadata' else
                       'model' if file['role'].startswith('pocket_') and name.endswith('.onnx') or file['role'] == 'kokoro_model' else 'data')
        keep.append(copy.deepcopy(file))
    notice_inputs = {
        'SPEECH-SOURCE-NOTICES.txt': '70d02ac824f60eabf1e59ee738779fd011fc2f07e10e9373a90226f5f57c7e63',
        'SPEECH-SOURCE-NOTICES.json': 'f3f3cd93fa6cdbebb4e0a8228247276c970d22d6b3618c6aafc113afe1e824d5',
        'LICENSE.Sherpa': 'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30',
        'LICENSE.onnxruntime': '2f07c72751aed99790b8a4869cf2311df85a860b22ded05fa22803587a48922c',
        'ORT-THIRD-PARTY-NOTICES.txt': '0e07b95f3a8d6230037707c5c4a2b554d12c4cb67369669ac255635528ffcee2',
        'COPYING.espeak': '8ceb4b9ee5adedde47b31e975c1d90c73ad27b6b165a1dcd80c7c545eb65b903',
        'COPYING.espeak-BSD2': 'a8b0c1c7d7af6c0415c068be8cf373799d1a42b0707b79dc4ccfcaf593cd902d',
        'COPYING.espeak-UCD': 'be029c50df83105a810e391778b6edcb522d6cb4b4e142332aca54437a499bf7',
        'COPYING.espeak-APACHE': 'cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30'}
    if args.profile == 'pocket':
        notice_inputs['VOICE-SOURCE.md'] = '47c610f38e0e0bfb4b353a2afa84a5277b748b1b808d5f77fb8ef74fb836689b'
    for name, pin in notice_inputs.items():
        source = (args.notices / name).resolve()
        assert digest(source) == pin
        paths[name], expected[name], roles[name] = source, pin, 'metadata'
        keep.append(dict(name=name, role='metadata', bytes=source.stat().st_size, sha256=pin))
    sources = dict(format='samosa.inference.speech-corresponding-sources.v1',
                   sherpa=dict(revision=SHERPA, url=f'https://github.com/k2-fsa/sherpa-onnx/archive/{SHERPA}.tar.gz'),
                   espeak=dict(revision=ESPEAK, url=f'https://github.com/csukuangfj/espeak-ng/archive/{ESPEAK}.zip',
                               sha256='e4e262cbe34f7fe21f91f1ba3397f2728e1f30eafbae7853f2b753a9ed13f0dd'),
                   ort=dict(version='1.27.0', url='https://github.com/microsoft/onnxruntime/archive/refs/tags/v1.27.0.tar.gz'),
                   build_instructions='Sherpa pinned CMake files include dependency sources/build rules; cmake/espeak-ng-for-piper.cmake pins the eSpeak source and checksum.',
                   distribution='Runtime includes GPL-3.0 eSpeak components; COPYING and source locations accompany the object code.',
                   loader_changes='Retained qualified Mach-O files; dependency imports resolve to colocated versioned ORT. No source changes or new runtime build in this preparation.')
    provenance = dict(format='samosa.inference.speech-distribution-provenance.v1', kind=args.profile,
                      original_manifest_sha256=PINS[args.profile], sherpa_revision=SHERPA, abi_version='1.13.4',
                      asset_sha256=expected.copy(), omitted_unreferenced_files=omitted,
                      minimum_os='15.5', execution_qualified_os='26.5.1', platform='macos-arm64',
                      qualification='candidate; requires exact installed synthesis and published-URL qualification')
    notice = ('Native ' + args.profile + ' speech using Sherpa-ONNX1.13.4 and ONNX Runtime1.27.0.\n'
              'Runtime includes GPL-3.0 eSpeak components; source locations/build rules are in CORRESPONDING-SOURCES.json.\n'
              'Original model LICENSE/README are preserved. Pocket README contains a non-commercial caution while its bundled LICENSE says CC BY4.0; neither is silently rewritten.\n'
              'Pocket voice-zero references are CC0; the complete pinned voice source card is included for that profile.\n'
              'No model/voice bytes are modified. Unused C++/unversioned runtime libraries and unrelated sample WAVs are omitted.\n')
    metadata = {'CORRESPONDING-SOURCES.json': sources, 'package-provenance.json': provenance, 'NOTICE.speech': notice}
    for name, value in metadata.items():
        body = encoded(value)
        keep.append(dict(name=name, role='metadata', bytes=len(body), sha256=hashlib.sha256(body).hexdigest()))
    native['files'] = keep
    manifest_pin = hashlib.sha256(encoded(native)).hexdigest()
    metadata['manifest.json'] = native
    policy = dict(format='samosa.inference.speech-distribution-host-policy.v1', kind=args.profile,
                  abi_version='1.13.4', manifest_sha256=manifest_pin, sherpa_revision=SHERPA,
                  library_sha256='c8d9e266ab5de81372e9d1a275b4d42c4bace1e55e7359ab1227de5914fd21e1',
                  redirect_hosts=['release-assets.githubusercontent.com'],
                  qualification='macOS26.5.1 arm64 execution pending;15.5 observed library deployment maximum')
    model = 'model/lm_main.int8.onnx' if args.profile == 'pocket' else 'model/model.int8.onnx'
    result = prepare(output=args.output, release_url=args.release_url, paths=paths, expected=expected, engine=None,
                     model=model, recipe='sherpa-speech-package-v1', provider=args.profile + '-speech-cpu',
                     license_notice='Original model/voice licenses and README retained; Apache/MIT and GPL-3.0 runtime components with notices and corresponding-source locations',
                     metadata=metadata, policy=policy, roles=roles, minimum_os='15.5')
    module = Path(__file__).resolve().parent.parent
    if args.profile == 'kokoro':
        profile = json.loads((module / 'tests/profiles/kokoro-macos-arm64-v1.json').read_text())
        profile['manifest_sha256'] = manifest_pin
        write(args.output / 'quality-profile.json', profile)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
