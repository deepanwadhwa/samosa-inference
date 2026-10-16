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
    parser.add_argument("--tokenizer-library", type=Path,
                        help="Pinned native static library enables fresh typed-decision qualification")
    parser.add_argument("--tokenizer-library-pin")
    parser.add_argument("--initializer", type=Path)
    parser.add_argument("--initializer-pin")
    parser.add_argument("--json-requests", action="store_true",
                        help="Exercise strict JSON decoding and complete typed result serialization")
    args = parser.parse_args()
    names = ["runtime", "model", "reference"]
    native_text = args.tokenizer_library is not None
    if args.json_requests and not native_text:
        raise ValueError("JSON requests require native text qualification inputs")
    if native_text:
        if args.tokenizer_library.suffix != ".a" or not args.initializer:
            raise ValueError("Typed qualification requires a static tokenizer and initializer")
        names += ["tokenizer_library", "initializer"]
    elif any([args.tokenizer_library_pin, args.initializer, args.initializer_pin]):
        raise ValueError("Incomplete typed qualification arguments")
    for name in names:
        pin = getattr(args, name + "_pin")
        if not pin or not re.fullmatch("[0-9a-f]{64}", pin) or digest(getattr(args, name)) != pin:
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
    sources = ["engines/decision/ort_nli.h", "engines/decision/ort_nli.cpp", "engines/decision/ort_runtime.h",
               "tests/test_decision_ort_real.cpp", "engines/json.h", "src/sha256.h"]
    if native_text:
        sources += ["engines/decision/native_text.h", "engines/decision/native_text.cpp",
                    "engines/decision/text_evidence.h", "engines/decision/text_evidence.cpp",
                    "engines/decision/tokenizer/tokenizer.h", "tests/test_decision_evidence_reference.cpp"]
    if args.json_requests:
        sources += ["engines/decision/request.h", "engines/decision/request.cpp",
                    "engines/decision/tokenizer/request.h"]
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
    if native_text:
        common.remove(str(output / "source/tests/test_decision_ort_real.cpp"))
        tokenizer_library = output / "lib" / args.tokenizer_library.name
        copy_pinned(args.tokenizer_library, tokenizer_library, args.tokenizer_library_pin)
        common += ["-DSAMOSA_DECISION_NATIVE_REAL", str(output / "source/engines/decision/native_text.cpp"),
                   str(output / "source/engines/decision/text_evidence.cpp"),
                   str(output / "source/tests/test_decision_evidence_reference.cpp"), str(tokenizer_library)]
        common += (["-framework", "Security", "-framework", "CoreFoundation", "-liconv", "-lresolv"]
                   if platform.system() == "Darwin" else ["-ldl", "-lpthread", "-lm"])
        if args.json_requests:
            common += ["-DSAMOSA_DECISION_JSON_REAL", str(output / "source/engines/decision/request.cpp")]
    for suffix in ["", "-sanitize"]:
        executable = output / ("test-decision-ort-real" + suffix)
        options = ["-O2"] if not suffix else ["-O1", "-g", "-fsanitize=address,undefined",
                   "-fno-sanitize-recover=all", "-fno-omit-frame-pointer"]
        print("BEGIN native NLI build " + (suffix or "normal"), flush=True)
        subprocess.run(compiler + options + common + ["-o", str(executable)], check=True, timeout=300)
        with (output / ("execution" + suffix + ".log")).open("w") as log:
            execution = ([str(args.model.resolve()), str(args.initializer.resolve()), str(args.reference.resolve()),
                          args.model_pin, args.initializer_pin, args.reference_pin] if native_text else
                         [str(args.model.resolve()), str(args.reference.resolve()),
                          args.model_pin, args.reference_pin, "0.01"])
            subprocess.run([str(executable)] + execution, stdout=log,
                           stderr=subprocess.STDOUT, check=True, timeout=600)
        print("PASS native NLI " + (suffix or "normal"), flush=True)
    for name in names:
        if digest(getattr(args, name)) != getattr(args, name + "_pin"):
            raise ValueError("Qualification input changed: " + name)
    record = dict(format="samosa.inference.nli-backend-qualification.v1",
                  scope=("fresh native JSON requests/results; public provider still unqualified" if args.json_requests else
                         "fresh native typed decisions; public provider still unqualified" if native_text else
                         "native tensor primitive only; tokenizer/provider still unqualified"),
                  host=dict(system=platform.system(), machine=platform.machine()),
                  compiler=compiler_version, sdk=sdk, source_sha256=source_pins,
                  inputs={name: getattr(args, name + "_pin") for name in names},
                  absolute_logit_tolerance=0.01,
                  absolute_result_tolerance=0.005 if native_text else None,
                  sanitizer_scope="local C++ code and harness; prebuilt runtime and Rust archive are not instrumented")
    (output / "qualification.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    print("PASS independent native tensor qualification: " + str(output), flush=True)


if __name__ == "__main__":
    main()
