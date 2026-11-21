# Registry lifecycle ownership — LIFE-03

Checkpoint 2026-10-25: LIFE-07a is in progress. The existing SamosaMmSession
now has additive request cancellation/deadline control for both original protocols,
with interrupted streams invalidated and leases retained until existing close/reap.
See `qualification/life07a-specialist-control/README.md` for transport evidence.
Next: borrowed guardian endpoints/retryable close and workspace/duplex ownership;
then original visual preparation/verification and real generation readiness.
This does not close visual lifecycle, area3 or the full repo-split goal.

Status: managed text and embedding slices qualified, 2026-10-24. The remaining
provider lifecycle work is open. Inference area3 and full architecture
C01–C14 remain open.

Behavior: trusted text/embedding registry entries may reference the existing
managed configuration. Discovery parses that configuration without state or
engine work. Public identity-only start/status/stop controls use the existing
managed host, activation store and process guardian. A persistent registry host
can generate and use existing scoped controls between lifecycle transitions.

Owner: Inference; registry_host owns stable managed hosts and configuration
backing storage. Existing activation and operation stores remain authoritative.
The public request cannot select config paths, launch arguments, owners or keys.
Managed provider IDs, admission paths, context and owner must match the trusted
managed configuration, preserving existing operation/session namespaces.

Reuse: managed_host.c config/key/launch construction, managed_provider.c actual
qualification and health, activation.c revision/last-good/residency, guardian
cleanup, registry dispatch and managed_stream.c bounded correlated transport.
This is wiring and necessary lifetime adaptation, not a new supervisor.

Included: managed host configuration/activation split qualification; reusable
deactivation retaining parsed configuration; registry lifecycle/dispatch and
optional persistent CLI transport; focused ownership/failure/regression tests.
Excluded: other-modality residency, global scheduling, network authentication,
App integration, legacy retirement and whole-area/release closure.

Acceptance: discovery creates no state; owner/identity/profile/config mismatch
refuses; unknown lifecycle fields refuse before engine work; actual native
fixture start qualifies, stop reaps, restart preserves revision semantics;
active work excludes stop/replacement while scoped cancellation remains usable;
stale/artifact-changed engines refuse readiness; failed startup keeps last-good;
EOF cleans owned engines and unrelated processes survive. Run existing managed
host/readiness/CLI and registry gates normally and under ASan/UBSan. Independent
Linux and actual-model evidence remain required before qualified closure.

No legacy path retires: these are component lifecycle integration prerequisites.

The integration's expanded crash gate exposed a retained-owner defect in the
existing extracted host: every non-null stop result retained `attempted`, even
after guardian reap and activation lease release. It made a confirmed crashed
engine permanently unreplaceable. The host now distinguishes live/unconfirmed
shutdown from confirmed terminal failure, cleans the latter while reporting its
error once, and retains parsed configuration for another start. Registry close
also releases backing storage after confirmed terminal failure, preserving the
error for its caller. This is a boundary cleanup fix; guardian/activation stop
semantics and last-good state remain authoritative.

## Text lifecycle qualification

Final native collection commands, in the component directory:

```
make test-registry-lifecycle sanitize-registry-lifecycle
make test-managed-host sanitize-managed-host
make build/samosa-inference-managed build/samosa-inference-managed-sanitize
sh tests/managed_cli.sh build/samosa-inference-managed
sh tests/managed_cli.sh build/samosa-inference-managed-sanitize
make test-registry-modalities sanitize-registry-modalities
```

macOS26.5.1 ARM64 final lifecycle260/260, host71/71, CLI41/41 pass. Independent
Debian12 ARM64 export /private/tmp/inference-life03-linux-siqynlpg passes
lifecycle261/261, host71/71, CLI41/41, with `CC=clang CURL_LIBS=-lcurl` in the
existing QA container. Different lifecycle totals reflect executable hash-read
chunks. Both platforms' nine-provider routing regression459/459 passes. Existing
readiness152/152 macOS and154/154 Linux, and registry CLI regressions also pass.
Final native handles34631/44775 and previous supporting collections are terminal0.

Actual native model command, normal then sanitized registry executable:

```
python3 tests/registry_managed_real.py build/samosa-inference-providers \
  /private/tmp/inference-life03-llama-server \
  /Users/deepanwadhwa/.samosa/models/ornith-9b/Ornith-1.0-9B-Q4_K_M.gguf \
  --engine-sha256 958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97 \
  --model-sha256 5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106 \
  --report /private/tmp/inference-life03-real-final-normal.json
```

For SAN use `build/samosa-inference-providers-sanitize` and the corresponding
sanitize report. Both pass all six scenarios: no-state discovery, actual native
generation-qualified startup/health, counting and exact answer4, engine-stopped
exact retained replay, actual restart/requalification, and EOF cleanup without
retained keys/listener. Final sequential collection16749 is terminal0. Runtime
is the unchanged published catalog engine, restored from its exact pinned URL;
the existing model was reused. No conversion or model copy was needed.

Compact logs, both actual-model receipts and source/evidence hashes are retained
in `qualification/life03-registry-lifecycle`. Full development logs are
/private/tmp/inference-life03-*.log. Early sandbox bind, nonprivate fixture,
same-port fixture, default-thread-stack, missing Linux compiler/runtime and
confirmed-crash cleanup failures are excluded, with original logs retained.
The fixture now uses SO_REUSEADDR and the production transport's4MiB thread
stack. No production assertion or deadline was relaxed. Linux uses the declared
Clang toolchain and sanitizer runtime. C hosts/guardian are instrumented; the
external model engine and Rust tokenizer remain release builds. Actual model
execution is qualified on macOS, not Linux. One post-export header delta was
ownership commentary only; the final manifest/export match all listed bytes.

Next LIFE-03 continuation: qualify embedding startup, live readiness, public
generation/control, stop/restart and replay through this same registry owner.
Reuse existing llama embedding/profiles, native fixtures and retained actual
EmbeddingGemma2 package. Other-modality common residency, global scheduling,
sessions/service, independent release and assembled App retirement stay open.

## LIFE-03E assignment — registry-owned embeddings

2026-10-24, qualified. Deliver the existing managed embedding implementation through
the same registry lifecycle, preserving public profile/model/owner/receipt
bindings. Reuse test_registry_lifecycle.c, the native llama HTTP embedding
fixture, managed qualification, registry ownership, and embedding_real.py's
original768-dimensional/four-paraphrase corpus. No new embedding engine or
installer. This is continuation of area3 and preparatory C01–C14 integration;
no legacy path retires.

