#!/usr/bin/env python3
"""Public owned T5 lifecycle, using the existing native worker fixture or model."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import signal
import sqlite3
import subprocess
import tempfile
import time


class Session:
    def __init__(self, binary, config, log, env):
        self.process = subprocess.Popen([str(binary), "--config", str(config), "--session"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log, env=env, bufsize=0)
        self.buffer, self.responses, self.sequence = bytearray(), {}, 0

    def submit(self, request):
        self.sequence += 1
        identity = str(self.sequence)
        self.process.stdin.write(json.dumps(dict(contract_version=1, request_id=identity, request=request)).encode() + b"\n")
        return identity

    def receive(self, identity, timeout=90):
        deadline = time.monotonic() + timeout
        while identity not in self.responses:
            if b"\n" in self.buffer:
                line, _, rest = self.buffer.partition(b"\n")
                self.buffer[:] = rest
                value = json.loads(line)
                self.responses[value["request_id"]] = value["response"]
                continue
            left = deadline - time.monotonic()
            assert left > 0 and select.select([self.process.stdout], [], [], left)[0], "registry response timeout"
            data = os.read(self.process.stdout.fileno(), 65536)
            assert data, f"registry exited {self.process.poll()}"
            self.buffer.extend(data)
        return self.responses.pop(identity)

    def close(self):
        self.process.stdin.close()
        assert self.process.wait(timeout=15) == 0
        self.process.stdout.close()


def private(path, value):
    path.write_text(json.dumps(value))
    path.chmod(0o600)


def digest(path):
    with path.open("rb") as f:
        h = hashlib.sha256()
        for data in iter(lambda: f.read(65536), b""):
            h.update(data)
        return h.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("binary", type=Path)
    p.add_argument("engine", type=Path)
    p.add_argument("--model", type=Path)
    p.add_argument("--output", type=Path)
    a = p.parse_args()
    binary_pin = digest(a.binary)
    root = a.output.resolve() if a.output else Path(tempfile.mkdtemp(prefix="samosa-summary-registry-"))
    if a.output:
        root.mkdir(mode=0o700)
    engine, model = root / "engine", root / "model"
    shutil.copyfile(a.engine, engine)
    engine.chmod(0o700)
    if a.model:
        shutil.copyfile(a.model, model)
    else:
        model.write_bytes(b"abc")
    model.chmod(0o600)
    state, config = root / "state", root / "registry.json"
    provider = dict(kind="summarization", provider_id="native-t5-summary-v1", state=str(state),
        engine=str(engine), model=str(model), engine_sha256=digest(engine), model_sha256=digest(model),
        engine_size=engine.stat().st_size, model_size=model.stat().st_size, lifecycle="resident-budget-v1")
    registry = dict(contract_version=1, owner="qa", providers=[provider,
        dict(kind="fake_text", provider_id="peer", state=str(root / "peer"))])
    private(config, registry)
    mode, marker, budgets = root / "mode", root / "marker", root / "budgets"
    mode.write_text("echo")
    env = os.environ.copy()
    if not a.model:
        env.update(SAMOSA_FAKE_SUMMARIZER_CONTROL=str(mode), SAMOSA_FAKE_SUMMARIZER_MARKER=str(marker),
                   SAMOSA_FAKE_SUMMARIZER_BUDGET_LOG=str(budgets))
    records, checks = [], []
    source = ("South Carolina officials reported 30 cases of cyclosporiasis in 2026. "
              "None were linked to the national outbreak. The illness can cause watery diarrhea.")

    def request(op, **extra):
        return dict(contract_version=1, op=op, **({} if op == "describe" else dict(provider_id=provider["provider_id"])), **extra)

    def call(op, expected=None, **extra):
        response = session.receive(session.submit(request(op, **extra)))
        records.append(dict(op=op, response=response))
        assert response.get("error", {}).get("code") == expected, response
        return response

    def activation():
        with sqlite3.connect(f"file:{state}/activation/activation.sqlite3?mode=ro", uri=True) as db:
            return db.execute("SELECT revision,config,digest FROM current WHERE id=1").fetchone()

    def released():
        with (state / "activation" / "residency.lock").open("rb") as fd:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                return True
            except BlockingIOError:
                return False

    with (root / "stderr.log").open("w") as log:
        session = Session(a.binary.resolve(), config, log, env)
        try:
            assert call("describe")["managed_lifecycle"]["provider_ids"] == [provider["provider_id"]]
            assert not state.exists()
            assert call("provider_lifecycle_status")["ready"] is False
            call("provider_start", "unknown_field", engine="/foreign")
            assert not state.exists()
            assert call("provider_start")["ready"] is True
            assert not released()
            initial = activation()
            assert initial[0] == 1
            assert call("provider_start")["ready"] is True and activation() == initial
            checks.append("lazy discovery, strict controls, actual warm qualification, idempotent start")
            saved = None
            generated = {}
            for tokens in (16, 64, 128, 256):
                response = call("summarize", text=source, max_output_tokens=tokens, operation_id=f"summary-{tokens}")
                assert response["text"] and response["coverage"]["status"] == "partial"
                assert response["provenance"]["max_output_tokens"] == tokens
                assert response["provenance"]["processed_input_tokens"] is None
                generated[tokens] = response["text"]
                if tokens == 128:
                    saved = response
                    if a.model:
                        assert "30" in response["text"] and "Carolina" in response["text"], response
            opening = call("summarize_opening", text=source * (20 if a.model else 1), max_output_tokens=64, operation_id="opening")
            assert opening["provenance"]["input_partitioning"] == "native_token_chunks"
            assert opening["coverage"]["status"] == "partial"
            assert activation() == initial
            if a.model:
                assert len(generated[16]) < len(generated[256]) and generated[256].startswith(generated[16]), generated
            if not a.model:
                observed = [line.split() for line in budgets.read_text().splitlines()]
                assert {int(row[1]) for row in observed} >= {16, 64, 128, 256}
                assert len({row[0] for row in observed}) == 1
            checks.append("16/64/128/256 budgets and opening/map-reduce on one activation/worker")
            call("summarize", "idempotency_conflict", text=source, max_output_tokens=64, operation_id="summary-128")
            assert call("provider_stop")["ready"] is False and released()
            assert call("summarize", text=source, max_output_tokens=128, operation_id="summary-128") == saved
            offline = json.loads(json.dumps(registry))
            offline["providers"][0]["engine"], offline["providers"][0]["model"] = "/missing/engine", "/missing/model"
            offline_config = root / "offline.json"
            private(offline_config, offline)
            result = subprocess.run([str(a.binary.resolve()), "--config", str(offline_config)],
                input=json.dumps(request("summarize", text=source, max_output_tokens=128, operation_id="summary-128")),
                capture_output=True, text=True, timeout=15)
            assert result.returncode == 0 and json.loads(result.stdout) == saved, result
            checks.append("exact stopped-worker and separate-process model/engine-free replay")
            foreign = json.loads(json.dumps(offline))
            foreign["owner"] = "bob"
            foreign_config = root / "foreign.json"
            private(foreign_config, foreign)
            result = subprocess.run([str(a.binary.resolve()), "--config", str(foreign_config)],
                input=json.dumps(request("operation_get", operation_id="summary-128")), capture_output=True, text=True, timeout=15)
            assert result.returncode == 1 and json.loads(result.stdout)["error"]["code"] == "unknown_operation"
            checks.append("owner-scoped retained receipts refuse a different configured owner")
            if not a.model:
                for bad in ("empty", "invalid-utf8", "legacy"):
                    mode.write_text(bad)
                    call("provider_start", "provider_process_failed" if bad == "legacy" else "invalid_provider_response")
                    assert activation() == initial and released()
                    assert call("provider_lifecycle_status")["ready"] is False
                checks.append("failed warm/protocol qualification preserves exact last-good")
                mode.write_text("stall-after-warm")
                assert call("provider_start")["ready"] is True
                active = session.submit(request("summarize", text=source, max_output_tokens=128, operation_id="cancel-active", timeout_ms=10000))
                until = time.monotonic() + 5
                while not marker.exists() and time.monotonic() < until:
                    time.sleep(.01)
                assert marker.exists()
                call("provider_stop", "provider_busy")
                call("provider_start", "provider_busy")
                call("operation_cancel", operation_id="cancel-active")
                assert session.receive(active)["error"]["code"] == "cancelled"
                assert released() and call("provider_lifecycle_status")["ready"] is False
                checks.append("active public cancellation, busy lifecycle exclusion and no false readiness")
                marker.unlink()
                assert call("provider_start")["ready"] is True
                call("summarize", "deadline_exceeded", text=source, max_output_tokens=128,
                     operation_id="deadline-active", timeout_ms=200)
                assert released() and call("provider_lifecycle_status")["ready"] is False
                checks.append("public deadline reaps its partial worker and releases residency")
                mode.write_text("echo")
            assert call("provider_start")["ready"] is True
            call("summarize", text=source, max_output_tokens=128, operation_id="survivor")
            if not a.model:
                with model.open("r+b") as file:
                    file.write(b"xyz")
                health = call("provider_lifecycle_status")
                assert health["ready"] is False and health["health_error"] == "provider_artifact_changed"
                call("summarize", "provider_artifact_changed", text=source, max_output_tokens=128, operation_id="mutated")
                assert call("provider_stop")["ready"] is False
                model.write_bytes(b"abc")
                assert call("provider_start")["ready"] is True
                checks.append("live artifact mutation refuses readiness/work and explicit restart recovers")
                engine_pid = int(budgets.read_text().splitlines()[-1].split()[0])
                good = activation()
                os.kill(engine_pid, signal.SIGKILL)
                until = time.monotonic() + 3
                while time.monotonic() < until:
                    if not call("provider_lifecycle_status")["ready"]:
                        break
                    time.sleep(.01)
                call("provider_start", "owned_engine_exited")
                call("provider_stop", "engine_failed")
                assert released() and activation() == good
                assert call("provider_start")["ready"] is True
                checks.append("engine crash refuses stale readiness and explicit stop/restart preserves last-good")
            session.close()
            assert released()
            checks.append("EOF reaps owned worker and releases residency")
            if not a.model:
                survivor = subprocess.Popen(["sleep", "30"])
                try:
                    session = Session(a.binary.resolve(), config, log, env)
                    assert call("provider_start")["ready"] is True
                    session.process.kill()
                    session.process.wait(timeout=5)
                    session.process.stdin.close()
                    session.process.stdout.close()
                    until = time.monotonic() + 3
                    while not released() and time.monotonic() < until:
                        time.sleep(.01)
                    assert released() and survivor.poll() is None
                    checks.append("SIGKILL owner cleanup releases residency without killing unrelated process")
                finally:
                    survivor.terminate()
                    survivor.wait(timeout=5)
        finally:
            if session.process.poll() is None:
                session.process.kill()
                session.process.wait()
    assert digest(a.binary) == binary_pin, "qualification binary changed during the run"
    private(root / "report.json", dict(checks=checks, records=records, provider=provider,
        actual_model=bool(a.model), binary_sha256=binary_pin))
    print(f"PASS summary registry lifecycle: {len(checks)} scenarios; {root}")


if __name__ == "__main__":
    main()
