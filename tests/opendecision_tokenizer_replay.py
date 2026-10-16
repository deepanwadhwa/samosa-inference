#!/usr/bin/env python3
"""Replay an explicitly pinned HF parity corpus with only Python's standard library."""
import argparse
import ctypes as c
import hashlib
import json
from pathlib import Path
from opendecision_tokenizer_parity import Output


def main():
    if not __debug__:
        raise RuntimeError("Checks require unoptimized Python")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    parser.add_argument("initializer", type=Path)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--corpus-sha256", required=True)
    args = parser.parse_args()
    initializer = args.initializer.read_bytes()
    corpus_bytes = args.corpus.read_bytes()
    if hashlib.sha256(initializer).hexdigest() != "7caa16d3d730231fc7b9c2db4716e3e38736880bd4f2e016bb37987645e4900a":
        raise ValueError("Initializer pin mismatch")
    if hashlib.sha256(corpus_bytes).hexdigest() != args.corpus_sha256:
        raise ValueError("Corpus pin mismatch")
    corpus = json.loads(corpus_bytes)
    if corpus["format"] != "samosa.tokenizer-parity.v1" or not corpus["cases"]:
        raise ValueError("Invalid corpus")
    lib = c.CDLL(str(args.library.resolve()))
    lib.sm_tokenizer_load.argtypes = [c.c_void_p, c.c_size_t, c.POINTER(c.c_void_p)]
    lib.sm_tokenizer_encode.argtypes = [c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(Output)]
    lib.sm_tokenizer_free.argtypes = [c.c_void_p]
    lib.sm_tokenizer_output_free.argtypes = [c.POINTER(Output)]
    lib.sm_tokenizer_free.restype = lib.sm_tokenizer_output_free.restype = None
    handle = c.c_void_p()
    assert lib.sm_tokenizer_load(initializer, len(initializer), c.byref(handle)) == 0
    try:
        for index, case in enumerate(corpus["cases"]):
            data = bytes.fromhex(case["request_hex"])
            output = Output()
            status = lib.sm_tokenizer_encode(handle, data, len(data), c.byref(output))
            assert status == case["status"], (index, status, case["status"])
            if status == 0:
                try:
                    actual = json.loads(c.string_at(output.data, output.length))
                    assert actual == case["result"], index
                finally:
                    lib.sm_tokenizer_output_free(c.byref(output))
            else:
                assert not output.data and not output.length and not output.owner, index
        print(f"PASS native tokenizer replay: {len(corpus['cases'])} pinned HF corpus cases")
    finally:
        lib.sm_tokenizer_free(handle)


if __name__ == "__main__":
    main()