Included: extend existing native lifecycle test in embedding mode, prove bad
embedding startup keeps last-good state, scoped active cancellation, readiness,
crash/mutation/restart and owner cleanup; adapt the existing actual-model harness
to own its engine through registry --session with engine-stopped replay and
restart. Exclude other-modality residency and later-area scheduling/sessions/
service/release closure. Actual-model availability remains explicitly macOS.

Acceptance: both-platform native normal/SAN lifecycle and existing text/adapter
regressions, plus actual pinned EmbeddingGemma2 normal/SAN corpus through the
registry owner, truthful readiness, offline replay, requalification and cleanup.
Commands: make test-registry-embedding-lifecycle sanitize-registry-embedding-lifecycle;
tests/embedding_real.py --registry-owned with the existing pinned assets/flags.
Record terminal results/source hashes and exact next component action here and
in the main verification checkpoint before handing off.

Final qualification: macOS normal/SAN332/332; independent Linux333/333, including
all inherited text crash/owner/mutation gates and embedding zero/nonfinite/usage/
HTTP startup refusal, last-good equality, exact stopped-engine replay, active
cancellation/deadline and healthy subsequent embeddings. The existing text
lifecycle remains260/260 macOS and261/261 Linux; native llama adapter477/477
passes both platforms after the shared-fixture change. Counters include binary
hash-read chunks. No production implementation changed for this assignment.

Actual pinned EmbeddingGemma2 owned-registry normal/SAN pass the complete
original768-dimensional/four-paraphrase corpus, context refusal, exact replay,
legacy single/stream profile/vector parity and shutdown. Added lifecycle checks
prove discovery creates no state, actual readiness, engine-stopped replay,
restart/requalification, EOF key/listener cleanup, and separate-process offline
registry replay without launching an engine. The original external-registry
harness also passes unchanged via its default mode. New mode does not capture
or copy credentials. Actual model qualification is macOS26.5.1 ARM64; Linux
qualifies native adapters/controls. Hosts/guardian use ASan+UBSan; external engine
and Rust tokenizer retain release instrumentation.

Commands (component directory):

```
make test-registry-embedding-lifecycle sanitize-registry-embedding-lifecycle
make test-registry-lifecycle build/test-llama build/test-llama-sanitize
build/test-llama
build/test-llama-sanitize
python3 tests/embedding_real.py --registry-owned \
  --registry-binary build/samosa-inference-providers \
  --binary build/samosa-inference-managed \
  --offline-binary build/samosa-inference-llama \
  --engine /private/tmp/inference-life03-llama-server \
  --model /private/tmp/inference-life03-embeddinggemma2-Q8_0.gguf \
  --model-sha256 2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135 \
  --runtime-revision 4fbc76dec51d0add466f0210855c0596589b60d4 \
  --output /private/tmp/inference-life03e-real-normal-r2
```

SAN uses the three `-sanitize` executables and a fresh output directory. Linux
uses the existing independent export with CC=clang CURL_LIBS=-lcurl. Actual model
was restored from the original catalog revision/content pin after confirming
earlier temporary installations were removed; no new model/profile/conversion.
The first restored model's0644 permissions correctly refused activation; the
fixture was fixed to0600. A mistyped trailing `-native` build target and a
relative argv0 in a manual test invocation are excluded; corrected native
collection1146 is terminal0, as are Linux58343, actual82367/20128 and regression
24331/71030. No live qualification handles remain. Both whitespace checks pass.

Compact logs, actual rankings/receipts/lifecycle/refusal/replay/parity artifacts
and exact source/evidence hashes are retained in
`qualification/life03e-registry-embedding`. All listed workspace/export bytes
match. Remaining modality residency and full area3–7/C01–C14 are still open.

## LIFE-04 — resident native T5 ownership (in progress)

Reuse the already extracted summary_process.c scalar/batch/coalescing transport
and the original engines/t5_summary.cpp model loop. Do not implement a new
summarizer or claim a one-shot process is persistently loaded. The public
summary.c path already verifies pins/owned captures/provenance through guardian
regular-file frames; preserve that behavior while adding real resident ownership.

Inspected boundary gap: summary_process currently starts a raw child with pipes,
whereas engine_process guardian's protocol mode accepts workspace regular files
and enforces a per-result file-size limit. It cannot transport successive resident
pipe requests unchanged, and the extracted summary owner alone does not retain
the existing guardian/residency lease after parent death. Necessary next work is
a bounded duplex-pipe mode in the existing guardian, using verified model/lease
descriptors and separate bounded stderr. Test pipe binding, scoped cleanup,
normal/abnormal engine and caller death, pending shutdown, unrelated-process and
residency-lease survival on macOS/Linux normal/SAN before wiring the existing
summary transport into activation/registry. Retain all old guardian modes/tests.

Owner is Inference; engine_process owns process/group/lease lifetime and the
existing summary process owns serialized framing/batch/coalescing. This is
preparatory C01–C14 area3 work; no App route retires. Later integration must
require an actual bounded warm summary before readiness, expose public lifecycle
and preserve last-good/pins/control/derived uncertainty. Other providers and all
remaining original area3 and areas4–7 requirements remain mandatory.

LIFE-04a implementation contract: add engine_process_start_bound_duplex to the
existing engine_process owner. Trusted host supplies an absolute argv and self,
existing residency lease/read-only model descriptors, read-only input FIFO and
write-only output FIFO, lifetime and stderr byte limit. Descriptors are borrowed;
caller retains its own protocol endpoints and frame/control policy. Guardian
binds model fd5, stdin/stdout separately from bounded stderr, closes its copies
of protocol ends after spawn, and retains residency until the owned group is
reaped. Kernel pipe capacity bounds buffering; this boundary does not interpret
summary frames or claim loaded readiness. Existing summary framing/result limits
remain required when its transport is wired in LIFE-04b.

Qualify repeated binary requests, stderr separation, exact model descriptor,
FIFO/access-mode refusal, engine exit/crash, blocked output, shutdown pending,
parent death/descendant cleanup, retained residency and unrelated survivors.
Reuse/extend test_engine_process.c and run its full old/new normal/SAN gates on
macOS/Linux. Preserve every existing guardian mode. This is necessary transport
adaptation for real resident T5 ownership, not a replacement process supervisor.

