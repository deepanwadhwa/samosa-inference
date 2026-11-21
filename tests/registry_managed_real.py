#!/usr/bin/env python3
"""Optional actual-model lifecycle gate; native registry owns the real engine."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import select
import socket
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("engine", type=Path)
    parser.add_argument("model", type=Path)
    parser.add_argument("--engine-sha256", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    host = {("Darwin", "arm64"): "macos-arm64",
            ("Linux", "aarch64"): "linux-arm64"}[(platform.system(), platform.machine())]
    root = Path(tempfile.mkdtemp(prefix="samosa-registry-managed-real-"))
    state = root / "state"
    engine_size, model_size = args.engine.stat().st_size, args.model.stat().st_size
    binding = (f"llama-static-system-v1:{host}:system-runtime-v1:llama-native-template-v1:"
               f"{args.engine_sha256}:{engine_size}:{args.model_sha256}:{model_size}:4096:public-managed")
    identity = "managed-text-" + hashlib.sha256(binding.encode()).hexdigest()
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        port = probe.getsockname()[1]
    managed = dict(contract_version=1, profile="llama-static-system-v1", state=str(state),
                   engine=str(args.engine.resolve()), engine_sha256=args.engine_sha256, engine_size=engine_size,
                   model=str(args.model.resolve()), model_sha256=args.model_sha256, model_size=model_size,
                   context_tokens=4096, model_alias="public-managed", owner="qa", port=port,
                   expected_revision=0, gpu_layers=99, threads=4)
    config = root / "managed.json"
    registry = root / "registry.json"
    for path, value in ((config, managed), (registry, dict(contract_version=1, owner="qa", providers=[
            dict(kind="text", provider_id=identity, state=str(state / "admission"),
                 context_tokens=4096, managed_config_file=str(config))]))):
        with path.open("x") as out:
            os.chmod(path, 0o600)
            json.dump(value, out)
    checks, records = [], []
    with (root / "stderr.log").open("w") as stderr:
        process = subprocess.Popen([str(args.binary.resolve()), "--config", str(registry), "--session"],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=stderr, bufsize=0)
        sequence = 0

        def request(op, **extra):
            nonlocal sequence
            sequence += 1
            frame = dict(contract_version=1, request_id=str(sequence), request=dict(
                contract_version=1, op=op, **({} if op == "describe" else dict(provider_id=identity)), **extra))
            process.stdin.write(json.dumps(frame).encode() + b"\n")
            deadline = time.monotonic() + 180
            reply = bytearray()
            while not reply.endswith(b"\n"):
                left = deadline - time.monotonic()
                if left <= 0 or not select.select([process.stdout], [], [], left)[0]:
                    raise AssertionError(f"response timeout for {op}; retained {root}")
                byte = process.stdout.read(1)
                if not byte:
                    raise AssertionError(f"registry exited {process.poll()}; retained {root}")
                reply.extend(byte)
            envelope = json.loads(reply)
            assert envelope["request_id"] == str(sequence), envelope
            result = envelope["response"]
            assert "error" not in result, result
            records.append(dict(op=op, response=result))
            return result

        try:
            discovery = request("describe")
            assert discovery["managed_lifecycle"]["provider_ids"] == [identity]
            assert not state.exists()
            checks.append("discovery performs no engine/state work")
            assert request("provider_lifecycle_status")["ready"] is False
            assert request("provider_start")["ready"] is True
            assert request("provider_status")["ready"] is True
            checks.append("native model generation-qualified startup and live health")
            prompt = "Compute 2 + 2. Answer only the integer."
            request("count_tokens", prompt=prompt, operation_id="real-count")
            generated = request("generate", prompt=prompt, max_output_tokens=32, operation_id="real-generation")
            assert generated["answer"].strip() == "4", generated
            checks.append("actual public native counting and generation")
            assert request("provider_stop")["ready"] is False
            assert not list(state.glob("key.*"))
            replay = request("generate", prompt=prompt, max_output_tokens=32, operation_id="real-generation")
            assert replay == generated, (replay, generated)
            checks.append("exact tracked replay with engine stopped")
            assert request("provider_start")["ready"] is True
            assert request("provider_status")["ready"] is True
            checks.append("owned model restart qualifies again")
            process.stdin.close()
            assert process.wait(timeout=15) == 0
            with socket.socket() as stopped:
                assert stopped.connect_ex(("127.0.0.1", port)) != 0
            assert not list(state.glob("key.*"))
            checks.append("EOF engine cleanup and no retained credentials")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            process.stdout.close()
            if not process.stdin.closed:
                process.stdin.close()
    args.report.write_text(json.dumps(dict(platform=host, root=str(root), provider_id=identity,
        engine_sha256=args.engine_sha256, model_sha256=args.model_sha256,
        checks=checks, records=records), indent=2) + "\n")
    print(f"PASS actual registry lifecycle: {len(checks)} scenarios; {args.report}")


if __name__ == "__main__":
    main()
