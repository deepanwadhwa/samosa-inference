#!/usr/bin/env python3
"""Actual pinned managed embedding gate; no synthetic vectors or model downloads."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import select
import socket
import subprocess
import time
import sys


def sha(path):
    with open(path, "rb") as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
        return digest.hexdigest()


def private_json(path, value):
    with open(path, "x") as stream:
        os.chmod(path, 0o600)
        json.dump(value, stream)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--registry-binary")
    parser.add_argument("--binary", required=True)
    parser.add_argument("--offline-binary", required=True)
    parser.add_argument("--engine", required=True)
    parser.add_argument("--model", required=True)
    parser.add_argument("--model-sha256", required=True)
    parser.add_argument("--runtime-revision", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    root = Path(args.output).resolve()
    root.mkdir(mode=0o700)
    engine, model = Path(args.engine).resolve(), Path(args.model).resolve()
    assert sha(model) == args.model_sha256, "model pin mismatch"
    engine_sha = sha(engine)
    embedding = dict(model_id="google/embeddinggemma-2-Q8_0",
                     model_revision=args.runtime_revision,
                     model_sha256=args.model_sha256, engine_sha256=engine_sha,
                     tokenizer_hash=args.model_sha256,
                     query_prefix="task: search result | query: ", document_prefix="",
                     recipe="embeddinggemma2-q8-mean-l2-f32-v1", dimensions=768)
    private_json(root / "embedding.json", embedding)
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    config = dict(contract_version=1, profile="llama-embedding-static-system-v1",
                  state=str(root / "state"), engine=str(engine), model=str(model),
                  engine_sha256=engine_sha, model_sha256=args.model_sha256,
                  engine_size=engine.stat().st_size, model_size=model.stat().st_size,
                  context_tokens=8192, model_alias="embedding-qa", owner="embedding-qa",
                  port=port, expected_revision=0, gpu_layers=0, threads=4,
                  embedding_config_file=str(root / "embedding.json"))
    private_json(root / "managed.json", config)
    receipts = []
    with open(root / "stderr.log", "wb") as log:
        process = subprocess.Popen([args.binary, "--config", str(root / "managed.json")],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log)
        buffer = bytearray()
        credential = root / "registry-key"

        def retain_test_credential():
            # The supervisor removes its startup key before announcing readiness.
            # This trusted host harness retains a private copy only for live
            # registry requests, and removes it before testing offline replay.
            if not args.registry_binary or credential.exists():
                return
            for path in (root / "state").glob("key.*"):
                try:
                    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW)
                except FileNotFoundError:
                    continue
                try:
                    value = os.read(fd, 65)
                    stat = os.fstat(fd)
                finally:
                    os.close(fd)
                assert stat.st_uid == os.geteuid() and not stat.st_mode & 0o077
                if len(value) == 64:
                    fd = os.open(credential, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
                    try:
                        os.write(fd, value)
                    finally:
                        os.close(fd)
                    return

        def receive(timeout=180):
            deadline = time.monotonic() + timeout
            while b"\n" not in buffer:
                retain_test_credential()
                remaining = deadline - time.monotonic()
                assert remaining > 0, "managed response timed out"
                ready, _, _ = select.select([process.stdout], [], [], min(remaining, 0.01))
                if not ready:
                    continue
                chunk = os.read(process.stdout.fileno(), 65536)
                assert chunk, f"managed EOF, exit={process.poll()}"
                buffer.extend(chunk)
            line, _, rest = buffer.partition(b"\n")
            buffer[:] = rest
            return json.loads(line)

        def registry_call(request, expected=0):
            result = subprocess.run([args.registry_binary, "--config", str(root / "registry.json")],
                                    input=json.dumps(request), text=True, capture_output=True, timeout=90)
            assert result.returncode == expected, (result.stdout, result.stderr)
            return json.loads(result.stdout)

        def embed(text, role, operation, title=None):
            request = dict(contract_version=1, op="embed_text", text=text,
                           role=role, operation_id=operation, timeout_ms=60000)
            if title is not None:
                request["title"] = title
            frame = dict(contract_version=1, request_id=operation, request=request)
            if args.registry_binary:
                request["provider_id"] = event["provider_id"]
                response = dict(request_id=operation, response=registry_call(request))
            else:
                process.stdin.write(json.dumps(frame).encode() + b"\n")
                process.stdin.flush()
                response = receive()
            assert response["request_id"] == operation, response
            result = response["response"]
            assert "error" not in result, result
            vector = result["vector"]
            assert len(vector) == 768 and all(math.isfinite(x) for x in vector)
            assert abs(sum(x*x for x in vector) - 1) < 0.001
            assert result["input_sha256"] == hashlib.sha256(text.encode()).hexdigest()
            assert result["role"] == role and result["title"] == title
            assert result["provenance"]["model_sha256"] == args.model_sha256
            assert result["provenance"]["engine_sha256"] == engine_sha
            receipts.append(response)
            return result

        try:
            event = receive()
            assert event.get("event") == "provider_started", event
            if args.registry_binary:
                assert credential.exists(), "startup credential was not retained"
                provider = dict(kind="embeddings", provider_id=event["provider_id"],
                                state=str(root / "state" / "admission"), endpoint=f"http://127.0.0.1:{port}",
                                model_alias="embedding-qa", context_tokens=8192,
                                api_key_file=str(credential), embedding_config_file=str(root / "embedding.json"))
                registry = dict(contract_version=1, owner="embedding-qa", providers=[provider,
                    dict(kind="fake_text", provider_id="contract-text", state=str(root / "text-state"))])
                private_json(root / "registry.json", registry)
                discovery = registry_call(dict(contract_version=1, op="describe"))
                assert len(discovery["providers"]) == 2
            documents = [
                ("Backups", "Restore a deleted file by opening the backup archive and recovering the saved copy."),
                ("Bread", "Combine flour, water and yeast. Knead the dough, let it rise, then bake the loaf."),
                ("Astronomy", "The moon orbits Earth and reflects sunlight. Its phases repeat each month."),
                ("Access", "Reset your forgotten password using the account recovery link sent to your email address."),
            ]
            queries = [("How can I get back a document I accidentally erased?", 0),
                       ("What are the steps for making a loaf?", 1),
                       ("Why does our natural satellite change shape in the sky?", 2),
                       ("I cannot remember my login secret. How do I regain access?", 3)]
            vectors = [embed(text, "document", f"doc-{i}", title)
                       for i, (title, text) in enumerate(documents)]
            rankings = []
            for i, (query, expected) in enumerate(queries):
                result = embed(query, "query", f"query-{i}")
                assert all(result["profile"] == v["profile"] for v in vectors)
                scores = [sum(a*b for a, b in zip(result["vector"], v["vector"])) for v in vectors]
                order = sorted(range(len(scores)), key=scores.__getitem__, reverse=True)
                assert order[0] == expected, (query, scores, order)
                rankings.append(dict(query=query, expected=expected, scores=scores, order=order))
            repeated = embed(queries[0][0], "query", "query-0")
            assert repeated == receipts[4]["response"], "durable receipt replay changed"
            frame = dict(contract_version=1, request_id="too-long", request=dict(
                contract_version=1, op="embed_text", text="sun " * 9000,
                role="document", operation_id="too-long", timeout_ms=60000))
            if args.registry_binary:
                frame["request"]["provider_id"] = event["provider_id"]
                refused = dict(response=registry_call(frame["request"], 1))
            else:
                process.stdin.write(json.dumps(frame).encode() + b"\n")
                process.stdin.flush()
                refused = receive()
            assert refused["response"]["error"]["code"] == "context_limit", refused
            private_json(root / "long-input-refusal.json", refused)
            private_json(root / "receipts.json", receipts)
            private_json(root / "rankings.json", rankings)
        finally:
            if credential.exists():
                credential.unlink()
            failed = sys.exc_info()[0] is not None
            process.stdin.close()
            try:
                rc = process.wait(timeout=20)
            except subprocess.TimeoutExpired:
                process.terminate()
                rc = process.wait(timeout=10)
            if not failed:
                assert rc == 0, f"managed shutdown failed: {rc}"
    offline_request = dict(contract_version=1, op="embed_text", text=queries[0][0],
                           role="query", operation_id="query-0", timeout_ms=60000)
    if args.registry_binary:
        registry["providers"][0]["endpoint"] = "http://127.0.0.1:1"
        registry["providers"][0].pop("api_key_file")
        # Supervisor shutdown removes its secret; replay needs neither endpoint
        # nor credentials, while the pinned representation remains unchanged.
        (root / "registry.json").unlink()
        private_json(root / "registry.json", registry)
        offline_request["provider_id"] = event["provider_id"]
        replay = registry_call(offline_request)
        assert replay == receipts[4]["response"]
        private_json(root / "registry-offline-replay.json", replay)
        offline_request.pop("provider_id")
    offline = subprocess.run([
        args.offline_binary, "--llama-endpoint", "http://127.0.0.1:1",
        "--provider-id", event["provider_id"], "--model-alias", "embedding-qa",
        "--context-tokens", "8192", "--owner", "embedding-qa",
        "--admission-state", str(root / "state" / "admission"),
        "--embedding-config-file", str(root / "embedding.json")],
        input=json.dumps(offline_request), capture_output=True, text=True, timeout=20)
    assert offline.returncode == 0, (offline.stdout, offline.stderr)
    assert json.loads(offline.stdout) == receipts[4]["response"], "offline replay changed"
    private_json(root / "offline-replay.json", json.loads(offline.stdout))
    config["state"] = str(root / "single-state")
    private_json(root / "single-managed.json", config)
    single = subprocess.run([args.binary, "--config", str(root / "single-managed.json"), "--single"],
                            input=json.dumps(offline_request), capture_output=True, text=True, timeout=180)
    assert single.returncode == 0, (single.stdout, single.stderr)
    result = json.loads(single.stdout)
    assert result["profile"] == receipts[4]["response"]["profile"]
    assert result["vector"] == receipts[4]["response"]["vector"], "single/stream vectors differ"
    private_json(root / "single-response.json", result)
    print(f"PASS actual managed/registry embedding: 768 dimensions, 4/4 paraphrase rankings, long-input refusal, offline replay, single/stream parity, shutdown; {root}")


if __name__ == "__main__":
    main()
