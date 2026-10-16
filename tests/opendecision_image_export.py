#!/usr/bin/env python3
"""Developer-only image export; reuse the pinned reference and text export checks."""
import argparse
import importlib.metadata
import json
import os
from pathlib import Path

from opendecision_export import digest
from opendecision_reference import inventory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('model', type=Path)
    parser.add_argument('reference', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--reference-sha256', required=True)
    args = parser.parse_args()
    model, reference, output = args.model.resolve(), args.reference.resolve(), args.output.absolute()
    if output.exists() or output.is_symlink():
        raise ValueError('Export output must not already exist')
    if digest(reference) != args.reference_sha256:
        raise ValueError('Reference report pin mismatch')
    report = json.loads(reference.read_text())
    assets = json.loads(Path(__file__).with_name('opendecision_image_assets.json').read_text())
    if (report['format'] != 'samosa.inference.opendecision-image-reference.v1'
            or report['model_assets'] != assets or report['model_dtype'] != 'torch.float32'
            or report['model_class'] != 'SiglipModel'):
        raise ValueError('Unqualified image reference profile')
    inventory(model, assets['files'])
    for name, version in report['versions'].items():
        if importlib.metadata.version(name) != version:
            raise ValueError('Reference dependency changed: ' + name)
    if importlib.metadata.version('onnx') != '1.18.0':
        raise ValueError('Unqualified ONNX exporter')
    os.environ.update(HF_HUB_OFFLINE='1', TRANSFORMERS_OFFLINE='1', USE_TF='0')
    import onnx
    import torch
    from transformers import AutoModel

    torch.set_num_threads(4)
    torch.manual_seed(0)
    classifier = AutoModel.from_pretrained(str(model), local_files_only=True).cpu().eval()

    class Logits(torch.nn.Module):
        def __init__(self, inner):
            super().__init__()
            self.inner = inner

        def forward(self, input_ids, pixel_values):
            return self.inner(input_ids=input_ids, pixel_values=pixel_values).logits_per_image

    wrapper = Logits(classifier).eval()
    batches = []
    with torch.no_grad():
        for case in report['cases']:
            for batch in case['batches']:
                inputs = batch['inputs']
                pixels = inputs['pixel_values']
                path = reference.parent / pixels['file']
                if (path.parent != reference.parent or path.stat().st_size != pixels['bytes']
                        or digest(path) != pixels['sha256'] or pixels['dtype'] != 'float32_le'
                        or pixels['shape'] != [1, 3, 224, 224]):
                    raise ValueError('Pixel tensor pin/profile mismatch')
                ids = torch.tensor(inputs['input_ids'], dtype=torch.int64)
                if ids.dim() != 2 or not 1 <= ids.shape[0] <= 8 or ids.shape[1] != 64:
                    raise ValueError('Token shape exceeds image profile')
                values = torch.frombuffer(bytearray(path.read_bytes()), dtype=torch.float32).reshape(pixels['shape'])
                actual = wrapper(ids, values).float()
                expected = torch.tensor(batch['logits_per_image'], dtype=torch.float32)
                if not torch.equal(actual, expected):
                    raise ValueError('Direct model differs from reference: ' + case['name'])
                batches.append((ids, values))
                print('PASS original image tensor parity: ' + case['name'], flush=True)
    output.mkdir(mode=0o700)
    destination = output / 'model.onnx'
    print('BEGIN pinned float32 image ONNX export', flush=True)
    with torch.no_grad():
        torch.onnx.export(wrapper, batches[0], str(destination), dynamo=False,
                          input_names=['input_ids', 'pixel_values'], output_names=['logits_per_image'],
                          dynamic_axes={'input_ids': {0: 'candidates'},
                                        'logits_per_image': {1: 'candidates'}},
                          opset_version=17, do_constant_folding=True)
    onnx.checker.check_model(str(destination), full_check=True)
    inventory(model, assets['files'])
    files = {p.name: dict(bytes=p.stat().st_size, sha256=digest(p))
             for p in sorted(output.iterdir()) if p.is_file()}
    provenance = dict(format='samosa.inference.decision-image-onnx-candidate.v1',
                      qualification='candidate; native execution not yet qualified',
                      source_assets=assets, reference_sha256=args.reference_sha256,
                      dtype='float32', token_positions=64, image_shape=[1, 3, 224, 224],
                      versions={**report['versions'], 'onnx': importlib.metadata.version('onnx')},
                      files=files)
    (output / 'provenance.json').write_text(json.dumps(provenance, indent=2, sort_keys=True) + '\n')
    print('PASS pinned image ONNX export candidate: ' + str(output), flush=True)


if __name__ == '__main__':
    main()
