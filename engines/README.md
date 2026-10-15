# Native package engines

## T5 summary engine

`t5_summary.cpp` is the existing Samosa Chat native encoder/decoder summarizer,
copied without implementation changes. Its framed scalar/batch protocol and
tokenizer-bounded reduction remain unchanged. The upstream example attribution
is retained in the source; the pinned ggml/llama license is in `LLAMA_LICENSE`.

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

`visionpsy/` and the supporting `json.h`, `tok.h`, `tok_unicode.h`, and
`stb_image.h` are copied from the existing Chat engine without implementation
changes. The image decoder retains its embedded license. These headers belong
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
