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
