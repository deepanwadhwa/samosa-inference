# Managed host ownership — LIFE-02

Status: extraction qualified, 2026-10-17. Area3 remains active.

The existing private config loader, schema validation, embedding-profile checks,
state binding, ephemeral credential creation/removal, native argument builder,
activation startup and cleanup moved from `managed_main.c` to `managed_host.c`.
The CLI retains signals, single/stream request handling and public output.
No model implementation, transport, installer or supervisor was replaced.

The internal owner API is declared in `managed_host.h`. Its stable host object
owns the configuration strings/arrays/profile backing the existing managed
provider. Cancellation/context are explicit caller inputs. Startup returns typed
errors; close uses the existing managed owner and retains backing storage if
shutdown cannot be confirmed. This lifetime rule is necessary for reuse outside
one CLI stack frame. Start only a fresh/closed object; join users before close.

Normal/SAN evidence:

- Existing private-config/credentials CLI41/41 checks pass on macOS and in the
  independent Linux source export `/host-tmp/inference-life02-linux-pdt89j1b`.
  Commands: build both managed executables, then run `sh tests/managed_cli.sh`
  against each. Existing integrated managed startup/rollback gates also pass in
  that export. Logs `/private/tmp/inference-life02-{cli,linux}.log`.
- The original actual Ornith managed/registry harness passes with both extracted
  host executables: native counting/generation, active cancellation, survivor,
  owner isolation, EOF shutdown and offline exact replay. Command and engine/model
  pins are the LIFE-01 command in LIFECYCLE_READINESS_VERIFICATION.md, selecting
  current managed binaries. Logs `/private/tmp/inference-life02-real-{normal,sanitize}.log`.
- Actual EmbeddingGemma2 cold installed-package normal/SAN gates pass through the
  extracted host:768 dimensions,4/4 rankings, context refusal, offline replay,
  single/stream parity, shutdown and cache-only resume/removal/reinstallation.
  Both reports confirm temporary installation removal. Command:
  `python3 tests/provider_distribution_remote.py --candidate catalog/embeddinggemma2-cpu-v1 --profile embedding`
  with existing setup/providers/managed/offline native binaries and
  `--harness tests/embedding_real.py`, switching all four executables to their
  sanitizer builds for SAN. Logs/reports:
  `/private/tmp/inference-life02-embedding-{normal,sanitize}.{log,json}`.
  Model/runtime pins and representation are unchanged from the qualified catalog.
  Actual model platform is macOS26.5.1 ARM64; engines remain release builds.

Compact logs, both embedding receipts and source/evidence hashes are retained in
`qualification/life02-managed-host`. Both repository whitespace checks pass.
Two leftover CLI error-print calls were corrected to typed errors during the
initial extraction build, before the passing results above.

Next LIFE-03: wire existing managed ownership into common registry lifecycle
using this API. Inspect `registry_host.c`, `registry.c` and `registry_main.c`;
only trusted private configuration may select managed configs/launch inputs.
Preserve provider IDs, owner namespaces, Sources transports and exact retries.
Use the existing guardian and activation store. Public lifecycle controls must
expose actual start/qualification/status/stop behavior and truthful failures.
Then extend the original lifecycle acceptance to the remaining configured
modalities. Text/embedding host extraction alone does not close area3 or any
of areas4–7, assembled integration or legacy retirement.
