#!/usr/bin/env python3
"""Build and qualify the native NLI primitive in an exclusive source snapshot."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(65536), b""):
            result.update(block)
    return result.hexdigest()


def copy_pinned(source, destination, expected):
    if digest(source) != expected:
        raise ValueError("Input pin mismatch: " + source.name)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, destination)
    destination.chmod(0o400)
    if digest(destination) != expected or digest(source) != expected:
        raise ValueError("Input changed during snapshot: " + source.name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk", type=Path)
    parser.add_argument("runtime", type=Path, help="Library filename must match its loader identity")
    parser.add_argument("model", type=Path)
    parser.add_argument("reference", type=Path)
    parser.add_argument("output", type=Path)
    for name in ["runtime", "model", "reference"]:
        parser.add_argument("--" + name + "-pin", required=True)
    args = parser.parse_args()
    for name in ["runtime", "model", "reference"]:
        pin = getattr(args, name + "_pin")
        if not re.fullmatch("[0-9a-f]{64}", pin) or digest(getattr(args, name)) != pin:
            raise ValueError("Unqualified " + name + " input")
    # A byte pin must identify the library actually selected by the loader.
    for name in ["DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
                 "DYLD_FALLBACK_LIBRARY_PATH", "LD_PRELOAD", "LD_LIBRARY_PATH"]:
        if os.environ.get(name):
            raise ValueError("Unqualified runtime loader environment")
    repository = Path(__file__).resolve().parent.parent
    sdk = json.loads((repository / "engines/decision/ort_sdk_pins.json").read_text())
    for name, entry in sdk["files"].items():
        source = args.sdk / name
        if source.stat().st_size != entry["bytes"] or digest(source) != entry["sha256"]:
            raise ValueError("Unqualified ORT SDK file: " + name)
    output = args.output.absolute()
    output.mkdir(mode=0o700)
    (output / "lib").mkdir()
    for name, entry in sdk["files"].items():
        target = output / ("licenses/ONNXRuntime-LICENSE" if name == "LICENSE" else "include/" + name)
        copy_pinned(args.sdk / name, target, entry["sha256"])
    library = output / "lib" / args.runtime.name
    copy_pinned(args.runtime, library, args.runtime_pin)
    sources = ["engines/decision/ort_nli.h", "engines/decision/ort_nli.cpp",
               "tests/test_decision_ort_real.cpp", "engines/json.h", "src/sha256.h"]
    source_pins = {}
    for name in sources:
        source_pins[name] = digest(repository / name)
        copy_pinned(repository / name, output / "source" / name, source_pins[name])
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    compiler_version = subprocess.check_output(compiler + ["--version"], text=True)
    common = ["-std=c++20", "-Wall", "-Wextra", "-Werror", "-Wpedantic",
              "-I" + str(output / "source/engines"), "-I" + str(output / "source/src"),
              "-I" + str(output / "include"), str(output / "source/engines/decision/ort_nli.cpp"),
              str(output / "source/tests/test_decision_ort_real.cpp"), str(library),
              "-Xlinker", "-rpath", "-Xlinker", str(output / "lib")]
    for suffix in ["", "-sanitize"]:
        executable = output / ("test-decision-ort-real" + suffix)
        options = ["-O2"] if not suffix else ["-O1", "-g", "-fsanitize=address,undefined",
                   "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
        print("BEGIN native NLI build " + (suffix or "normal"), flush=True)
        subprocess.run(compiler + options + common + ["-o", str(executable)], check=True, timeout=300)
        with (output / ("execution" + suffix + ".log")).open("w") as log:
            subprocess.run([str(executable), str(args.model.resolve()), str(args.reference.resolve()),
                            args.model_pin, args.reference_pin, "0.01"], stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=600)
        print("PASS native NLI " + (suffix or "normal"), flush=True)
    for name in ["runtime", "model", "reference"]:
        if digest(getattr(args, name)) != getattr(args, name + "_pin"):
            raise ValueError("Qualification input changed: " + name)
    record = dict(format="samosa.inference.nli-backend-qualification.v1",
                  scope="native tensor primitive only; tokenizer/provider still unqualified",
                  host=dict(system=platform.system(), machine=platform.machine()),
                  compiler=compiler_version, sdk=sdk, source_sha256=source_pins,
                  inputs={name: getattr(args, name + "_pin") for name in ["runtime", "model", "reference"]},
                  absolute_logit_tolerance=0.01,
                  sanitizer_scope="local C++ wrapper and harness; prebuilt runtime is not instrumented")
    (output / "qualification.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print("PASS independent native tensor qualification: " + str(output), flush=True)


if __name__ == "__main__":
    main()
