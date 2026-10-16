#!/usr/bin/env python3
"""QA only: pin the existing OpenDecision image computation, without copying weights."""
import argparse
import hashlib
import importlib.metadata
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
from opendecision_reference import REFERENCE_REVISION, finite, inventory


def main():
    assert __debug__
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('repository', type=Path)
    parser.add_argument('model', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--extended', action='store_true', help='Qualify batch boundaries, capacity, relation categories and pixel bounds')
    args = parser.parse_args()
    repository, model, output = args.repository.resolve(), args.model.resolve(), args.output.absolute()
    assets = json.loads(Path(__file__).with_name('opendecision_image_assets.json').read_bytes())
    inventory(model, assets['files'])
    revision = subprocess.check_output(['git', '-C', str(repository), 'rev-parse', 'HEAD'], text=True).strip()
    if revision != REFERENCE_REVISION:
        raise ValueError('Unqualified OpenDecision reference revision')
    output.mkdir(mode=0o700, parents=False, exist_ok=False)
    with tempfile.TemporaryDirectory(prefix='samosa-decision-image-source-') as temporary:
        snapshot = Path(temporary)
        archive = snapshot/'source.tar'
        subprocess.run(['git','-C',str(repository),'archive','--format=tar','--output='+str(archive),
                        'HEAD','src/opendecision','LICENSE'],check=True)
        with tarfile.open(archive) as source:
            source.extractall(snapshot,filter='data')
        sys.path.insert(0,str(snapshot/'src'))
        os.environ.update(HF_HUB_OFFLINE='1',TRANSFORMERS_OFFLINE='1',USE_TF='0')
        import torch
        from PIL import Image
        from opendecision import ImageDecisionEngine
        torch.set_num_threads(4)
        torch.manual_seed(0)
        engine = ImageDecisionEngine(model=str(model),device='cpu')
        tokenizer_bytes = engine.processor.tokenizer.backend_tokenizer.to_str().encode('utf-8')
        (output/'tokenizer.json').write_bytes(tokenizer_bytes)
        def blob(data, extension):
            checksum = hashlib.sha256(data).hexdigest()
            name = checksum+extension
            path = output/name
            if path.exists():
                assert path.read_bytes() == data
            else:
                with path.open('xb') as destination:
                    destination.write(data)
            return dict(file=name,bytes=len(data),sha256=checksum)
        images = {}
        for name, image in [
            ('red',Image.new('RGB',(224,224),(255,0,0))),
            ('blue',Image.new('RGB',(224,224),(0,0,255))),
            ('small',Image.new('RGB',(37,19),(0,0,255))),
            ('pattern',Image.frombytes('RGB',(451,173),bytes(
                channel for y in range(173) for x in range(451)
                for channel in ((x*13+y*3)%256,(x*5+y*17)%256,(x*7+y*11)%256)))),
        ]:
            images[name] = (image, dict(width=image.width,height=image.height,format='rgb8',**blob(image.tobytes(),'.rgb')))
        batches = []
        def before(module, positional, keyword):
            assert not positional
            inputs = {}
            for name,value in keyword.items():
                if not isinstance(value,torch.Tensor):
                    continue
                value = value.detach().cpu().contiguous()
                if name == 'pixel_values':
                    assert value.dtype == torch.float32 and list(value.shape) == [1,3,224,224]
                    inputs[name] = dict(dtype='float32_le',shape=list(value.shape),**blob(value.numpy().astype('<f4',copy=False).tobytes(),'.f32'))
                else:
                    inputs[name] = value.tolist()
            assert 'input_ids' in inputs and 'pixel_values' in inputs
            assert all(len(row)==64 for row in inputs['input_ids'])
            batches.append(dict(inputs=inputs))
        def after(module, positional, result):
            batches[-1]['logits_per_image'] = result.logits_per_image.detach().float().cpu().tolist()
        pre = engine.model.register_forward_pre_hook(before,with_kwargs=True)
        post = engine.model.register_forward_hook(after)
        colors = {'red':'An entirely red square.','blue':'An entirely blue square.'}
        cases = [
            ('red_choice','choice','red',dict(instructions='Which color?',criteria=colors)),
            ('unused_choice_instructions','choice','red',dict(instructions='Unrelated instructions must not condition image matching.',criteria=colors)),
            ('blue_choice','choice','blue',dict(instructions='Which color?',criteria=colors)),
            ('description_collision','choice','red',dict(instructions='unused',criteria={'first_red':colors['red'],'second_red':colors['red'],'blue':colors['blue']})),
            ('predicate','noul','red',dict(instructions=colors['red'])),
            ('explicit_binary','noul','red',dict(instructions='unused',criteria={'true':colors['red'],'false':colors['blue']})),
            ('ordered_score','score','blue',dict(instructions='unused',criteria=[colors['red'],'An entirely green square.',colors['blue']])),
            ('relation','relation','red',dict(proposition=colors['red'],contradiction=colors['blue'],threshold=0.5)),
            ('non_square_resize','choice','pattern',dict(instructions='unused',criteria={'pattern':'A colorful geometric pattern.','sky':'A clear blue sky.'})),
            ('upscale','choice','small',dict(instructions='Which color?',criteria=colors)),
            ('unicode_truncation','noul','red',dict(instructions=('Café 猫 invoice € ')*100)),
        ]
        if args.extended:
            for name, width, height in [('full',512,512),('narrow',1,512)]:
                image = Image.frombytes('RGB',(width,height),bytes(
                    channel for y in range(height) for x in range(width)
                    for channel in ((x*13+y*3)%256,(x*5+y*17)%256,(x*7+y*11)%256)))
                images[name] = (image, dict(width=width,height=height,format='rgb8',**blob(image.tobytes(),'.rgb')))
            def many(count):
                return {**{f'blue_{i}':f'An entirely blue square, example {i}.' for i in range(count-1)},
                        'tail_red':colors['red']}
            cases += [
                ('fallback_choice','choice','red',dict(instructions='unused',criteria={'An_entirely_red_square.':None,'blue':colors['blue']})),
                ('cross_batch_choice','choice','red',dict(instructions='unused',criteria=many(9))),
                ('capacity_choice','choice','red',dict(instructions='unused',criteria=many(128))),
                ('capacity_score','score','red',dict(instructions='unused',criteria=list(many(128).values()))),
                ('relation_supports','relation','red',dict(proposition=colors['red'],contradiction=colors['blue'],threshold=0.2)),
                ('relation_contradicts','relation','red',dict(proposition=colors['blue'],contradiction=colors['red'],threshold=0.2)),
                ('relation_conflicted','relation','red',dict(proposition=colors['red'],contradiction=colors['blue'],threshold=0.0005)),
                ('full_frame','choice','full',dict(instructions='unused',criteria={'pattern':'A colorful geometric pattern.','sky':'A clear blue sky.'})),
                ('narrow_frame','choice','narrow',dict(instructions='unused',criteria=colors)),
            ]
        captured = []
        try:
            for name,method,key,arguments in cases:
                print('BEGIN image reference '+name,flush=True)
                batches.clear()
                result = getattr(engine,method)(state=images[key][0],**arguments)
                assert len(batches)==1 and 'logits_per_image' in batches[0]
                finite(result)
                finite(batches)
                if name=='red_choice':
                    assert result['choice']=='red'
                if name in {'blue_choice','upscale'}:
                    assert result['choice']=='blue'
                if name=='unused_choice_instructions':
                    assert result==captured[0]['result'] and batches==captured[0]['batches']
                if name in {'cross_batch_choice','capacity_choice'}:
                    assert result['choice']=='tail_red'
                if name=='fallback_choice':
                    assert result['choice']=='An_entirely_red_square.'
                if name in {'relation_supports','relation_contradicts','relation_conflicted'}:
                    assert result['relation']==name.removeprefix('relation_')
                captured.append(dict(name=name,method=method,state=images[key][1],arguments=arguments,result=result,batches=list(batches)))
                print('PASS image reference '+name,flush=True)
        finally:
            pre.remove()
            post.remove()
        inventory(model,assets['files'])
        report = dict(format='samosa.inference.opendecision-image-reference.v1',reference_revision=revision,
                      model_assets=assets,device='cpu',threads=4,
                      processor=type(engine.processor).__name__,image_processor=type(engine.processor.image_processor).__name__,
                      model_class=type(engine.model).__name__,model_dtype=str(engine.model.dtype),
                      versions={name:importlib.metadata.version(name) for name in ['torch','transformers','tokenizers','Pillow']},
                      tokenizer=dict(bytes=len(tokenizer_bytes),sha256=hashlib.sha256(tokenizer_bytes).hexdigest()),cases=captured)
        with (output/'reference.json').open('x') as destination:
            json.dump(report,destination,ensure_ascii=False,allow_nan=False,indent=2)
            destination.write('\n')
            destination.flush()
            os.fsync(destination.fileno())
        print(f'PASS pinned image reference: {len(captured)} actual-model cases; processor {report["image_processor"]}; tokenizer {len(tokenizer_bytes)} bytes',flush=True)


if __name__=='__main__':
    main()
