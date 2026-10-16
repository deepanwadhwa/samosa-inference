#!/usr/bin/env python3
"""Differential QA for native state text against OpenDecision's JSON convention."""
import argparse
import ctypes as c
import json
import math
from pathlib import Path
import random
import struct
from opendecision_tokenizer_parity import Output


def main():
    if not __debug__:
        raise RuntimeError("Checks require unoptimized Python")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=Path)
    args = parser.parse_args()
    lib = c.CDLL(str(args.library.resolve()))
    lib.sm_decision_state.argtypes = [c.c_void_p, c.c_size_t, c.POINTER(Output)]
    lib.sm_tokenizer_output_free.argtypes = [c.POINTER(Output)]
    lib.sm_tokenizer_output_free.restype = None
    checks = 0

    def check(raw, status=0):
        nonlocal checks
        data = raw.encode() if isinstance(raw, str) else raw
        output = Output()
        observed = lib.sm_decision_state(data, len(data), c.byref(output))
        assert observed == status, (data[:200], observed, status)
        if observed:
            assert not output.data and not output.owner and not output.length
        else:
            try:
                value = json.loads(data)
                expected = value if isinstance(value, str) else json.dumps(
                    value, ensure_ascii=False, sort_keys=True, separators=(",", ":"), allow_nan=False)
                actual = c.string_at(output.data, output.length)
                assert actual == expected.encode(), (data[:200], actual, expected)
                original = (output.data, output.length, output.owner)
                assert lib.sm_decision_state(data, len(data), c.byref(output)) == 1
                assert (output.data, output.length, output.owner) == original
            finally:
                lib.sm_tokenizer_output_free(c.byref(output))
                lib.sm_tokenizer_output_free(c.byref(output))
        checks += 1

    fixtures = [None, True, False, "", "hello\0world", "नमस्ते 中文 🙂", "é e\u0301",
                {"status": "paid", "currency": "€", "note": "Café invoice"},
                {"😀": 1, "é": 2, "\uffff": 3, "a": [None, True, -0.0]},
                {"b": {"z": 1, "a": "\t\r\n\b\f\u0001"}, "a": []}, 10**2048]
    for value in fixtures:
        check(json.dumps(value, ensure_ascii=False))
    for raw in ["-0", "0e0", "1E+003", "1e-324", "-1e-999", "1.2345678901234567890123456789",
                '{"z":0,"a":1,"a":2}', '["\\uD83D\\uDE00","\\u0000"]']:
        check(raw)
    for exponent in range(-323, 309):
        check(f"[1e{exponent},-1e{exponent}]")
    rng = random.Random(0x53414D4F5341)
    for _ in range(25000):
        value = struct.unpack("!d", rng.getrandbits(64).to_bytes(8, "big"))[0]
        if math.isfinite(value):
            check(json.dumps({"z": value, "a": [value, -value]}, allow_nan=False))
    check(json.dumps("x" * 65536))
    check(json.dumps("x" * 65537), 5)
    check("[" * 66 + "0" + "]" * 66, 5)
    for raw in [b"\xff", b'"\\ud800"', b"1e999", b"NaN", b"[1,]", b"{}{}", b""]:
        check(raw, 1)
    print(f"PASS native state rendering: {checks} Python differential and ABI checks")


if __name__ == "__main__":
    main()