The new caller-death test reproduced a pre-existing guardian gap on macOS and
Linux: a stopped guardian becomes an orphaned process group when its owner dies.
The OS delivers SIGHUP/SIGCONT; the default HUP disposition terminated the
guardian before group cleanup, while a descendant retained its lease. A separate
instrumented source copy caught SIGHUP before finish_child; that diagnostic is
not acceptance evidence. The guardian now ignores HUP and follows its existing
lifeline to group kill/reap. Worker spawn attributes still explicitly restore
default HUP; the fixture returns success if HUP were ignored, so the expected
engine_failed result genuinely tests that distinction. Parent-death coverage
confirms pending stop before death and then observes group/residency release
after the OS resumes the orphaned guardian, without assuming it remains stopped.

The fixture explicitly confirms stdout backpressure with POLLOUT before killing
the caller or stopping the engine. Kernel pipes, not an unbounded relay buffer,
carry protocol data. Other cases prove three successive binary/NUL-containing
frames, fd5 model binding without the original pathname, normal input EOF,
stdout EOF without a spare guardian writer, engine crash, bounded stderr,
lifetime expiry, stop-pending lease retention and an unrelated process survivor.
All former regular-frame/capture/sized-protocol and guardian cases remain.

LIFE-04b next: adapt the existing summary_process.c lifecycle entry/exit to use
this guardian boundary while keeping its scalar/batch/coalescing transport.
Prefer explicit externally owned lifecycle hooks/protocol endpoints so the
existing host can supply activation/pinned descriptors and retain truthful
pending shutdown. Do not duplicate the framing loops or change the T5 engine.
Preserve the current App path until its replacement integration qualifies.
Before public residency claims, qualify actual warm summary, same-engine
successive scalar/batch/coalesced work, pin/owner/control/last-good behavior,
parent death and cleanup on the public registry path. Write-side as well as
read-side cancellation/deadlines must be bounded; existing blocking writes do
not establish that public guarantee. Broader area3 and all later gates remain.

### LIFE-04a qualification — 2026-10-24

`make test-engine-process sanitize-engine-process` passes the complete old/new
guardian suite: macOS525/525, native ARM64 Linux527/525 (counts include bounded
backpressure polling). No assertion or deadline was relaxed. On Linux use
CC=clang in /host-tmp/inference-life04a-linux-ohu030fo, an independent minimal
source export of the exact modified files. Both final collections88336/45684
are terminal0. The inherited initial stdout-EOF fixture incorrectly retained a
writer in its own descendant; that fixture was corrected. Original leaked
descendant failures remain excluded and diagnosed as described above.

The existing activation, managed-readiness and public-summary normal/SAN gates
also pass on macOS/Linux with the same guardian implementation. Commands:

```
make test-activation sanitize-activation
make test-managed-readiness sanitize-managed-readiness
make test-summary sanitize-summary
```

Activation checks110/116 macOS and112/168 Linux; managed-readiness152/152 and
154/154; public summary478/478 and479/503. Timing-dependent hashing/polling may
change totals, not scenarios. Summary protocol boundary gates also pass.
Collections79377/86226 are terminal0; full logs
/private/tmp/inference-life04a-final-{mac,linux}.log. Linux regressions reuse the
full independent export /host-tmp/inference-life03-linux-siqynlpg; guardian
implementation/header bytes match the workspace. The final expanded guardian
test bytes match the minimal export. Source/evidence hashes, compact terminal
logs and the explicitly excluded SIGHUP diagnostic are retained in
`qualification/life04a-resident-duplex`. Both whitespace checks pass; no live
qualification handles remain.

This qualifies the process/pipe/lease boundary, not actual resident T5 readiness,
public summary lifecycle, all-provider area3 or later-area/release requirements.
Exact next assignment is LIFE-04b above. No App route, model implementation,
installer, original framing algorithm or production integration pin was replaced.

LIFE-04b contract, qualified 2026-10-24: keep the existing summary transport and
legacy launch path; add trusted external lifecycle callbacks supplying borrowed
duplex endpoints, with typed cleanup errors and retained pending ownership.
Add controlled scalar/batch/coalesced entry points sharing the original framing.
Controls cover mutex/queue waits and nonblocking writes/reads with monotonic
deadlines and cancellation. Coalescing may join only identical control scopes;
queued cancellation removes only its own request, and already admitted protocol
interruption invalidates that process so a subsequent request cannot consume a
partial frame. Lifecycle callbacks run under the transport mutex and must not
reenter it; the host owns pins/lease/guardian and retains its context until close.

Files: summary_process.c/public header, existing fixtures/transport tests and
focused Makefile runners. Reuse guardian duplex and T5 scalar/batch/coalescing;
no engine changes, installer, network service or public readiness placeholder.
Acceptance includes same-guardian scalar/batch/coalescing, blocked input/output
deadline/cancel, queued isolation, cleanup pending, failed protocol restart,
legacy tests/App build, independent Linux normal/SAN and actual retained T5.
This is area3/C01–C14 preparation; public registry T5 lifecycle/actual warm
readiness must still qualify before its original full requirements close.

### LIFE-04b qualification

Production summary_process.c/public header now support external lifecycle
binding, retryable typed close, controlled scalar/batch/coalesced calls,
nonblocking finite I/O, monotonic queue/mutex waits and stable error results.
The original wire format, prefix, scalar/batch limits, punctuation handling,
4ms/64-frame grouping and reply partitioning remain. A queue head now coordinates
only its own batch before handing off; the former loop could keep a completed
caller draining unrelated work beyond its deadline. Distinct control objects
cannot be coalesced into a shared cancellation scope. A queued cancellation or
deadline removes only that request; an in-flight partial frame invalidates its
worker. Pending cleanup preserves endpoints/context and refuses new work.
Protocol input/output now reuses existing UTF-8 validation. Thread-local SIGPIPE
handling turns closed writers into errors without changing host signal policy.

Final macOS normal/SAN owned transport280/356 and native Linux267/272 pass;
counts include control/timing observations. Coverage includes same-guardian
scalar/batch/eight-caller coalescing, malformed output and healthy restart,
blocked-input and blocked-output cancellation/deadline, queued cancellation and
deadline without stopping an active scope, bounded mutex waits, truthful pending
shutdown/retry, closed engine pipes, caller death and unrelated-process survival.
The original resident process/transport fixture also passes normal/SAN on both
platforms. Native scopes use the production guardian and a release-built echo
fixture, not model-quality assumptions.

