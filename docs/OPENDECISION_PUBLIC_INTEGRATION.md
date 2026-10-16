# OpenDecision public integration — active

This work follows the parent architecture rebuild plan's C02, C03, C04 and C13
requirements. Inference is an independent repository, currently checked out as
`components/samosa-inference`; its root Makefile, contracts, dependencies and
release qualification must work without Samosa Chat, Sources, Agent or App.
The larger rebuild proceeds Sources → Inference → Agent → App → pinned product
assembly and legacy retirement. This assignment does not close any full milestone.

## Assignment and ownership

Deliver the existing OpenDecision native text decisions through the public
Inference registry. Reuse `engines/decision`, its strict Rust decoder,
`decision_assets`, registry discovery, owner-scoped operations, admission and
`engine_process` guardian. The legacy consumer is `tools/samosa_decision.py`;
application routing rules and action policy stay with Agent. This integration is
preparatory: legacy callers are not switched or retired until replacement gates
qualify. Image decisions follow text integration as required by the module plan.

Changes belong only to this repository: `src/decision.{c,h}`, registry/config
wiring, tokenizer normalization, focused registry tests and the actual-model QA
harness. No sibling private source/state or parent build is used. Native model
execution stays native; Python is developer-only qualification tooling under the
architecture plan's 2026-10-10 runtime clarification.

Public work is `decide_text`, selecting a configured `text_decisions` provider
and a nested typed `request` matching the existing worker contract. Trusted host
configuration supplies the private flat `assets` directory and four pins
(`engine_sha256`, `runtime_sha256`, `model_sha256`, `tokenizer_sha256`). Public
requests cannot select asset paths or commands. Normalized retry identity binds
all four pins and preserves candidate order and Python-compatible state rendering.
Results carry typed `result`, normalized input digest and asset provenance.

## Current evidence and unresolved gates — 2026-10-16

- `make providers`: builds on macOS ARM64.
- `make test-registry-modalities sanitize-registry-modalities`:401 checks each,
  including descriptor/routing/refusal, equivalent state/default normalization,
  runtime-pin identity changes, NUL preservation, malformed owned payloads,
  expired deadlines and foreign-platform refusal.
- Eight locked Rust unit tests pass, including model-free complete typed result
  validation and atomic C ABI refusal. The existing C++ request and transport
  gates remain in the full shared command.
- `tests/test_decision_public.py` against the pinned flat inventory and original
  v3 model reference:all ten actual-model cases pass normally and with the
  sanitizer-built public registry. Complete typed values, shapes, ordered keys,
  exact completed replay, operation status and changed-request conflicts pass.
  Maximum numeric result error is0.000583839146, within the predeclared0.005
  tolerance. The external model worker/ORT retain their existing instrumentation;
  these public sanitizer runs instrument the adapter and guardian.
- Initial capacity qualification failed at an invented60-second default. Direct
  measurement of the existing private full8x512 case completed in34.024seconds.
  The public default remains60seconds; callers may explicitly request up to
 120seconds, matching the existing worker qualification window. The complete
  actual-model harness now requests this declared bounded allowance rather than
  dropping or weakening the capacity case.
- `make test-decision-public sanitize-decision-public` passes through the existing
  native transport fixture extended only for public adapter gates. It covers
  NUL candidate names/order, model-absent exact replay, typed result refusal,
  cross-owner/provider isolation, cancellation, deadlines, mutation refusal,
  requester-death workspace cleanup and a healthy subsequent request. Counters
  include timing-dependent readiness polls; logs currently report90 normal and
 142 sanitizer checks. This fixture uses tiny dummy model/loader files and makes
  no actual-model claim.
- Initial sanitizer-linked external fixture correctly refused with
  `unqualified_library_dependencies`:its injected compiler runtime is outside
  the production closed asset graph. The public sanitizer gate uses the normal
  external fixture, like other external model adapters; the existing raw worker
  sanitizer gate separately instruments C++ scoring/transport. No loader policy
  was weakened to admit the compiler runtime.
- The complete shared check sequence now passes on macOS26.5.1 ARM64 and
  Ubuntu24.04 ARM64 from independent source copies, with the fixes below and
  focused delta/regression gates. macOS core, combined registry401/401,
  registry CLI584/587 and speech175/175 pass; Linux core, combined registry401/401,
  CLI587/674 and speech175/175 pass. All native HTTP/TLS/setup/package,
  summary/Whisper/vision normal/sanitizer gates and real-harness compilation pass.
  No actual Linux decision model execution is claimed.
- Full-run macOS observation exposed a test race: the guardian's unlink and
  rmdir are separate syscalls, but the test stopped waiting after unlink alone.
  The test now waits for both within the unchanged10-second bound. The existing
  guardian already cleans startup-acknowledgment failure; its deterministic
  broken-reader regression passes234 checks normal/SAN on both platforms. No
  production guardian rewrite was made. Five diagnostic repetitions of public
  controls also passed before the observation fix; those repetitions alone
  were not treated as resolving the failure.
