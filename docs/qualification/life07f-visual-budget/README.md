# LIFE-07f: extract the existing visual budget and reclaim wait

Opened 2026-11-21. Baselines: Chat ce06f94, Inference4ad140d. Mac first;
no Python, Docker/VM or new scheduler. Full refactor remains unfinished.

Reuse the gateway's vision_available_memory_gb, vision_memory_pressure,
vision_total_memory_bytes, vision_thermal_pressure, vision_resource_budget and
vision_wait_for_molmo_memory bodies. Move the original resident-estimate/reserve
arithmetic from molmo2_required_available_gb into the same owner, retaining the
caller's existing manifest read and fallback behavior. Expose explicit telemetry,
stop/clock/sleep and trace inputs; native defaults use the original host probes.
Keep turn/model selection, pause/resume and lower-side retry unchanged.

Preserve all decimal-GB thresholds,16-GiB profile behavior, detail override,
unknown telemetry, original trace fields,100-ms polling and3-second deadline.
This is shared ownership of the working policy, not global reservations,
fairness or replacement of its existing resource algorithm.

Acceptance: deterministic native normal/SAN threshold/trace/unknown-telemetry,
reserve and wait recovery/pressure/stop/deadline cases, actual Mac telemetry
smoke check, mechanical source audit, fresh source/archive build and original
gateway Molmo/VisionPsy behavior and native package/installation checks.
Record terminal results and exact remaining area3 work below.

## Qualified result — 2026-11-21

`vision_budget.c` and public `vision_budget.h` now own the original four host
probes, visual policy, bounded reclaim wait and resident-estimate/reserve
arithmetic. Gateway uses the owner type and thin stop/trace adapters. Its manifest
reader calls the extracted arithmetic and preserves both original fallback
values. The16-GiB text-backend pause/resume and lower-side retry remain unchanged.
The existing archive and release source/installer assembly include the new owner.

`audit-extraction.mjs` compares recoverable Chat ce06f94 and proves the four
native probes, complete budget/wait bodies, default monotonic/sleep helpers and
reserve arithmetic are preserved after the documented type/sample/control/
trace substitutions. The host manifest read/fallback is also checked. See
source-audit.txt. No memory policy, threshold, engine or model was redesigned.

All final commands below returned0 on macOS26.5.1 ARM64. No Python, container,
VM or Linux qualification was invoked. The live sample observed this Mac's
17179869184 bytes of RAM; available-memory/pressure readings are observations,
not fixed expected machine state or reserved resources.

| Gate | Result/evidence |
| --- | --- |
| Native normal/SAN policy |25 boundary cases, exact trace fields, decimal-GB reserve/fallback, recovery, unknown telemetry, critical pressure, stop and30-poll/3-second deadline pass; native-final-macos.log |
| Native host probes | Same tests call actual Mac telemetry/default policy; native-final-macos.log |
| Independent source/archive, normal/SAN | Pass outside both checkouts using copied source/declarations only; standalone-macos.log |
| Gateway build + existing summary/runtime/Molmo workflow | Pass; chat-macos.log |
| Existing full VisionPsy workflow + native components | Pass, including image/text routing, authenticated correlated traces, multi-page reuse and labelled partial recovery; visionpsy-macos.log |
| Exact release source/manifest/version selection | Pass; package-macos.log |
| Clean native gateway installation | Pass; installer-macos.log |

The deterministic wait cases use explicit virtual clock/sample hooks, preserving
the original100-ms poll/3000-ms loop and checking deadline/stop/peak/trace behavior
without applying pressure to the Mac. Default clock/sleep bodies are audited
against the original. Unknown telemetry and the original detail override are
preserved, including their interaction with thermal refusal. Returning a current
budget after timeout does not independently admit a model; the original caller
still checks its required-memory threshold.

Reproduce from Inference:

```sh
../../tools/without_python.sh make test-vision-budget sanitize-vision-budget
../../tools/without_python.sh node docs/qualification/life07f-visual-budget/audit-extraction.mjs
```

For the independent gate, copy Makefile, src/, include/, engines/json.h and
tests/test_vision_budget.c into an empty directory outside both repositories,
then run the two native targets there. No parent source/object, model, Rust
archive or MLX build is consumed. standalone-source.txt locates the local copy.
The archive's existing unrelated native objects are built from that copy too.

Reproduce from Chat:

```sh
./tools/without_python.sh make samosa-gateway test-native-summarizer-supervisor test-runtime-settings test-molmo2-gateway
./tools/without_python.sh sh tests/test_visionpsy_e2e.sh
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_gateway_installer.sh
```

The guard refuses/records PATH-based Python calls, not absolute paths; changed
source/recipes were reviewed as well. An initial audit-parser attempt matched a
call in a conditional rather than a function definition; its return-type matcher
was corrected and the final comparison passes. No production algorithm changed
to satisfy the audit. Earlier native-macos.log precedes a test-only replacement
of struct-padding comparisons with field comparisons; native-final-macos.log
is the final acceptance run.

The current Chutni worktree's four pre-existing modifications remain unchanged
and match Chat/docs/EXTRACTION_NATIVE_PACKAGING_DEPENDENCIES.sha256. Passing this
assembly subset does not establish a completely pinned clean dependency release.
Full Inference release/Python-free default CI remain open.

## Remaining ownership audit

Chat/docs/INFERENCE_AREA3_REMAINING_AUDIT.md compares all four area3 requirements
with actual owner/registry/legacy code. Common managed/resident profiles exist,
but legacy primary-backend state, default launch paths, Whisper guardian/common
control consolidation and full supported-path qualification remain incomplete.
Resource observation does not close global reservation/fairness duties in area4.

The next concrete extraction is LIFE-07g: delegate the existing Mac VisionPsy/
Molmo2 builds/tests to their already extracted owner files and retire proven
parent duplicates. Direct comparison finds all13 engine sources/headers and
three support headers identical; engines/json.h has the already documented
Unicode-buffer portability change. Preserve existing native dependencies,
output names, compiler flags, media frameworks, kernel pairing and actual behavior.
Do not require a new model or algorithm. The audit then orders primary-backend,
Whisper guarded execution and shared coordination work. Full area3/areas4–7,
Python tooling/CI, Agent/App/assembly/retirement and deferred Inkling remain open.