Actual original T5 scalar plus four-subject/number batch/coalesced corpus passes
normal/SAN through the same guardian across all requests. It asserts one owned
startup and stable guardian identity, then checks cleanup/residency release.
macOS26.5.1 ARM64 actual model scope; external engine is the exact published
catalog release build. Engine SHA256
f3748ba69924338abcabbcffea32e6f567c73b00c0c1f9d38def7a984e708ecf;
model30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c.
Restored the existing engine pin and made one private copy of the retained
hard-linked model; no conversion or model implementation changed. Test-host
callbacks do not replace production activation/pin verification/readiness.

Commands (component directory):

```
make test-summary-owned sanitize-summary-owned
make test-summary-process sanitize-summary-process
make test-summary-owned-real sanitize-summary-owned-real \
  T5_TEST_ENGINE=/private/tmp/inference-life04b-summary-engine \
  T5_TEST_MODEL=/private/tmp/inference-life04b-summary-model.gguf
```

Independent Linux export /private/tmp/inference-life04b-linux-wxuyfv9_ uses
CC=clang with identical listed source/header/test bytes. Native C transport,
host and guardian are instrumented in sanitizer gates; external model/fixture
engines retain release instrumentation. No Linux actual model claim. App
`make samosa-gateway`, original `make test-native-summarizer-supervisor`, isolated
gateway installer and all three package/PDFium tests pass. Packaging/install
staging now includes the existing private json.h used for UTF-8 validation; the
first installer gate caught this missing dependency and passed after it was
added to both manifests. Missing test declarations were also corrected before
acceptance. No assertion or deadline was relaxed.

Final native97811/74849, actual/legacy17757/44803, App/staging77402 and App
supervisor71189 are terminal0; no live qualification handles remain. Compact
terminal logs and exact component/parent source, model/engine and evidence hashes
are retained in qualification/life04b-summary-owned. Both whitespace checks pass.

Next LIFE-04c: integrate a production pinned resident T5 owner with the existing
activation/guardian and public summary.c normalization, opening/map-reduce,
provenance and receipts. Extend activation's trusted launch boundary for duplex
without duplicating its verified admission/revision/last-good store. Reuse the
new transport and existing model engine; public registry summary start must
qualify an actual bounded warm summary before ready. Preserve configured
provider/owner/operation identities and truthful uncertain coverage, then qualify
registry start/status/stop, public work/replay, failure/replacement, artifact/
owner death and scoped controls. This transport integration alone does not close
that public readiness gate, other-modality residency, full area3 or areas4–7,
C01–C14/App/assembly/retirement. Full goal remains active.

LIFE-04c active specification, 2026-10-24: reuse activation verification/store
and guardian with a trusted duplex launch hook; reuse public summary.c request
normalization/opening reduction/provenance/receipts with a resident transport
callback. Registry owns a pinned native summary host and exposes the same strict
identity-only lifecycle controls and per-provider lifetime exclusion.

Inspected requirement gap: public max_output_tokens accepts16–256 per request,
while the existing resident wire format fixes generation tokens at startup.
Ignoring the request or restricting it to128 would violate that contract.
Necessary boundary extension: a versioned bounded budget wrapper around existing
scalar/opening frames, parsed by the same T5 engine with per-request local limits
and preallocated256-token contexts. Legacy frames remain unchanged; no model,
generation algorithm, map/reduce or installer rewrite. An actual warm budget
frame must succeed before readiness, so an old engine refuses qualification.
Existing published T5 catalog remains qualified for its legacy one-shot path;
new-runtime distribution qualification remains required before production catalog
admission/release, not implied by a local native build.

Owned files: activation.c/h, summary protocol/transport and T5 frame parsing,
resident summary host, registry wiring and focused tests/contracts. Qualify
pin/config/owner isolation, exact old normalized receipts, changing16/64/128/256
budgets without reloading, opening/map/reduce, failure/last-good, stale mutation,
cancel/deadline and EOF/owner death on macOS/Linux normal/SAN, plus actual
retained T5/native-engine qualification. No release/area3/C01–C14 closure or
legacy retirement follows from a partial slice.


LIFE-04c public resident T5 ownership is locally qualified (2026-10-24).
Existing activation admission/revision/last-good store and guardian own the
worker; existing summary_process transport and summary.c normalization, opening
map/reduce, provenance and operation receipts remain authoritative. summary_host
is the lifecycle adapter, not a replacement backend. Shared owned-status checks
are extracted from managed_provider. The same native T5 engine parses a minimal
budget-v1 envelope so requests16/64/128/256 use one resident activation; legacy
frames and the generation algorithm remain intact. Actual bounded calibration
must succeed before activation commit; incompatible old engines fail closed.

Final macOS and independent Debian ARM64 Linux normal/SAN native lifecycle gates
pass11 scenarios each. Actual retained T5 macOS normal/SAN passes5 scenarios each,
including real16-versus256 output differences, unchanged activation revision,
opening reduction, exact stopped-worker/offline replay and owner isolation.
Native failure/cancellation/deadline/mutation/crash/owner-death gates preserve
last-good ownership and release only after confirmed reap. Existing activation,
managed readiness, text/embedding lifecycle, one-shot summary, controlled
transport and nine-provider registry regressions pass both platforms. Original
actual scalar/batch/coalesced corpus passes against the new engine normal/SAN.
Final lifecycle handles67823/68730 and regressions89750/56935/95323 are terminal0.
Actual-model scope is macOS; native C hosts are instrumented in SAN runs, external
model engine remains a release build. Initial build-overlap actual-model evidence
is excluded; final harness pins its CLI before/after execution.

Candidate engine SHA256:
179efc64fa559a4a5ebc5ece2cb2b70b54b0784e264af7d2c84083e0dae606a1
Model SHA256 remains:
30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c
Evidence: docs/qualification/life04c-summary-registry (component-relative).
Published legacy catalog is unchanged. Local resident qualification does not
admit a new production distribution or close area3/areas4–7/C01–C14.

