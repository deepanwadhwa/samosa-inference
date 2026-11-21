# Native package engines

## Existing Qwen engine

`qwen36b.c`, expert_cache, kernels, vision, shard readers and their support
headers are the original Chat implementation, moved without algorithm changes.
The original Apache license applies; `CHAT_NOTICE` preserves Chat's extraction
baseline attribution notice. The shared JSON header retains the named Unicode
buffer portability adaptation described below; tokenizer/image headers match
Chat's originals. Chat delegates its standard CPU/OpenMP builds to this owner.

```sh
make qwen-engine
make qwen-omp
make test-qwen-engine-core sanitize-qwen-engine-core
make test-qwen-engine-serve sanitize-qwen-engine-serve QWEN_TEST_ASSETS=/absolute/app-assets
```

The CPU target needs only the system C toolchain and emits `build/qwen36b` with
the original O3/pthread/libm flags. OpenMP retains the original native flags and
local libomp dependency (`QWEN_OMP_PREFIX`, or explicit `QWEN_OMP_CFLAGS` and
`QWEN_OMP_LDFLAGS`). `qwen-omp-sched-runtime` retains the opt-in experimental
schedule profile and separate output name; it is not the shipping binary.
Run CPU/OpenMP targets sequentially because both retain the original output
filename. None downloads models or uses Python. Linux qualification is deferred.

The unchanged native core tests cover exhaustive/random cache traces,
repetition/thinking budgets, quantized kernels and the original legacy image
decoder smoke check. The existing serve test preserves configuration, session,
scheduling, streaming, static-file and HTTP assertions. It changes only its
owner include and explicit App asset paths; supply the caller's original
app.html and samosa-chat.png as test data. The model-context fixture is local to
this repository. Building/running Qwen itself requires no App source checkout;
the optional legacy static-page serving inputs remain explicit runtime paths.

The original CLI/HTTP options, model formats, expert cache policy and environment
variables are retained, including SNAP/TOKENIZER and context/thread overrides.
See [native source and actual-model evidence](../docs/qualification/life07h-qwen-sources/README.md).
Backend process selection/launch lifecycle and Maple source consolidation are
still separate unfinished ownership work.

## T5 summary engine

`t5_summary.cpp` is the existing Samosa Chat native encoder/decoder summarizer,
copied without implementation changes. Its framed scalar/batch protocol and
tokenizer-bounded reduction remain unchanged. The upstream example attribution
is retained in the source; the pinned ggml/llama license is in `LLAMA_LICENSE`.

Chat now delegates its summarizer build to this owner; its duplicate source is
removed. For the existing macOS dynamic-library development profile, run
`make t5-engine T5_LLAMA_DIR=/absolute/prism-checkout`
(optionally set `T5_LLAMA_BUILD`). This preserves the original development link
flags. Run the moved original protocol test with
`make test-t5-engine-real T5_TEST_ENGINE=/absolute/engine T5_TEST_MODEL=/absolute/model.gguf`.
The pinned static release build below remains independent of that profile.

Build from this repository with a separate checkout at Prism revision
`9fcaed763ccda38ea81068ad9d7f991aaddca451`:

```sh
sh tools/build_t5_runtime.sh /absolute/pinned-prism-checkout /absolute/new-private-output
```

The script verifies the revision and tracked source cleanliness, archives the
pinned tree into a new private output directory, builds static CPU libraries and
links this repository's summarizer. It does not edit the checkout or overwrite an
existing output directory. The macOS build targets 14.0. Build tools are not
end-user runtime dependencies. The Linux build branch still needs qualification.

The CPU profile disables Metal, BLAS backend plugins, OpenMP, dynamic backend
loading, native-CPU targeting and OpenSSL. On macOS it retains the OS Accelerate
framework. There are no packaged dynamic libraries or build-machine search paths.
This matches the existing public summary bridge's CPU-mode request.

`tests/t5_package_candidate.py` creates an owned qualification package from this
build, the separately supplied engine pin and the existing pinned T5 GGUF model.
It checks the build profile, model pin and observed deployment target, and records
engine/model/source provenance. Emitting a candidate does not qualify it or
publish a production distribution catalog. See
[T5 package evidence](../docs/T5_PACKAGE_INSTALLATION_VERIFICATION.md).

## Whisper CLI

Build the existing pinned whisper.cpp source archive independently of Chat:

```sh
sh tools/build_whisper_runtime.sh /absolute/pinned-source.tar.gz /absolute/new-private-output
```

The script verifies the archive checksum before extraction, creates an exclusive
private output, builds a static CPU runtime and includes the upstream license.
The supported package profile is `base.en-f16`, qualified against independent
engine and model pins. Source pins, build constraints and actual installed-model
results are in [Whisper package evidence](../docs/WHISPER_PACKAGE_INSTALLATION_VERIFICATION.md).

## VisionPsy

Chat delegates its native development builds here. Supply the existing matching
MLX checkout/build explicitly; these targets neither build MLX nor fetch models:

