# Existing Kokoro model qualification

The existing Chat assets under `~/.samosa/voice/kokoro/{model,runtime}` were copied
into a complete private closed package, preserving every regular file, including
licenses and all eSpeak resources. Original assets were not modified. The host's
`ready` marker is not runtime/model content and was not used as readiness evidence.

Package: `/private/tmp/samosa-kokoro-pinned.s5_d_czq`,364 payload files,
218280879 bytes, manifest pin
`8d3ce27844a7147201dbb0520d7407d861de52b9c729e7140175484a7a848aa7`,
model identity
`7f4e36f900cbdd1b715ae704cdae3a54a62467955bf3737d214be8006c380ada`.
The package pins the existing int8 ONNX model, voices, tokens, complete eSpeak
resources and four native libraries. The loaded Sherpa ABI is verified as1.13.4.

The first complete-package run refused because eSpeak includes a literal `!v`
directory and a `Mr serious` filename. The verifier now preserves those exact
native names only for direct pinned `kokoro_data` files under
`<data_dir>/voices/!v/`. Tests reject traversal, nested variants, incorrect roles,
other space-containing names and altered `!v` spelling. Directory traversal still
uses retained descriptors and no-follow checks; the entire tree remains pinned.
`make test-speech-package sanitize-speech-package` passes2423 checks each.

The existing actual-model native harness now accepts both speech kinds. Run
`build/test-speech-real --package PACKAGE MANIFEST_PIN` and the corresponding
`-sanitize` binary. Both pass native loader graph/host policy, package verification,
all six voices at24kHz, bounded nonzero PCM, callback/final receipt hashes,
cancellation after partial PCM, subsequent generation and retained package recheck.
The phrase is “The meeting starts at nine. Please bring your notes.”

Fresh Kokoro executions differ slightly in sample counts and PCM hashes. Observed
normal/sanitizer counts were Bella55591/55592, Sarah59112/59129,
Nicole67188/67194, Adam56676/56677, Michael59983/59979 and Emma60158/60181.
These are observations, not a deterministic-output guarantee. Reference counts
and hashes are retained in `tests/profiles/kokoro-macos-arm64-v1.json`.

The optional public actual-model harness accepts this profile as its fourth
argument after CLI, package and pin. It checks24kHz, a1% sample-count regression
bound, exact byte count, nonzero ranged PCM and its exact per-request receipt hash,
then exact stored replay/status. Both normal and sanitizer public builds pass153 checks across all six voices.
Regression gates also pass public speech91/91, owned worker324/324 and loader
2713/2713 checks in normal/sanitizer builds.
The tolerance applies only to fresh model output;
retrieval and replay require exact bytes. This is execution/integrity and duration
regression evidence, not listening or intelligibility qualification.

Module/harness sanitizer builds do not rebuild external Sherpa/ORT libraries.
Actual Linux/x86-64 Kokoro execution, listening quality, authenticated service,
persistent readiness/model residency and legacy Chat replacement remain open.