User reaffirmed the purpose: split existing code into separate individual repos.
For each remaining slice, inventory the monolith/component implementation, move
or wire that implementation and its tests, and add only necessary ownership/API
boundary changes. Preserve existing behavior and licenses. Package qualification
uses existing helpers and remains a separate supporting gate; do not let new
packaging work displace the extraction. Next: audit the existing speech worker/
runtime and other modality owners against the remaining area3 checklist, select
the next extraction boundary, and retain the new T5 distribution gate as open.


LIFE-05a speech session extraction qualified, 2026-10-25.
Direct inspection found the working implementation in speech_worker.c and the
previously extracted speech_runtime.c (original gateway native Pocket/Kokoro ABI,
voice mapping and PCM generation). Extracted the worker's existing package
verification, library-graph check, options/voice selection and native open/close
into private speech_session.c/h. The original worker now consumes this helper;
its request parser, guardian ownership, PCM packets, provisional streaming,
terminal publication and public speech adapter are unchanged. The helper retains
one loaded runtime across sequential requests and owns a duplicated bound root
fd plus copied host path/pin. Package checks precede/follow generation; successful
worker finish also rechecks after native destruction as the original worker did.
No second speech engine, installer, model or public lifecycle implementation.

Focused fixture checks run in a separate child process: repeated Caro/Stuart
requests after deliberately changing the native ABI probe prove no reopen;
closing the caller's fd preserves the session's bound root; early cancellation
and package mutation expose no successful result/PCM. Failed generations require
caller cleanup before worker reuse. This private helper alone does not establish
public residency, readiness or process ownership.

Final macOS normal/SAN worker354/358, independent Debian ARM64 Linux355/357;
public speech175/175 on both platforms. Poll-related worker counts vary.
Existing runtime324/324, loader2713/2713 and package compatibility/publication
checks pass both platforms. Final qualified handles69131/62358, regressions
12075/64376 and actual11876 are terminal0; no live qualification handles remain.
Earlier session-mac/final logs are development evidence, superseded by qualified
logs after retaining the post-destruction verification boundary.

Actual public Pocket --stream passes265 checks normal and265 sanitizer on macOS,
using an isolated copy of retained local artifacts, all checked against the
existing catalog manifest before copy. Manifest SHA256:
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173
Package: /private/tmp/inference-life05-pocket-vlslz1c_
Caro78720/Stuart90240 samples retain the original exact PCM hashes, ranged reads,
streaming terminal verification and replay. C host/worker code is instrumented;
external Sherpa/ORT libraries remain release builds. This is existing public
one-shot execution via the extracted helper, not actual resident-worker proof.
No actual Linux or Kokoro claim is added. Model/library/config bytes unchanged.

Next LIFE-05b: reuse this session inside the existing guarded speech worker for
sequential requests, keeping the established bounded/provisional PCM packet
format and final hash checks. Reuse engine_process ownership/duplex controls and
existing package/root/workspace descriptors; preserve cancellation/deadline
invalidation and confirmed-reap lease retention. Qualify repeated native/actual
requests under one loaded worker before wiring identity-only public registry
start/status/stop and successful synthesis readiness. Reuse activation/store
semantics for last-good ownership and existing speech package identity; do not
replace speech.c/artifact stores, the native runtime, installer or gateway paths.
Inference area3 remains active, areas4–7/all C01–C14/Agent/App/pinned assembly/
legacy retirement remain open. The purpose remains splitting existing code into
independent repos, with only necessary ownership/API boundary changes.

Evidence: docs/qualification/life05a-speech-session.


LIFE-05b private resident speech worker qualified, 2026-10-25.
Reuses the existing speech session, native Pocket/Kokoro runtime, guarded duplex
engine_process ownership, request decoder, provisional/terminal PCM headers,
PCM packet validator and streaming hash checks. New hidden resident dispatch
reads bounded length-prefixed copies of the existing text/voice JSON requests.
The same native session serves sequential requests; each provisional cumulative
header precedes its newly emitted PCM suffix, and the final header adds no PCM.
The host reconstructs the original regular-file PCM packet and runs the existing
progress/result validators. Existing one-shot dispatch remains available.

The nonblocking pipe transfer/SIGPIPE handling is extracted from summary_process
into controlled_pipe.h and shared with speech, preserving summary errors and
control behavior. Resident host requests retain a60s ceiling plus caller control,
16KiB request/16MiB packet/45s audio bounds and bounded chunks. No public paths,
commands, voices or model rules were added. Guardian retains bound package and
residency and handles lifetime, stderr bounds and process-group/owner cleanup.
Active control/sink/protocol failure clears the result and stops/reaps the worker;
shutdown-pending keeps its process/endpoints for retry. Preflight cancellation
sends no request and leaves the worker available. A caller must serialize use.
Startup itself is not readiness; public activation qualification remains open.

Final native macOS worker493/495 and independent Debian ARM64 Linux493/495
normal/SAN pass (poll counts vary). Added tests prove one native load through
three requests, same guardian, original decoded hashes, final-only generation,
preflight versus active cancellation,200ms stalled-read deadline, malformed
native audio, crash/restart, changed package refusal, request EOF and SIGKILL
owner cleanup, released leases and unrelated-process survival.

Actual retained Pocket normal/SAN passes135 checks each on macOS: original
one-shot Caro/Stuart/cancellation plus both voices on one resident guardian,
provisional streaming, original PCM hashes, cancellation after two chunks with
worker invalidation/reap, and successful restarted Stuart generation. Caro78720
samples/22 chunks; Stuart90240/25. Existing package manifest SHA256 remains
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173;
isolated retained package /private/tmp/inference-life05-pocket-vlslz1c_. Native C
host/worker are instrumented; external Sherpa/ORT libraries are release builds.
This adds no actual Linux or Kokoro model execution claim.

Existing normal/SAN summary owned transport passes macOS274/354 and Linux267/272;
original summary-process scalar/batch/reuse/crash/recovery tests and public
speech175/175 pass both platforms. App gateway build, original native summarizer
supervisor, isolated installer and3 package/PDFium tests pass. Existing stage
manifests now include the extracted controlled_pipe.h so owner source builds
remain self-contained. Model, generation, installer and activation algorithms
were reused; no legacy paths were retired or remote catalog assets changed.

