#!/usr/bin/env python3
"""QA only: compare the native C ABI with the pinned HF tokenizer wrapper."""
import argparse
import ctypes as c
import hashlib
import importlib.metadata
import json
from pathlib import Path
import random
import sys


class Output(c.Structure):
    _fields_ = [("data", c.c_void_p), ("length", c.c_size_t), ("owner", c.c_void_p)]


def main():
    if not __debug__ or sys.flags.optimize:
        raise RuntimeError("Run parity checks without Python optimization")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("model", type=Path)
    parser.add_argument("initializer", type=Path)
    parser.add_argument("--corpus-output", type=Path,
                        help="Exclusively write checked requests/results for cross-platform replay")
    args = parser.parse_args()
    if importlib.metadata.version("tokenizers") != "0.23.2":
        raise ValueError("Reference requires tokenizers 0.23.2")
    if importlib.metadata.version("transformers") != "5.17.0":
        raise ValueError("Reference requires transformers 5.17.0")
    assets = json.loads(Path(__file__).with_name("opendecision_text_assets.json").read_text())
    for name, pin in assets["files"].items():
        data = (args.model / name).read_bytes()
        if len(data) != pin["bytes"] or hashlib.sha256(data).hexdigest() != pin["sha256"]:
            raise ValueError("Unqualified checkpoint: " + name)
    initializer = args.initializer.read_bytes()
    if hashlib.sha256(initializer).hexdigest() != "7caa16d3d730231fc7b9c2db4716e3e38736880bd4f2e016bb37987645e4900a":
        raise ValueError("Unqualified effective tokenizer")
    from transformers import AutoTokenizer
    reference = AutoTokenizer.from_pretrained(str(args.model), local_files_only=True)
    # Check the wrapper's effective configuration before any request changes it.
    if json.loads(reference.backend_tokenizer.to_str()) != json.loads(initializer):
        raise ValueError("Reference wrapper initializer differs")
    lib = c.CDLL(str(args.library.resolve()))
    lib.sm_tokenizer_load.argtypes = [c.c_void_p, c.c_size_t, c.POINTER(c.c_void_p)]
    lib.sm_tokenizer_encode.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(Output)]
    lib.sm_tokenizer_free.argtypes = [c.c_void_p]
    lib.sm_tokenizer_output_free.argtypes = [c.POINTER(Output)]
    lib.sm_tokenizer_free.restype = lib.sm_tokenizer_output_free.restype = None
    handle = c.c_void_p()
    assert lib.sm_tokenizer_load(initializer, len(initializer), c.byref(handle)) == 0
    checks = 0
    corpus = []

    def native(request):
        nonlocal checks
        data = request if isinstance(request, bytes) else json.dumps(request, ensure_ascii=False).encode()
        output = Output()
        status = lib.sm_tokenizer_encode(handle, data, len(data), c.byref(output))
        if status:
            assert not output.data and not output.length and not output.owner
            corpus.append(dict(request_hex=data.hex(), status=status, result=None))
            return status, None
        result = json.loads(c.string_at(output.data, output.length))
        # Refuse overwriting a live allocation, preserving all ownership fields.
        original = (output.data, output.length, output.owner)
        assert lib.sm_tokenizer_encode(handle, data, len(data), c.byref(output)) == 1
        assert (output.data, output.length, output.owner) == original
        lib.sm_tokenizer_output_free(c.byref(output))
        lib.sm_tokenizer_output_free(c.byref(output))
        assert not output.data and not output.length and not output.owner
        checks += 1
        corpus.append(dict(request_hex=data.hex(), status=status, result=result))
        return status, result

    rng = random.Random(841)
    samples = ["", "hello", "a\t b\n\r c", "é e\u0301", "नमस्ते दुनिया", "مرحبا بالعالم",
               "中文 日本語", "🙂👩🏽‍💻", "\x00hello", "[CLS] [SEP] [PAD]", "  hello   "]
    samples += ["".join(rng.choices("abc é\t\n中🙂", k=90)) for _ in range(12)]
    samples += ["The quick brown fox. " * 300]
    try:
        for pair in [False, True]:
            batches = [[[text, "This example concerns language."] if pair else [text]] for text in samples]
            batches += [[([text, "A possible answer."] if pair else [text]) for text in samples[:8]]]
            for texts in batches:
                for padding in ["none", "longest", "max_length"]:
                    for truncation in ["none", "longest_first", "only_first", "only_second"]:
                        for side in ["left", "right"]:
                            request = dict(texts=texts, max_length=512, padding=padding,
                                           truncation=truncation, padding_side=side, truncation_side=side)
                            reference.padding_side = reference.truncation_side = side
                            try:
                                expected = dict(reference(
                                    [item[0] for item in texts],
                                    text_pair=[item[1] for item in texts] if pair else None,
                                    padding=False if padding == "none" else padding,
                                    truncation=False if truncation == "none" else truncation,
                                    max_length=512, return_token_type_ids=True,
                                    return_attention_mask=True))
                            except Exception:
                                status, _ = native(request)
                                assert status in (1, 3), (request, status)
                                continue
                            status, actual = native(request)
                            if any(len(row) > 512 for row in expected["input_ids"]):
                                assert status == 5, (request, status)
                            else:
                                assert status == 0 and actual == expected, (request, status, actual, expected)
        valid = dict(texts=[["hello"]], max_length=16, padding="none", truncation="none")
        invalid = [b"{", b"\xff", dict(valid, unknown=1), dict(valid, texts=[]),
                   dict(valid, texts=[["hello"]] * 9), dict(valid, max_length=0),
                   dict(valid, max_length=513), dict(valid, texts=[["x" * 65537]]),
                   dict(valid, pad_id=99), dict(valid, texts=[[]]),
                   dict(valid, max_length=1, truncation="longest_first")]
        for request in invalid:
            assert native(request)[0] == 1
            assert native(valid)[0] == 0  # Every rejected request leaves the handle usable.
        print(f"PASS native tokenizer: {checks} successful encodings; exact three-matrix HF parity and ABI recovery")
        # Publish only after every comparison passes. The corpus uses synthetic
        # QA text and lets Linux replay the reference without a Python ML stack.
        if args.corpus_output:
            with args.corpus_output.open("x") as output:
                json.dump(dict(format="samosa.tokenizer-parity.v1", cases=corpus), output)
                output.write("\n")
    finally:
        lib.sm_tokenizer_free(handle)


if __name__ == "__main__":
    main()
