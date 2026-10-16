# OpenDecision installation follow-through

Assignment OD-P01 belongs to area2 of `INFERENCE_MODULE_PLAN.md`, following
the expanded area1 provider audit. This is a reuse map and acceptance task,
not a claim that installation or any later area is complete.

## Existing implementation to use

`package_install_from_file` already connects owner-private catalog loading,
catalog ID selection, host/minimum-OS admission, verified index acquisition,
verified payload acquisition, shared copy reservations, compatible publication,
cache-only status/retry and registered removal. Its catalog/setup transports and
credential/HTTPS primitives remain the entry points. Do not add another catalog,
downloader, installer, reservation store or publication journal.

The five original recipes in `package_backend_compatible` cover Sherpa
speech, GGUF, Whisper, VisionPsy and Molmo2. The GGUF recipe's separate host policy
already distinguishes text, embeddings and T5 summaries. Its existing summary
package and actual installed quality proof are in
`T5_PACKAGE_INSTALLATION_VERIFICATION.md`; no additional T5 recipe is required.
The corresponding existing installation records cover the other original model
families. These records remain bounded evidence, not a production distribution
catalog or full area2 completion.

The new missing family is OpenDecision. Native text/image execution and the
public interface already share `decision_assets`, the Rust request/result
validator, ORT runtime and the framed worker. Text/image profiles bind distinct
model/tokenizer identities, limits and result metadata. Reuse those verifiers;
do not copy their loader or inference logic into installation.

## Concrete integration and acceptance

- Add a bounded pinned OpenDecision descriptor and recipe to the existing
  package compatibility dispatch. A descriptor selects the explicit text/image
  profile and resolves the four qualified assets against `PackageIndex`.
  Independent host policy must agree; a manifest cannot qualify its own runtime.
- Preserve the existing four-file flat execution layout inside the package
  (for example `assets/`), with descriptor/provenance outside that closed subtree.
  Package-wide tree/index and native graph checks remain mandatory. Join the
  existing asset verifier to the held/named package root; do not relax flat-tree,
  owner, link, mutation, file-size or loader policies to fit metadata files.
- Feed the recipe through the current catalog installer and public setup entry
  points. Derive the existing provider's trusted assets/pins from verified
  installed identities, following the established installed-provider harnesses.
  Public requests continue to select provider/catalog identities, never paths,
  commands, host policy or credentials.
- Refuse profile mismatch, changed model/tokenizer/runtime pins, wrong roles,
  missing/extra assets, unsafe paths, foreign platform/minimum OS and unqualified
  native dependencies before compatible publication. Preserve previous good
  installations and unchanged output on refusal.
- Extend the existing controlled package fixture to cover cold acquisition,
  interruption/recovery, credentials/redirect policy, cache-only retry,
  reservation/reclamation, status and registered removal for this recipe.
  Reuse existing recovery gates rather than introducing a parallel journal.
- Qualify actual text and image packages from fresh installed file identities,
  through the existing public actual-model gates, normally and with sanitizer
  setup/provider controls. Include text capacity, image128-candidate capacity,
  complete typed parity, replay and provenance. Installation remains readiness
  `unqualified`; persistent loaded readiness belongs to area3.
- Reuse the existing local source weights and converted graphs. Stream them
  into exclusive temporary installation fixtures and clean those fixtures after
  qualification. Do not download/export another model or retain extra weight
  layouts merely to make tests convenient.

Only after these recipe/package gates and the remaining original area2 catalog,
compatibility and recovery requirements pass may area2 close. Lifecycle,
resource scheduling, sessions/streaming, authenticated service, platform release
qualification and pinned App assembly/legacy retirement retain their original
acceptance requirements in areas3–7.

## Descriptor preparation

`decision_package_descriptor.c` implements the first bounded declaration/index
binding step, using the existing JSON parser, package filename validator and
`decision_asset_name` identities. The five-field descriptor and exact four-file
subtree are specified in [the descriptor contract](../contracts/decision-package-v1.md).
Normal and ASan/UBSan descriptor gates pass406 checks each on macOS ARM64 and
Linux ARM64. Logs: `/private/tmp/samosa-decision-package-descriptor.log` and
`/private/tmp/samosa-decision-package-descriptor-linux.log`.

## Recipe and controlled setup qualification

`decision_package_verify` now compares independent host profile/revision and all
four asset pins, reuses package-wide native/tree checks and the existing flat
asset verifier, binds the asset subtree to the held package root and repeats
whole-tree/named-root checks before publishing output. Normal/SAN direct gates
pass99 checks on macOS/Linux. Synthetic assets prove static policy/graph behavior,
not ONNX loading or model readiness. The shared named-root helper was extracted
from Molmo's existing check; its72-check normal/SAN admission regressions pass.

Recipe `opendecision-package-v1` is connected to the existing compatibility
dispatch, acquisition preflight, assembly/status and public setup. The six
trusted host flags are specified in the descriptor contract. The setup duplicate
flag mask now uses64 bits to preserve all existing flags plus the new policy;
partial and duplicate high-bit flags refuse. Public request fields cannot choose
the policy. Missing policy refuses before download/cache/reservation creation.

The existing `llama_setup_https.py` gate now covers this recipe using a thin
`decision_setup_fixture.py` wrapper. Both text and image profiles pass cold
catalog HTTPS acquisition, TLS refusal, complete policy admission, immutable
publication, engine/runtime/model/tokenizer pin mismatch, profile mismatch,
last-good inode preservation, offline status/resume, complete/incomplete removal
and cache-only reinstall. Both platforms pass normally and with ASan/UBSan.
The complete setup suite also reruns every existing recipe with the shared
script; all gates pass. Logs:

- `/private/tmp/samosa-decision-setup-all-macos.log`
- `/private/tmp/samosa-decision-setup-all-linux.log`
- `/private/tmp/samosa-decision-package-admission-final.log`
- `/private/tmp/samosa-decision-package-admission-linux.log`

The sanitizer setup binary now has explicit source/header dependencies, so a
direct build cannot silently reuse an older sanitizer executable. No production
catalog entry, model download or retained weight copy was added for these
controlled fixtures. Next qualify actual fresh installed text/image inodes and
their public inference, then finish the remaining production catalog and original
area2 acceptance requirements. Installation remains readiness `unqualified`.