Final native19996/52645, regression93114/49614, actual53142, App82266 and
installer70797 are terminal0; no live qualification handles remain. Development
compile errors in life05b-build.log/native-mac-r1.log are excluded; final gates
retain the strict compiler flags/assertions. Linux export bytes match final source
except a subsequent API ownership comment, also copied and recorded as final
bytes. Source/evidence hashes and compact terminal PASS logs are retained in
components/samosa-inference/docs/qualification/life05b-speech-resident.

Next LIFE-05c: integrate this owner into the existing speech.c/public registry
through a trusted lifecycle callback, reusing package verification, admission,
activation revision/last-good semantics and identity-only lifecycle controls.
Require actual bounded successful synthesis before ready, retain exact
owner/provider operation receipts and provisional audio publication semantics,
and qualify failed replacement, mutation/crash/owner death, cancellation/deadline,
stop/restart, exact replay and cleanup. Do not treat a library bundle as an
executable engine or replace its loader/installer; adapt the existing activation
launch/qualification boundary for this verified multi-file native package.
New T5 production distribution gate remains open. Inference area3 and areas4–7,
all C01–C14/Agent/App/pinned assembly/legacy retirement remain incomplete.
The full goal remains active: split existing implementation and tests into
independent repos, adding only necessary API/ownership boundary changes.


## Resident speech registry adapter (LIFE-05c)

The trusted private `speech_output` entry may opt into `resident-speech-v1`:

```json
{
  "provider_id": "voice",
  "kind": "speech_output",
  "state": "/absolute/private/provider-state",
  "package": "/absolute/verified/speech-package",
  "manifest_sha256": "<separately-qualified-manifest-sha256>",
  "lifecycle": "resident-speech-v1",
  "worker_sha256": "<separately-pinned-registry-host-sha256>",
  "worker_size": 123456,
  "manifest_size": 1234
}
```

Sizes above are illustrative. Host executable and manifest must satisfy the
existing private activation input rules. The worker is the running registry
host executable, not a path chosen by an inference request. Library/model/voice
pins and closed loader graph remain in the existing speech manifest. An entry
without lifecycle retains its existing one-shot behavior. Private lifecycle
metadata is refused on unrelated provider kinds and public lifecycle requests
accept only contract_version/op/provider_id.

Discovery and lifecycle status do not install, create state or load a model.
`provider_start` verifies the host and manifest, binds/verifies the complete
native speech package and executes a bounded calibration through the same
resident transport/PCM validator before committing activation. The committed
argv records the actual host/package/pin command; the model_path identifies the
manifest. Repeated starts do not reload a healthy owned worker. Lifecycle stop
and replacement are excluded while provider requests hold the existing host
lifetime gate; operation controls stay concurrent.

The adapter uses the activation store's explicit `pinned-host-adapter-v1` policy:
engine must equal the trusted host executable, with a qualifying launch callback.
This records the already trusted application's runtime separately from external
model engines' system-runtime-v1 dependency restriction. Unsafe loader environment
variables remain refused. The speech bundle retains its original pinned library
loader verification. The host allowance cannot be applied to an external engine;
text/embedding/T5 engine policies and defaults are unchanged.

One authoritative activation EngineProcess record is borrowed by the resident
transport, avoiding copied live descriptors. Failure during warm synthesis reaps
the worker while activation retains its store/residency fence through rollback.
Confirmed cleanup releases root/manifest/package/lease; pending shutdown retains
ownership for retry. Loaded readiness also checks the guardian channel, retained
activation identities, and the complete package against its verified receipt.

`speech.c` still owns request normalization, provisional PCM callback semantics,
validated AudioStore publication and durable operation receipts. Its callback
only changes execution ownership. Stopped/missing-package receipt replay and
retained audio reads do not load the model. Existing one-shot `--stream` behavior
is preserved; correlated `--session` responses do not yet deliver incremental
resident PCM events. Completing that persistent event transport is an open
follow-up; this slice does not close area3, area5 or the full speech provider.

Native and actual qualification outcomes are recorded in the latest extraction
checkpoint and docs/qualification/life05c-speech-registry after terminal gates.


LIFE-05c public resident speech lifecycle qualified, 2026-10-25.
Existing speech.c now delegates execution to a trusted resident callback while
retaining normalization, original JSON request serialization, AudioStore
publication, provisional PCM callback semantics and durable operation receipts.
SpeechHost reuses existing activation admission/revision/last-good and guardian
ownership, speech package/loader verification and the LIFE-05b resident worker.
The transport borrows one authoritative activation EngineProcess record; no live
process descriptors are copied between owners. Warm request failure reaps through
that record while activation retains its lease/store fence until rollback.

Private speech entries optionally select resident-speech-v1 and declare separately
qualified worker_sha256/worker_size/manifest_size. The actual registry executable
and package manifest are admitted as pinned inputs; the closed speech bundle
retains its existing per-file/loader/model/reference verification. A trusted
model_argument lets the adapter record its actual package-directory argv while
model_path remains the manifest. The explicit pinned-host-adapter-v1 activation
policy requires engine==self plus a qualifying launch callback: it records the
already trusted application's runtime separately from external model-engine
system-runtime-v1. Unsafe loader environments still refuse. External engine
policy is unchanged; a test refuses applying this allowance to a different engine.
The first model-engine policy rejected host curl/sqlite/ASan dependencies; that
failed diagnostic is excluded, not used as acceptance or an external-engine waiver.

Discovery remains configuration-only. Public identity-only start/status/stop use
the existing provider lifetime gate, while operation controls stay concurrent.
Readiness requires complete bundle verification and actual bounded successful
synthesis through the existing PCM/hash validator before activation commit, then
live guardian/activation/artifact/package rechecks. Same healthy start is
idempotent. Failed qualification preserves the exact last-good tuple. Confirmed
cleanup releases owned state/manifest/package/residency; shutdown pending retains
ownership for retry. Existing one-shot entries remain available.

Final macOS and independent Debian ARM64 Linux normal/SAN public native gates
pass8 scenarios each: lazy discovery/strict controls, actual warm qualification,
both voices and original ranges/hash on one loaded worker/revision, exact
stopped-worker and missing-package replay, owner refusal, failed native ABI and
synthesis rollback, active cancellation/deadline with busy lifecycle exclusion,
package mutation and crash/restart, EOF/SIGKILL owner cleanup and unrelated survivor.
Actual macOS retained Pocket normal/SAN passes5 scenarios each, including
original Caro/Stuart PCM hashes on one worker, exact replay/offline retained reads,
observed-running operation cancellation and deadline refusal, explicit restart
and EOF cleanup. Public actual cancellation is not asserted to occur after PCM;
private actual PCM interruption/reap was separately qualified in LIFE-05b.
The harness polls the normal pre-registration unknown_operation race rather than
assuming a reserved control request runs after generation registration.