- Linux setup initially lacked Clang18 sanitizer runtime archives; installing
  the declared native sanitizer dependency allowed the gate to run. The normal
  real Rust-linked fixture then exposed explicit `DT_NEEDED` dependence on
  `ld-linux-aarch64.so.1`. The shared existing OS resolver now recognizes only
  the standard ARM64/x86-64 loader basenames, consistent with its existing
  validated `PT_INTERP` trust boundary. Absolute private paths and custom loader
  names still refuse. Engine-runtime108, library-runtime4435, speech-loader2713
  and decision-asset94 normal/SAN regressions pass on both platforms. Linux
  public decision controls subsequently pass228/247 checks; final Mac controls
  pass88/152 (poll counts vary).
- The public adapter's full typed payload is strictly decoded by the existing
  Rust implementation. The envelope parser only preserves the raw payload span,
  allowing escaped NUL without relaxing root control/routing strings. Results
  are checked against the admitted request before publication. Worker exceptions
  and unknown output are not forwarded as private error text.
- Execution currently uses the existing one-request framed-file guardian, with
  clean EOF and asset rechecks; persistent engine residency remains a later
  lifecycle requirement. `contracts/decision-v1.md` and `DEPENDENCIES.md` now
  declare the public interface, optional Rust build dependency and external
  native runtime boundary.
- User-authorized storage cleanup removed redundant build/conversion and temporary
  model copies, Bonsai and Maple. Retained real decision assets remain at
  `/private/tmp/samosa-decision-flat-xn7bui8y`, retained embedding and summary
  weights remain in their existing QA roots, and installed Molmo2 remains intact.
  Old qualification paths whose duplicate models were removed are historical;
  do not recreate extra model copies merely to reuse their old pathnames.

The shared source archive manifest is
`/private/tmp/samosa-decision-current-source-manifest.json`; macOS's earlier
independent-copy manifest is `/private/tmp/samosa-decision-public-ci-manifest.json`.
The later source archive differs only by formatting the new adapter/fixture; no
new behavior was introduced by that formatting. Terminal log scope is recorded below; no live qualification handle remains for
these gates.

## Qualification logs and pins

macOS shared prefix: `/private/tmp/samosa-decision-public-ci-current.log`
(the original failed observation is retained there).
Passing corrected checks: `/private/tmp/samosa-decision-public-macos-delta.log`.
Remaining shared gates: `/private/tmp/samosa-decision-public-macos-remainder.log`.
Final loader/adapter deltas: `/private/tmp/samosa-decision-public-macos-loader-final.log`.
Linux shared prefix: `/private/tmp/samosa-decision-public-linux-ci-resumed.log`
(the original loader refusal is retained there).
Passing final loader/adapter deltas: `/private/tmp/samosa-decision-public-linux-delta.log`.
Remaining shared gates: `/private/tmp/samosa-decision-public-linux-remainder.log`.
Every resumed/delta/remainder handle reached terminal exit0. Earlier failed logs
are not relabeled as passes; together the completed command lists cover the
shared local/hosted sequence, with focused verification of its final deltas.

Actual normal public results: `/private/tmp/samosa-decision-public-current.log`.
Actual public ASan/UBSan adapter: `/private/tmp/samosa-decision-public-real-sanitize.log`.
Both completed exit0. Executed public binary SHA256:

- Normal `72631c3db3ffa7c2c3f4958bf51db371c2f93ada5314fcd514c86efbc7a5e35d`.
- Sanitized `fb49eefbb1c02316beeb68effb70427e185e033f3207e40b5a99011a558f5539`.
- Original reference report `f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11`.
- Flat inventory `57229d7ed8dbcfddf74caa70199606b4cc786ea912f4ff32b2c5ea97613218c9`.

Worker/model/tokenizer/runtime pins match the existing asset-binding and native
backend records. The public runs precede formatting-only adapter changes and the
ELF OS-loader basename correction; final rebuilt public/loader fixture gates
cover those changes. No model computation was changed.

## Text prerequisite completion decision

The existing native text engine is now connected to the public provider registry,
with typed request/results, actual-model parity, ownership/controls, provenance,
independent builds and both-platform normal/sanitizer boundary qualification.
This closes the text-provider integration prerequisite within area1. It does not
close OpenDecision image support, area1 as a whole or any downstream module area.
Actual model availability is qualified only for the pinned macOS ARM64 profile;
Linux here qualifies the native adapter and controls. Hosted CI and the complete
release platform/model matrix retain their own exact evidence requirements.

Next: inventory and qualify OpenDecision image decisions from the existing
`ImageDecisionEngine`, reuse Sources-prepared pixels and native runtime/tokenizer
components, and preserve its independent SigLIP224 matching semantics. The
VisionPsy512 captioning model cannot substitute for this required computation.
No legacy production path or parent integration pin has been retired/advanced.