```sh
make vision-engine molmo2-engine molmo2-pack molmo2-model-compile \
  test-vision-engine-components test-molmo-engine-components \
  MLX_DIR=/absolute/mlx-checkout MLX_BUILD_DIR=/absolute/mlx-build
```

Outputs retain `build/samosa-visionpsy`, `build/samosa-molmo2`, `build/molmo2-pack`
and the original test binary names. Use that build's
`mlx/backend/metal/kernels/mlx.metallib` with the binaries: set `METAL_PATH` to its
directory for development, or colocate that exact library with an isolated
runtime. Never substitute a kernel library from a different MLX build.
`vision-engine-real-check` compiles the unchanged original
`tests/test_visionpsy_real.cpp` gate to `build/test-visionpsy-real`; execute it with
`MODEL_DIR RENDERED_FIXTURE.ppm` and the same `METAL_PATH`. Chat's
`make test-visionpsy-real VISIONPSY_MODEL_DIR=...` retains native PDF rendering
and runs that owner test. `sanitize-molmo-engine-components` runs the original
contract/processor tests with ASan/UBSan. Native development qualification is
for this Mac; independent release builders below retain their pinned profiles.

`visionpsy/` and the supporting `json.h`, `tok.h`, `tok_unicode.h`, and
`stb_image.h` were copied from the existing Chat engine. The shared JSON header
uses named Unicode decoding buffers so it compiles as standard C++ under GCC;
the engine implementation is unchanged. The image decoder retains its embedded
license. These headers belong
to the C++ engine and are separate from the inference service's JSON transport.

Build without a Chat checkout:

```sh
sh tools/build_vision_runtime.sh /absolute/mlx-checkout /absolute/fmt-checkout \
  /absolute/json.tar.xz /absolute/metal-cpp_26.zip /absolute/new-output
```

The build requires macOS arm64 and the Xcode Metal compiler. It verifies clean
tracked sources at MLX revision `7a1d4f5c12ac82f4b4d0a6e71538d89ca0605247`
and fmt revision `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f`, then archives
them into private build directories. Dependency archives are checked before
extraction:

| Dependency | SHA-256 |
| --- | --- |
| nlohmann/json 3.11.3 `json.tar.xz` | `d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d` |
| Apple `metal-cpp_26.zip` | `4df3c078b9aadcb516212e9cb03004cbc5ce9a3e9c068fa3144d021db585a3a4` |

CMake uses those local dependencies with FetchContent disconnected. The output
contains the native executable, colocated `mlx.metallib` and dependency licenses.
The MLX kernel fallback points to `/samosa-inference-no-fallback`, preventing
implicit loading from the build directory. Package admission separately requires
host-qualified engine, kernel and model pins. Build completion alone does not
qualify actual installed inference or a platform deployment target.

The shared source/dependency build now lives in `tools/build_mlx_runtime.sh`.
The VisionPsy wrapper invokes that helper and links only VisionPsy sources;
building VisionPsy does not build Molmo2.

## Molmo2

`molmo2/` reuses the existing native model, processor, model contract, media
helpers and framed engine protocol without implementation changes. The copied
engine includes the existing prepared `video_frames` protocol required by the
public temporal adapter. Chat now delegates its builds/tests here and retires
its duplicate visual engine sources. The original `tools/molmo2_pack.cpp` and
VisionPsy component/real tests also live here without implementation changes;
the original Molmo component tests were already present. Shared headers are reused
from Chat, with the JSON Unicode-buffer portability change described above.

```sh
sh tools/build_molmo_runtime.sh /absolute/mlx-checkout /absolute/fmt-checkout \
  /absolute/json.tar.xz /absolute/metal-cpp_26.zip /absolute/new-output
```

This wrapper independently invokes the shared pinned MLX build and links Molmo2
from this repository. It does not require building VisionPsy or a Chat checkout.
The same source revisions, archive checksums, disconnected dependency loading,
exclusive private output and disabled build-directory kernel fallback apply.
The model and processor are separate pinned package assets. macOS media helpers
link the OS AVFoundation, CoreMedia and CoreGraphics frameworks. Sources still
owns preparation/decoding for the public inference interface; callers provide
prepared pixels through that interface.

The current build profile explicitly targets the build host's macOS version.
It does not establish support for an older deployment target or Linux model
execution. Build and actual installed-model results are tracked in
[Molmo2 installation evidence](../docs/MOLMO_PACKAGE_INSTALLATION_VERIFICATION.md).


## OpenDecision text backend prerequisite

`decision/ort_nli.{h,cpp}` is the private native float16 DeBERTa tensor primitive.
It requires the public ORT API headers pinned by `decision/ort_sdk_pins.json`
and an independently byte-pinned runtime. The snapshot qualifier
`tests/opendecision_native_backend.py` builds and executes normal/sanitizer
harnesses outside the checkout using explicit SDK, runtime, model and reference
inputs. See [qualification and commands](../docs/OPENDECISION_BACKEND_VERIFICATION.md).
Native tokenization, typed decisions, framed engine dispatch and public provider
integration remain under construction; this primitive alone is not an engine
release or an installed model package.
