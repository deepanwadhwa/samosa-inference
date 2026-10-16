#!/usr/bin/env python3
"""Exercise framed worker pipes against the actual Rust decoder and controlled logits."""
import json
import os
import select
import struct
import subprocess
import sys
import time


def frame(data):
    return struct.pack("!I", len(data)) + data


def receive(process, timeout=10):
    deadline = time.monotonic() + timeout
    def exact(count):
        result = bytearray()
        while len(result) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([process.stdout], [], [], remaining)[0]:
                raise RuntimeError("Worker response timeout")
            data = os.read(process.stdout.fileno(), count-len(result))
            if not data:
                raise RuntimeError("Worker response truncated")
            result.extend(data)
        return bytes(result)
    size = struct.unpack("!I", exact(4))[0]
    assert 0 < size <= 1048576
    return json.loads(exact(size))


def main():
    if not __debug__:
        raise RuntimeError("Checks require unoptimized Python")
    executable = sys.argv[1]
    checks = 0
    def start():
        return subprocess.Popen([executable], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, bufsize=0)
    good = b'{"method":"noul","state":"text","instructions":"question"}'
    process = start()
    try:
        # Split headers, body and a multibyte scalar across separate writes.
        request = frame(json.dumps(dict(method="noul", state="🙂", instructions="q"),
                                   ensure_ascii=False).encode())
        for byte in request:
            process.stdin.write(bytes([byte]))
        assert receive(process) == {"type": "noul", "noul": 0.5}
        checks += 1
        cases = [(b"{", "invalid_decision_request"), (b"\xff", "invalid_decision_request"),
                 (b"throw", "engine_failed"), (b"empty", "engine_failed"),
                 (b"input-limit", "decision_input_limit"),
                 (b"oversize", "decision_result_limit")]
        for request, error in cases:
            process.stdin.write(frame(request))
            assert receive(process) == {"error": error}
            process.stdin.write(frame(good))
            assert receive(process) == {"type": "noul", "noul": 0.5}
            assert process.poll() is None
            checks += 3
        # Maximum legal input frame, with a tiny result and no model pairs.
        request = b'{"method":"relations","items":[]}'
        request += b" " * (1048576-len(request))
        # Unbuffered pipe writes can be short; retain and send the unwritten tail.
        data = frame(request)
        while data:
            count = process.stdin.write(data)
            assert count
            data = data[count:]
        assert receive(process) == []
        checks += 1
        process.stdin.write(frame(good) + frame(good))
        assert receive(process) == receive(process) == {"type": "noul", "noul": 0.5}
        checks += 1
        process.stdin.close()
        assert process.wait(timeout=10) == 0
        assert process.stdout.read() == b"" and process.stderr.read() == b""
        checks += 2
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    for malformed in [b"\0\0", struct.pack("!I", 0), struct.pack("!I", 1048577),
                      struct.pack("!I", 10)+b"xx"]:
        result = subprocess.run([executable], input=malformed, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=10)
        assert result.returncode == 1 and result.stdout == b"" and result.stderr == b""
        checks += 1
    process = start()
    try:
        process.stdout.close()
        process.stdin.write(frame(good))
        process.stdin.close()
        assert process.wait(timeout=10) == 2  # EPIPE, not an unhandled SIGPIPE.
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
    checks += 1
    print(f"PASS decision transport: {checks} pipe checks; fragmentation, boundaries, recovery, EOF and broken reader")


if __name__ == "__main__":
    main()
