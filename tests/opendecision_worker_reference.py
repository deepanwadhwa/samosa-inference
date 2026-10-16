#!/usr/bin/env python3
"""QA only: pinned actual-model decisions through one persistent framed worker."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import select
import subprocess
import time
from test_decision_transport import frame, receive


def digest(path):
    value = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(65536), b""):
            value.update(chunk)
    return value.hexdigest()


def main():
    if not __debug__:
        raise RuntimeError("Checks require unoptimized Python")
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ["worker", "model", "initializer", "reference"]:
        parser.add_argument(name, type=Path)
        parser.add_argument("--"+name+"-pin", required=True)
    parser.add_argument("--stderr-log", type=Path, required=True)
    args = parser.parse_args()
    for name in ["DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
                 "DYLD_FALLBACK_LIBRARY_PATH", "LD_PRELOAD", "LD_LIBRARY_PATH"]:
        if os.environ.get(name):
            raise ValueError("Unqualified loader environment")
    def verify():
        for name in ["worker", "model", "initializer", "reference"]:
            if digest(getattr(args, name)) != getattr(args, name+"_pin"):
                raise ValueError("Pin mismatch: "+name)
    verify()
    report = json.loads(args.reference.read_bytes())
    assert report["format"] == "samosa.inference.opendecision-reference.v1"
    assert len(report["cases"]) == 10
    maximum_error = 0
    def compare(actual, expected):
        nonlocal maximum_error
        if isinstance(expected, (int, float)) and not isinstance(expected, bool):
            assert isinstance(actual, (int, float)) and not isinstance(actual, bool) and math.isfinite(actual)
            error = abs(actual-expected)
            maximum_error = max(maximum_error, error)
            assert error < 0.005, (actual, expected)
        elif isinstance(expected, dict):
            assert isinstance(actual, dict) and actual.keys() == expected.keys()
            for key in expected:
                compare(actual[key], expected[key])
        elif isinstance(expected, list):
            assert isinstance(actual, list) and len(actual) == len(expected)
            for a, e in zip(actual, expected):
                compare(a, e)
        else:
            assert actual == expected
    with args.stderr_log.open("xb") as log:
        process = subprocess.Popen([str(args.worker.resolve()), "--model", str(args.model.resolve()),
                                    "--tokenizer", str(args.initializer.resolve())],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log, bufsize=0)
        try:
            os.set_blocking(process.stdin.fileno(), False)
            def send(data):
                pending = memoryview(frame(data))
                deadline = time.monotonic()+120
                while pending:
                    remaining = deadline-time.monotonic()
                    if remaining <= 0 or not select.select([], [process.stdin], [], remaining)[1]:
                        raise RuntimeError("Worker write timeout")
                    try:
                        count = os.write(process.stdin.fileno(), pending)
                    except BlockingIOError:
                        continue
                    assert count > 0
                    pending = pending[count:]
            def run(case):
                send(json.dumps(dict(method=case["method"], **case["arguments"]),
                                ensure_ascii=False, allow_nan=False).encode())
                compare(receive(process, timeout=120), case["result"])
                assert process.poll() is None
            for case in report["cases"]:
                print("BEGIN worker "+case["name"], flush=True)
                run(case)
            send(b"{")
            assert receive(process) == {"error": "invalid_decision_request"}
            run(next(case for case in report["cases"] if case["name"] == "predicate_true"))
            process.stdin.close()
            assert process.wait(timeout=15) == 0
            assert process.stdout.read() == b""
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()
        assert log.tell() <= 1048576
    verify()
    print(f"PASS persistent native worker: 10 actual-model cases, error recovery and clean EOF; max result error {maximum_error:.9g}")


if __name__ == "__main__":
    main()
