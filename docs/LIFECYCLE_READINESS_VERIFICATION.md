# Engine lifecycle readiness — LIFE-01

Status: LIFE-01 qualified for managed text startup, including integrated negative
startup/last-good gates. Area3 remains active with its full original requirements.

`managed_provider.c:qualify` now reuses `llama_probe` after owned guardian,
metadata and authentication checks. The probe preserves existing token/vector
qualification and adds actual text generation through `llama_generate`, with an
eight-token output reservation. Empty output, zero generated tokens, malformed
usage and generation errors refuse qualification. The native transport retains
cancellation and deadline controls. Capability requests have a30-second transfer
bound capped by the existing120-second startup deadline and any earlier caller
deadline. No alternate engine, transport, scheduler or activation store was added.

`tests/test_llama.c` reuses the existing native HTTP fixture. New cases prove
tokenization succeeds while generation is unavailable, empty, zero-token or
malformed; each refuses the production probe. Healthy text and embedding probes,
cancelled/expired probes and healthy subsequent work also pass.

Executed evidence, 2026-10-17:

- Native adapter477 checks, normal/SAN on macOS:
  `build/test-llama`, `build/test-llama-sanitize` in the component;
  `/private/tmp/inference-life01-{normal,sanitize}.log`.
- Existing managed CLI41 checks in each mode:
  `sh tests/managed_cli.sh build/samosa-inference-managed` and the `-sanitize`
  executable; `/private/tmp/inference-life01-cli.log`.
- Existing activation failure/mutation/revision/last-good gates, normal/SAN:
  `make test-activation sanitize-activation`;
  `/private/tmp/inference-life01-activation.log`.
- Source-only independent Linux build and the same native adapter477/477,
  CLI41/41 and activation gates pass in the existing QA container. Export:
  `/host-tmp/inference-life01-linux-1yf44qxw`;
  logs `/private/tmp/inference-life01-linux{,-build}.log`.
- Actual retained Ornith runtime/model pass the existing managed/registry corpus
  with generation-qualified startup, native counting/generation, active
  cancellation, survivor, owner isolation, EOF shutdown and exact offline replay,
  normal/SAN hosts. Logs `/private/tmp/inference-life01-real-{normal,sanitize}.log`.
  Command is `tests/managed_native.sh MANAGED ENGINE ENGINE_SHA ENGINE_SIZE MODEL
  MODEL_SHA MODEL_SIZE` with `SAMOSA_TEST_REGISTRY_BINARY` and
  `SAMOSA_TEST_OFFLINE_BINARY` selecting each mode. Engine:
  `/private/tmp/inference-text-resume-inputs/llama-server`, SHA256
  `958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`,15950064 bytes.
  Model: retained `.samosa/models/ornith-9b/Ornith-1.0-9B-Q4_K_M.gguf`, SHA256
  `5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106`,5629108704 bytes.
  Actual model scope is macOS26.5.1 ARM64; external engine is a release build.

The first development build needed the existing embedding type header; that
include was corrected before these passing gates. Both repository whitespace
checks pass. No full area3 or final-release claim follows from these results.

## Integrated startup result — 2026-10-17

The existing HTTP fixture was extracted into `tests/llama_http_fixture.h` and is
shared by the original adapter suite and a small native owned-engine fixture.
`tests/test_managed_readiness.c` calls the real `managed_provider_start`,
activation persistence and process guardian. No activation or transport function
is mocked, and the fake engine is checked by the actual native loader policy.
This is model-free lifecycle evidence, not model quality.

Eight successive startup scenarios prove: healthy initial revision; HTTP503 on
generation despite successful metadata/tokenization; empty generation; zero
generated tokens; invalid usage; deadline during generation; cancellation after
the generation request is observed; and a final healthy replacement. Each failed
candidate preserves the exact last-good activation record, returns unqualified,
and is reaped. An unrelated child survives every transition. Successful starts
advance revision only1→2 across all failures.

`make test-managed-readiness sanitize-managed-readiness` passes152/152 checks on
macOS, log `/private/tmp/inference-life01-integrated-mac.log`. Independent native
Linux source export `/host-tmp/inference-life01-integrated-linux-ejvqbo31` passes
154/154, log `/private/tmp/inference-life01-integrated-linux.log`. The existing
adapter477/477 checks pass after fixture extraction on both platforms; macOS
logs `/private/tmp/inference-life01-shared-fixture-{normal,sanitize}.log`, Linux
results are in the integrated log. New targets join existing native normal/SAN
CI entry points. The fake engine uses a normal system-runtime build; production
host, activation and guardian code are instrumented in SAN gates.

No production implementation changed after the preceding actual Ornith
normal/SAN qualification. LIFE-01's integrated negative case is now closed.
Next extract the existing private managed host configuration/setup/cleanup from
`managed_main.c` into a reusable component-owned host API so common registry
lifecycle can consume the existing owner. Preserve its config validation,
credentials, arguments, activation/guardian and CLI semantics. Then finish the
original common all-provider lifecycle/readiness, crash/owner death, stale
readiness, package mutation and unrelated-process requirements; this task does
not close area3.