Actual manifest SHA256 is unchanged:
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173
Package: /private/tmp/inference-life05-pocket-vlslz1c_. C hosts/workers are
instrumented in SAN; external Sherpa/ORT remain release builds. No actual Linux
or Kokoro model execution is added. Existing native activation final113/121
macOS and115/171 Linux, text260/260 (Linux261/261), embedding332/332, T5 lifecycle
11/11, public speech175/175 and nine-provider459/459 pass normal/SAN on both
platforms. Resident worker/old one-shot/PCM integrity/owner cleanup regressions
also pass. Variable activation/worker counts include polling and executable size.

Final native52168/23080, actual65811, text/embedding/T5 regressions55155/26548,
activation26802/61254 and final-default21959 are terminal0. No live gates remain.
Initial host-policy and actual polling failures are excluded in life05c native/
actual logs; no production deadlines/compiler flags/PCM assertions were relaxed.
Final source/export bytes match. Compact logs, source SHA256 and derived reports
are in components/samosa-inference/docs/qualification/life05c-speech-registry.
Reports preserve generation receipts; decoded audio-range hashes/lengths replace
large base64 ranges and original report locations remain recorded.

Next LIFE-05d: qualify the existing published Kokoro package/profile through this
same resident owner and original voice/PCM quality harness, without changing its
runtime, voices, loader or installer. Then audit/extract remaining transcription,
image/video and decision lifecycle ownership from the monolith/component before
closing area3. Persistent registry-session incremental PCM delivery remains an
explicit area5 integration gate; existing one-shot --stream remains qualified and
unchanged, while --session currently returns complete receipts. Do not mistake
correlated replies for incremental generation/events. New T5 production distribution
remains open. Inference area3/areas4–7, all C01–C14/Agent/App/pinned assembly/legacy
retirement are incomplete. Full goal stays active; continue splitting existing
code and tests into independent repos with necessary boundary changes only.


LIFE-05d Kokoro on existing public speech owner qualified, 2026-10-25.
No production code/model/runtime/installer changed. The existing lifecycle
harness now accepts the original manifest-bound voice quality profile and runs
all6 Kokoro voices through the same loaded worker and activation revision.
The original public real-model harness's fresh-generation assertions were
extracted verbatim into tests/speech_quality.py and reused by both harnesses.
The published profile's1% sample-count tolerance is unchanged; actual generated
PCM is validated against its own exact hash/ranges, and retained replay is exact.
Reference hashes for variable fresh PCM remain diagnostics, as before.

Retained local runtime/model/voice/eSpeak/notice files were matched against every
published catalog manifest entry before copying into an isolated read-only QA
package.375 files including manifest; no conversion, new download or catalog
publication. Package /private/tmp/inference-life05d-kokoro-u75dyiqk;
manifest SHA256:
ebaa30beadfcc040df55621155c8cb56e839ca7ee9ce54f0ce929d7d08afbe76
Original quality profile and all model/library/data pins are unchanged.

Final actual macOS ARM64 normal/SAN public resident gates pass5 scenarios each:
synthesis-qualified start/idempotent readiness, all6 voices with original bounds
and exact range/hash verification on one worker/revision, exact stopped-worker/
missing-package replay and owner refusal, observed-running cancellation/deadline
refusal with explicit reactivation, EOF cleanup. Final profile and executable
pins are checked before/after execution. Earlier local actual passes remain
corroborating but final reports include the explicit profile-pin fence.

The original one-shot public --stream gate also passes583 checks normal and583
SAN across all6 voices, preserving byte-exact provisional stream versus retained
ranges and exact replay. Existing Pocket snapshot/isolation regressions pass265
checks per mode with original Caro/Stuart PCM hashes. The release QA snapshot
helper now includes the shared speech_quality.py with its own recorded hash;
an isolated renamed harness plus pinned helper executes actual Pocket without
repository imports. This is QA tooling reuse, not a new installer or another
published-URL qualification claim.

Native public speech lifecycle remains8/8 scenarios normal/SAN on macOS and
independent Debian ARM64 Linux. Only QA profile/pin/report handling changed after
those native gates; no production bytes changed. Final source/export bytes match.
Actual model scope remains macOS: C host/worker instrumented in SAN, external
Sherpa/ORT release builds. No Linux model execution, intelligibility/human quality
score, full area3 closure or persistent-session PCM-event completion is claimed.
Final actual14985, legacy/original52848, native30757/65616 and snapshot99157 are
terminal0; no live qualification handles remain. Compact logs, derived reports,
voice metrics, source/export/profile pins and original report locations are in
components/samosa-inference/docs/qualification/life05d-kokoro-resident.

Remaining engine audit found existing persistent NativeText decision framing,
VisionPsy load/unload JSON lines and Molmo2 framed command loops. Preserve these
implementations; missing ownership is in registry adapters. Image decisions bind
pixels at startup, and Whisper still invokes its CLI per canonical audio input;
their boundaries need explicit inspection before claiming resident reuse.

Next LIFE-06a: integrate text decisions around its existing persistent
engines/decision/main.cpp serve_requests loop. Reuse DecisionAssets open/recheck/
close and closed native loader graph, existing1MiB BE framing, guardian duplex,
controlled_pipe I/O, typed Rust result validation/normalization, operation
receipts and last-good activation. Require a bounded successful native decision
from the retained reference corpus before readiness. Do not apply the trusted
host dependency allowance to an external ORT engine, replace the engine/model,
or reimplement typed decisions. Preserve the existing image path while its
changing-image resident boundary remains open. Then continue the remaining
modality lifecycle audit/extraction against the full area3 checklist.
Inference area3/areas4–7, new T5 production distribution, all C01–C14/Agent/App/
pinned assembly/legacy retirement remain open. Full goal stays active. The
purpose remains separating existing code/tests into independent repos, with only
necessary ownership and API boundary changes.


## Resident text decisions (LIFE-06a)

