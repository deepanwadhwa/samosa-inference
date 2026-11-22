# Qualified static-system engine recipe

Optional macOS ARM64 QA package, built outside the personal backend checkout.
Source checkout HEAD9fcaed763ccda38ea81068ad9d7f991aaddca451; the **binary digest**,
not that commit label alone, identifies all compiled code in this private fork.
No upstream contribution or backend source edits are involved.

```sh
cmake -S CONFIGURED_LLAMA_SOURCE -B PRIVATE_BUILD_DIRECTORY \
  -DBUILD_SHARED_LIBS=OFF -DLLAMA_OPENSSL=OFF \
  -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
  -DLLAMA_BUILD_APP=OFF -DLLAMA_BUILD_TOOLS=ON \
  -DGGML_BACKEND_DL=OFF -DGGML_METAL=ON -DGGML_METAL_EMBED_LIBRARY=ON
cmake --build PRIVATE_BUILD_DIRECTORY --target llama-server -j 6
```

The observed build cache confirms shared libraries OFF, OpenSSL OFF, backend
plugins OFF and Metal embedding ON. Direct loader inspection accepts only
system libraries/frameworks: Accelerate, Foundation, Metal, MetalKit,
CoreFoundation, libSystem, libc++ and libobjc. The original small dylib wrapper
fails system-runtime-v1. Backend and embedded shader bytes now reside inside
the pinned executable instead of externally resolved backend libraries.

Qualified binary size20513984, SHA256
1634265c6be962401fb2d2419b371b5eedef20957bc603fb3bbde62ed8389ccf.
Built/tested on macOS26.5.1 ARM64 (25F80), SDK26.5, Mach-O minimum OS26.0.
This specific artifact is not declared supported on older macOS or Linux.
The native owner itself also has independent Debian12 ARM64 fixture/build
qualification. A Linux actual engine artifact remains a separate optional gate.

Model qualification used a configured read-only GGUF,5629108704 bytes, SHA256
5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106.
The profile is optional; model-free Sources/Agent/App and default Inference
must not install this model or depend on the backend checkout.

Actual owned activation qualifies full streamed inputs, fd5 model binding,
authenticated metadata/template/count/generation, public ready status and
artifact mutation refusal, active cancellation observed through the owned
engine's slot endpoint, subsequent surviving generation, explicit stop and
offline tracked retry. Production CLI additionally qualifies private host
configuration, ephemeral credential removal and bounded FIFO EOF shutdown.
Exact logs/counts/failed attempts are in the architecture verification checkpoint.

This recipe and QA artifact are not yet a distributed installer/package catalog.
Versioned distribution must include provider license notices, acquisition source,
exact size/digest/platform/minimum OS/runtime policy and trusted recipe identity.
Remote acquisition and complete product installation/recovery remain open.

## LIFE-07v Gateway primary qualification on macOS

The existing Metal build recipe was repeated from Prism
`9fcaed763ccda38ea81068ad9d7f991aaddca451` in an isolated native build. The
build explicitly disabled OpenMP and the server's embedded UI because the
Gateway uses `--no-ui`. The pinned executable is 11,692,896 bytes with SHA-256
`544e0df94e43167ef33c2918675dafb0e2a863e070d64dc918c854e014e5dce2`.
The model is the same 5,629,108,704-byte Ornith GGUF above. Mach-O inspection
listed only system frameworks and libraries.

The former four-token qualification request returned empty final content and
four reasoning tokens on real Ornith. The original Gateway chat request with
`chat_template_kwargs.enable_thinking=false` returned final text under the same
token limit. The Inference readiness request now supplies that existing template
control for llama-server. The fixture still rejects a reasoning-only reply.

The patched owner passed 174 primary host assertions in normal and sanitizer
builds in both the module and a source-only copy. The parent Gateway on this Mac qualified actual Ornith with explicit
engine and catalog model pins, returned a four-token public chat reply and
reaped the worker and PID marker. The process tree peaked at 5,721 MB RSS
with zero swap growth and ran at a 1,024-token context with two llama threads.
The result covers the manually pinned, static-system Ornith profile. Release
provisioning and actual Bonsai, Maple and projector tests remain open.
