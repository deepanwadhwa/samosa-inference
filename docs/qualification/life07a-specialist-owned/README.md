# LIFE-07a specialist ownership: resumed assignment

Resumed 2026-11-20. Supports C03/C04/C11/C13/C14. This assignment qualifies the
already implemented ownership adaptation of the extracted SamosaMmSession;
it does not redesign the supervisor or VisionPsy/Molmo2 inference loops.

## Existing code and boundary

- Original implementation: Chat's `src/samosa_multimodal.c`, already moved into
  this component's `src/specialist_supervisor.c` and public specialist header.
- Existing control transport: both JSON-line and BE32 framed protocols and their
  existing tests in `tests/test_multimodal_supervisor.c`.
- Existing process ownership: `src/engine_process.c` guardian, workspace/duplex
  transport and residency lease; reused by `tests/summary_owned_fixture.h`.
- Current boundary adaptation: the existing transport borrows owner endpoints;
  close retains the session/lease until the owner confirms cleanup. No engine
  algorithm or framing change is planned for this assignment.
- Existing product caller: Chat's gateway consumes the owner-built specialist
  archive via its public-header adapter and delegated build/test targets.

## Scope and acceptance

Repair the existing ownership fixture's missing POSIX spawn/environment declarations
using the same convention as `tests/test_summary_owned.c`. Run the existing normal
and ASan/UBSan ownership and supervisor checks. Cover repeated requests, both
protocols, reservation refusal, pending/retryable close, failed open, deadline,
worker crash/reaping, workspace cleanup, cache bounds and owner death with an
unrelated surviving process. Preserve assertions and operation deadlines.

Commands, from the component root:

```sh
make test-specialist-owned sanitize-specialist-owned
make test-specialist-supervisor sanitize-specialist-supervisor test-summary-process
```

No Python is permitted in runtime, tooling, qualification, tests or CI. The commands
above use existing native fixtures. Do not invoke Python-backed legacy installer
or packaging checks; their native/shell migration remains separate unfinished work.

Mac product regression adaptation: Chat's existing `tests/test_molmo2_gateway.sh`
uses Python solely to create a 30-second silent PCM16/16kHz mono WAV and a sparse
147964211-byte model fixture. Replace that generator with POSIX shell printf/dd
producing the same bytes and lengths, then run the unchanged gateway assertions
through `make test-molmo2-gateway`. This is a required Python-wrapper replacement,
not a new media engine. Remove eager NumPy/Python discovery from the parent
Makefile so native targets do not invoke Python while parsing the build file.

No visual public readiness, real-model generation, complete area 3, independent
release or production-path retirement is claimed from this fixture assignment.
Follow with reuse of the existing visual preparation/verification/readiness paths.

## Evidence status at resume

The pre-existing `macos.log` and `linux.log` are failed compilation attempts from
the earlier checkpoint. Retain them as failure evidence. New passing results must
have distinct filenames and exact source identities. User steering now requires
native Mac verification first; Docker/Linux work is deferred and does not block
this assignment. Do not start containers or VMs. Linux tests have not been rerun.

Before implementation changes, verified local source snapshots of the parent,
Inference and modified Chutni checkout were saved under
`/private/tmp/samosa-extraction-checkpoint-z6b6bwly`. Each contains base source,
working/index patches, changed files and SHA256 manifests. This is a local recovery
copy, not a commit or remote backup.

## Native Mac result — 2026-11-20

The fixture declaration repair adds `<spawn.h>` and `extern char **environ`,
matching the existing summary-owned test convention. No supervisor or engine
algorithm was changed by this repair. The existing borrowed-endpoint/close
implementation now passes its previously uncompilable fixture.

Host: macOS 26.5.1 ARM64, Apple clang 21.0.0; see `host-2026-11-20.txt`.

All following invocations terminated with exit 0:

| Scope | Command | Evidence |
| --- | --- | --- |
| Owner normal/SAN, inherited transport normal/SAN, summary regression | `make test-specialist-owned sanitize-specialist-owned test-specialist-supervisor sanitize-specialist-supervisor test-summary-process` | `macos-2026-11-20.log` |
| Fresh independent native source subset, same checks | `make -C /private/tmp/samosa-specialist-native.0ht5cI test-specialist-owned sanitize-specialist-owned test-specialist-supervisor sanitize-specialist-supervisor test-summary-process` | `independent-macos-2026-11-20.log` |
| Chat summary/runtime settings and actual gateway build | From Chat: `make test-native-summarizer-supervisor test-runtime-settings samosa-gateway` | `app-2026-11-20.log` |
| Existing Chat visual product assertions with shell-generated fixtures | From Chat: `make test-molmo2-gateway` | `molmo-gateway-native-2026-11-20.log` |

The independent subset contains only Makefile, native headers/sources and the six
selected C fixture/test files plus their shared header. It builds a fresh archive
without parent/sibling sources, cached workspace binaries or Python. Owner and
product source bindings are in `source-2026-11-20.sha256` and
`app-source-2026-11-20.sha256` respectively.

The earlier `molmo-gateway-2026-11-20.log` is an excluded sandbox failure: the
gateway could not bind its localhost test port. The successful native invocation
ran outside that sandbox, with unchanged behavioral assertions. No Linux tests
ran; Docker was briefly started before the user's Mac-first clarification, then
was unavailable at cleanup. Do not restart it for this work.

This closes the native Mac borrowed-endpoint/retryable-close fixture assignment.
Real visual generation readiness, public registry integration, full lifecycle
area 3, Python-tooling removal and complete product release remain open.

Next: reuse the gateway's existing `visionpsy_session_inspect` and
`molmo2_session_analyze_request` alongside this component's `vision.c`,
`temporal.c`, preparation/verifier and guardian paths. Extract the remaining
working visual request/response and resident host ownership, with explicit
configuration and host trace callbacks, rather than adding a new visual protocol.