A trusted private `text_decisions` entry may add
`lifecycle: "resident-text-decision-v1"`, `engine_size` and `model_size` to its
existing absolute `assets` path and four separately pinned engine/runtime/model/
tokenizer digests. The engine cap remains64MiB and text model cap1GiB. Existing
entries omitting lifecycle retain the old bounded framed-file execution. Image
entries cannot select the text lifecycle profile.

The registry owns a DecisionHost using the existing DecisionAssets closed-tree,
held-file and native loader graph verifier, activation lease/revision/last-good
store and guardian duplex transport. The existing decision engine's persistent
serve_requests loop, NativeText inference, framing, typed Rust request/result
validation and error mapping are unchanged. The callback only changes execution
ownership; decision.c still normalizes requests and publishes the original result
and provenance through the existing durable operation machinery.

Activation's trusted verify_runtime callback must verify its closed package and
bind retained engine/model identities to the already admitted descriptors before
launch. This records `closed-native-bundle-v1`, independently of the unchanged
system-only model-engine policy and pinned-host-adapter policy. Its bounded JSON
bundle_config retains all four pins and profile; bundle_identity is the SHA256
of that exact snapshot. The activation core checks JSON/hash correspondence
before acquiring state and stores the snapshot with the existing last-good row.
The adapter rechecks the complete bundle before/after inference and successful
warm qualification. No saved installation or activation claims current readiness.

Identity-only start/status/stop and the existing provider lifetime gate apply.
Readiness requires a bounded successful native typed choice before commit.
A healthy repeated start is idempotent; status checks the live guardian and all
retained inputs. Failure during qualification reaps while retaining activation's
lease/store fence through failure cleanup. Later partial transport, cancellation,
deadline or malformed typed output invalidates that worker; confirmed stop
releases all descriptors and residency, pending shutdown retains ownership.
Complete typed context_limit replies do not corrupt framing and leave the loaded
worker available. Queued control failures use the existing admission machinery
and do not stop an active peer. Exact completed operation replay remains available
after stop or with missing execution assets, without model loading.

Native and actual acceptance are recorded in the latest extraction checkpoint
and docs/qualification/life06a-decision-registry after terminal gates. Image
decisions still bind pixels at startup and require a changing-image boundary;
this text slice does not imply image residency, full area3 or later-area closure.


### LIFE-06a — public persistent text decisions, 2026-10-25

Qualified: existing persistent decision engine/typed algorithms, DecisionAssets
loader verification and guardian transport now back registry activation/readiness.
The public adapter preserves normalization, result validation/provenance and
receipts. Last-good stores all four pins/profile as a recoverable hash-bound
bundle snapshot, verified before launch. Native macOS/Linux normal/SAN8/8
scenarios and actual macOS5/5 plus all ten original reference cases pass; maximum
error0.000583839146 within unchanged0.005. Existing provider/image/transport
regressions pass. No model/engine implementation was replaced or published.

Authoritative commands, scope, diagnostics, hashes, terminal handles and actual
asset/reference paths: [LIFE-06a evidence](qualification/life06a-decision-registry/README.md).
No live gates remain. Next LIFE-06b: reuse NativeImage/image_pixels/image_backend
for changing prepared images on one loaded context, preserving existing limits
and legacy image behavior; then registry lifecycle/actual SigLIP qualification.
Remaining image/VisionPsy/Molmo2/Whisper lifecycle, areas4–7, new T5 distribution,
persistent session PCM events and all assembled milestones remain open.


## Changing-image decision residency (LIFE-06b)

Private `image_decisions` entries may select resident-image-decision-v1 with the
existing four pins and declared engine/model sizes. The image model ceiling stays
2GiB; text stays1GiB. They use the same DecisionHost asset verification, activation
snapshot and guardian ownership. The public request, prepared-RGBA identity,
alpha-discard/PIL preprocessing, typed results, reference tolerance, provenance
and durable receipts remain the existing adapter's responsibility.

The native engine's explicit --resident-images-v1 mode loads NativeImage once,
then invokes original image_pixels/image_backend/execute_request for each prepared
frame. Existing text and fixed-image argv modes remain intact. The versioned
private envelope is24 bytes: literal SMIMG001, then BE uint32 width, height,
rgba_bytes, request_bytes. Exact RGBA follows, then unchanged typed JSON. Each
axis is1..512, RGBA is exactly w*h*4 (at most1MiB), and typed JSON retains its
separate1MiB allowance. A reply retains the original BE length plus bounded JSON.
Malformed/partial envelopes terminate; complete semantic refusals preserve
framing and can recover. The host still serializes one frame at a time through
existing admission and invalidates interrupted/invalid workers before reuse.

This mode requires a newly qualified engine pin; the published legacy engine
refuses it and cannot become ready. The source-built engine candidate must pass
its own distribution qualification before production catalog admission. Existing
published legacy distributions are unchanged. Native codecs, registry ownership
and actual model scope are recorded in qualification/life06b-image-registry.


### LIFE-06b — changing prepared image decisions, 2026-10-25

The existing NativeImage/preprocessing/typed executor now serves changing frames
on one loaded context through a bounded explicit envelope. Same DecisionHost,
asset verifier, activation snapshot and registry controls; public DTO/provenance/
receipts and original image/text modes are preserved. Independent image/typed1MiB
limits and512-axis bounds remain. Native macOS/Linux normal/SAN codec21/21 and
image/text lifecycle8/8 pass. Actual macOS image20-case and text10-case original
corpora pass resident and legacy modes normal/SAN, within unchanged0.005 bounds.
The old image engine fails new-mode readiness without state/lease leakage.

Evidence/commands/provenance/terminal handles: [LIFE-06b evidence](qualification/life06b-image-registry/README.md).
New engine3054cf6a1beead8b75afa911ac6467a9c82e0b8f6bc8908931c0b2a3069c9458
is a local candidate; production distribution admission remains open alongside
T5. No model/runtime algorithms were replaced or legacy paths retired.

Next LIFE-07a reuses specialist_supervisor.c/SamosaMmSession and existing
VisionPsy/Molmo2 load/unload loops, with original verification, P6/white-alpha
preparation, reply/usage checks and guardian workspace/Metal-cache behavior.
Require actual native generation before public readiness. VisionPsy/Molmo2/
Whisper lifecycle, full area3/areas4–7, events, Agent/App/assembly/retirement remain
open; the full repo-split goal stays active. No live qualification handles remain.
