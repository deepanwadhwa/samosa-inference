# Engine system runtime v1

Trusted host activation may require `system-runtime-v1`. After verifying exact
engine size/SHA256, Inference reads the pinned descriptor's loader metadata.
ARM64 little-endian Mach-O MH_EXECUTE and ELF64 executable/PIE are supported.
Parsing uses bounded headers, <=64KiB command/string tables and <=16KiB dynamic
table, positional reads, and file bounds. It does not invoke inspection commands.

Mach-O permits direct dylib loads only beneath /usr/lib/ or /System/Library/,
rejects path traversal, rpaths and embedded loader environment, and permits only
/usr/lib/dyld as the interpreter. ELF permits the /lib/ld-linux-aarch64.so.1
interpreter and libc/libm/libstdc++/libgcc_s/libgomp/libpthread/libdl/librt ABI
sonames; rpaths/runpaths, filters, auxiliary libraries and audit tags refuse.
Malformed metadata, unknown dependencies and unsupported architectures refuse.
Host DYLD_*, LD_*, GGML_* and LLAMA_* environment overrides refuse before engine
spawn, including environment-selected model/projector paths.

This policy declares the installed OS runtime trusted and outside the engine
artifact digest. It inspects direct loader dependencies, not arbitrary program
behavior. A pinned engine's build/provenance must separately establish that it
does not acquire plugins, shaders or model dependencies from unpinned paths.
It cannot prove arbitrary machine code never calls dlopen or opens other files.
Do not use this check alone to claim arbitrary third-party engine closure.

The intended llama-server package statically links its llama/ggml/server/common
libraries, embeds Metal shaders, disables dynamic backend plugins and OpenSSL,
and references system runtimes only. Model bytes have a separate pinned digest
and enter the engine through Inference's read-only fd5. The engine artifact
digest includes embedded backend/shader bytes. Build identity, platform and this
runtime policy belong in the provider/version descriptor; external-wrapper
versions remain explicitly unqualified.

Activation records the selected runtime policy in durable last-good config.
Qualification still requires owned lifecycle, authenticated adapter identity,
compatible alias/context and successful real provider checks. Loader inspection
does not itself establish live readiness. A failed inspection preserves prior
last-good config and cleans up only candidate-owned resources.
