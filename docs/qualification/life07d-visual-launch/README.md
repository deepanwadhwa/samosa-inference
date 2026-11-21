# LIFE-07d: extract original visual launch and close glue

Assignment opened 2026-11-20. Baselines: Chat5d5f5fe, Inference4b35a35.
This Mac first; no Python/Docker. Full refactor goal stays active.

Move the original gateway `visionpsy_session_start`, `molmo2_session_start` and
`visionpsy_session_close` bodies into the existing specialist owner, with explicit
supervisor, session, configured paths, availability and trace callbacks. Reuse
the already extracted TextBuffer helpers and SamosaMmSession process machinery.
Keep original protocol/timeouts/frame sizes and trace/error vocabulary; keep
model selection, resource/retry policy and source preparation with their caller
until those existing implementations move. No new model/codec/scheduler.

Review found an existing cleanup defect: the gateway close adapter returns when
`session.active` is false, although a failed transport exchange marks it false
while retaining PID/supervisor ownership until close/reap. First prove that
failure using the extracted original guard, then fix only that ownership check
and retain the regression. The externally owned resident profile continues to
use typed close/retry; this legacy adapter must not absorb its lifecycle.

Acceptance: native public launch/profile/trace/failed-request cleanup checks,
existing supervisor/protocol normal/SAN, independent source archive build and
gateway summary/runtime/Molmo/VisionPsy behavior. The existing VisionPsy shell
driver embeds Python; port its assertions/fixtures without dropping coverage
before running it. Record commands, terminal results and limits below.

## Qualified result — 2026-11-21

The original launch/close bodies now live in `src/specialist_protocol.c`, exported
by its existing public header and archive. Gateway keeps thin calls with explicit
paths, availability, supervisor/session and borrowed trace callbacks. Body audit
against Chat5d5f5fe proves identical starts after those substitutions; close has
only the demonstrated retained-ownership guard correction. See source-audit.txt.
All original provider IDs, protocols, ready/request timeouts, shutdown grace,
frame sizes, trace/error names and diagnostic JSON are preserved. Selection,
larger-image/video/media semantics, Mac memory/reclaim policy and bounded retry
remain unchanged in their working caller. No engine/model algorithm changed.

The native regression first failed with the original inactive-session guard
(original-close-failure.log). Close now observes retained supervisor/PID ownership
after a failed exchange, reaps the worker and releases its slot. Both protocols
can restart afterward. Typed externally owned close/retry is unchanged.

Every final command below returned0 on macOS26.5.1 ARM64 with system SQLite and
Xcode26.5 C/C++; test-only Node23.10.0 was used for the ported gateway assertions.
No Python, Docker, VM or Linux qualification was invoked.

| Gate | Result/evidence |
| --- | --- |
| Original launch profiles, normal/SAN | Both protocols pass availability/failure, exact trace/limits, busy-peer survival, request reuse, close/reap, failed-exchange cleanup and restart; native-macos.log |
| Original protocol, normal/SAN |25 exchanges/refusals each; native-macos.log |
| Existing supervisor and owned guardian, normal/SAN | Both protocols/control/retryable close/workspace/cache/owner death pass; native-macos.log |
| Fresh independent source archive, same normal/SAN gates | All pass without parent/sibling source or borrowed object files; standalone-macos.log |
| Gateway build, original summary/runtime and Molmo integration | Pass; chat-macos.log |
| Ported full VisionPsy shell integration plus native model component tests | Pass; visionpsy-e2e-final-macos.log |
| Existing package selection/version/owner bytes/manifest checks | Pass; package-macos.log |
| Clean gateway installation and model request | Pass; installer-macos.log |

Reproduce native gates from this repository:

```sh
../../tools/without_python.sh make test-specialist-launch sanitize-specialist-launch test-specialist-protocol sanitize-specialist-protocol test-specialist-supervisor sanitize-specialist-supervisor test-specialist-owned sanitize-specialist-owned
```

From Chat:

```sh
./tools/without_python.sh make samosa-gateway test-native-summarizer-supervisor test-runtime-settings test-molmo2-gateway
./tools/without_python.sh make test-visionpsy
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_gateway_installer.sh
```

The first composite VisionPsy invocation ran its existing UI/component checks,
then exposed a stale routing expectation in the shell driver. After correcting
that test as documented below, the full shell driver was rerun successfully
with `./tools/without_python.sh sh tests/test_visionpsy_e2e.sh`; UI/component code
did not change. The final shell driver rebuilds/runs the native component tests.

Standalone procedure: copy Makefile, src/, include/, engines/json.h and the five
native test/fixture C files named in the native command, plus
tests/summary_owned_fixture.h, into an empty directory outside both checkouts.
Run the same make targets there. The first subset copy omitted the existing
owned-test fixture header; standalone-incomplete-fixture.log is excluded. After
copying that declared dependency, all targets passed. standalone-source.txt
identifies the local source candidate; source.sha256 pins changed owner inputs.

## Python removal and existing fixture drift

The shell driver's Python JSON assertions moved to
Chat/tests/visionpsy_e2e_checks.mjs. Sparse fixture creation and the fake JSON-line
worker now use Node's native fs/readline APIs. Original catalog, capability,
auxiliary-selection rejection, missing-model, trace authentication/persistence/
redaction/correlation/clear, multi-page reuse and labelled partial-retry checks
remain. This is test tooling only; no new shipped runtime dependency.

The original shell test expected an image text request to skip VisionPsy. The
current working gateway deliberately requires OCR plus visual evidence for all
images and prevents a planner from suppressing that evidence floor. An isolated
binary compiled from unchanged Chat5d5f5fe reproduces the same failure with the
ported assertions (visionpsy-baseline-failure.log); before-source.txt locates it.
The test now checks the established image floor and retains the no-specialist
text-only assertion on a separate PDF fixture. Product routing was not changed
to satisfy the stale assertion. All other workflow assertions were retained.
The initial e2e log is historical/excluded; the final shell gate passes every case.

The PATH guard refuses/records interpreter calls but is not an absolute-path
sandbox; changed source/recipes were reviewed too. Remaining Python drivers and
aggregate CI elsewhere remain migration debt. Compiler warnings from retained
MLX headers/static objects are recorded, not evidence of older Mac support.
The package/install gate still uses the pre-existing dirty Chutni dependency;
its four hashes match Chat/docs/EXTRACTION_NATIVE_PACKAGING_DEPENDENCIES.sha256
unchanged. This is not complete reproducible assembled-release qualification.

Actual model generation through the component resident owners remains the
separate LIFE-07c evidence. This mechanical legacy-profile move does not turn
its inherited handshake-only ready trace into public generation readiness.
Full Inference area3, resource scheduling, later areas, Python removal/CI,
Agent/App/assembly and legacy retirement remain open.

Next LIFE-07e: reuse/extract the shared Whisper runner, preserving text/SRT,
microphone and long-window behavior, selected model identity, exact-child
cancellation and temporary cleanup. Follow Chat's gateway lifecycle audit;
do not introduce a new server or narrow long audio to the30-second public profile.
