#!/usr/bin/env python3
"""Capture SigLIP token matrices for the existing native tokenizer replay tool."""
import argparse
import importlib.metadata
import json
import os
from pathlib import Path
from opendecision_export import digest
from opendecision_reference import inventory


def main():
    assert __debug__
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('model', type=Path)
    p.add_argument('reference', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--reference-sha256', required=True)
    args = p.parse_args()
    if digest(args.reference) != args.reference_sha256 or args.output.exists():
        raise ValueError('Reference pin mismatch or output already exists')
    report = json.loads(args.reference.read_bytes())
    assets = json.loads(Path(__file__).with_name('opendecision_image_assets.json').read_bytes())
    if report['model_assets'] != assets:
        raise ValueError('Unexpected image model profile')
    inventory(args.model, assets['files'])
    for name in ['transformers', 'tokenizers']:
        if importlib.metadata.version(name) != report['versions'][name]:
            raise ValueError('Reference dependency changed: '+name)
    os.environ.update(HF_HUB_OFFLINE='1',TRANSFORMERS_OFFLINE='1',USE_TF='0')
    from transformers import AutoProcessor
    tokenizer = AutoProcessor.from_pretrained(str(args.model),local_files_only=True).tokenizer
    initializer = args.reference.parent/'tokenizer.json'
    if digest(initializer) != report['tokenizer']['sha256'] or json.loads(tokenizer.backend_tokenizer.to_str()) != json.loads(initializer.read_bytes()):
        raise ValueError('Effective image tokenizer changed')
    cases = []
    for case in report['cases']:
        a, method = case['arguments'], case['method']
        if method == 'choice':
            texts = [description or name.replace('_',' ') for name,description in a['criteria'].items()]
        elif method == 'score':
            texts = a['criteria']
        elif method == 'noul':
            texts = [a['criteria']['true'],a['criteria']['false']] if a.get('criteria') is not None else [a['instructions']]
        elif method == 'relation':
            texts = [a['proposition'],a['contradiction']]
        else:
            raise ValueError('Unexpected reference image method')
        expected = dict(tokenizer(texts,padding='max_length',max_length=64,truncation=True,
                                  return_attention_mask=True,return_token_type_ids=True))
        assert expected['input_ids'] == case['batches'][0]['inputs']['input_ids']
        for start in range(0,len(texts),8):
            request = dict(texts=[[text] for text in texts[start:start+8]],max_length=64,padding='max_length',
                           truncation='longest_first',padding_side='right',truncation_side='right',
                           pad_token='<pad>',pad_id=0,pad_type_id=0)
            observed = {key:value[start:start+8] for key,value in expected.items()}
            name = case['name'] if len(texts)<=8 else case['name']+f'/batch-{start//8}'
            cases.append(dict(name=name,request_hex=json.dumps(request,ensure_ascii=False).encode().hex(),status=0,result=observed))
    inventory(args.model,assets['files'])
    corpus = dict(format='samosa.tokenizer-parity.v1',scope='SigLIP image-description matrices; all three fields compared',
                  reference_sha256=args.reference_sha256,initializer_sha256=report['tokenizer']['sha256'],cases=cases)
    with args.output.open('x') as destination:
        json.dump(corpus,destination,ensure_ascii=False,allow_nan=False,indent=2)
        destination.write('\n')
    print(f'PASS image tokenizer corpus: {len(cases)} complete HF matrices agree with actual model inputs')


if __name__ == '__main__':
    main()
