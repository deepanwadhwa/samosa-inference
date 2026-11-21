# Component 2 acceptance audit

Status: complete, 2026-10-17. This audit preserves all four component-2 requirements in
[the module plan](INFERENCE_MODULE_PLAN.md#2-model-acquisition-and-installation--complete).
It does not replace the remaining lifecycle, scheduling, generation/session,
authenticated service or final-release requirements.

| Requirement | Existing implementation and authoritative evidence | Current result |
| --- | --- | --- |
| Trusted catalogs using existing artifact/HTTPS/setup primitives | `provider_catalog`, `private_config`, `package_catalog`, `package_installer`; [catalog installation evidence](PACKAGE_INSTALLER_VERIFICATION.md); actual `catalog/*/remote-{normal,sanitize}.json` | All ten declared entries pass actual cold published-URL normal/SAN installation and cleanup |
| Full engine/model packages, provenance and verified acquisition | Native generic index plus existing backend descriptors/manifests; recipe-specific native compatibility checks; `tools/distribution_candidate.py` prepares metadata only | Complete independently pinned packages; all ten qualified metadata-only catalog entries retained |
| Credentials and redirects | Existing remote artifact/key/TLS tests, private source configuration, separate index/payload credentials in `llama_setup_https.py --per-file-sources --credential-isolation`; [remote cache evidence](PACKAGE_REMOTE_CACHE_VERIFICATION.md) | Shared native gates included in successful independent macOS/Linux normal/SAN CI at b08b4f7; actual package TLS/credential gates also run per distribution |
| Quota, concurrent installs and reservations | Existing artifact/cache bounds, mandatory shared package reservation pool, cancellation and contention tests; [reservations](PACKAGE_RESERVATIONS_VERIFICATION.md) | Native bounded admission/refusal and preserved state; runtime memory/residency scheduling remains component4 |
| Resumable interruption and corrupt partial state | Existing checkpoint, registered copy/intent, reservation and metadata-publication recovery; [copy recovery](PACKAGE_COPY_RECOVERY_VERIFICATION.md) | Controlled normal/SAN coverage plus actual published embedding installer SIGKILL/contention/corrupt-prefix recovery in both modes |
| Offline retry and last-good preservation | Actual source configuration removed after cold installation; native cache-only resume/removal/reinstall; wrong runtime/model policy must retain completed installation | Passed for all ten entries, including text cold normal/SAN exact replay and cache-only reinstall/removal |
| Architecture, minimum OS and declared native loader/backend compatibility before activation | Existing `package_backend_compatible`, per-recipe model binding, native Mach-O/ELF inspection and closed dependency graph; [compatibility](PACKAGE_COMPATIBILITY_VERIFICATION.md) | Current curated entries are macOS arm64 only; actual execution scope is recorded individually. Installation receipts remain explicitly unqualified for loaded readiness |
| Publish catalog entries only after actual-package gates | Production catalog directories carry exact installed-package qualifications and cold published-URL receipts | All ten entries admitted after actual-package and cold URL normal/SAN gates; exact receipts retained |

The independent source command is `sh tools/check_native.sh`; its Makefile target
closures include native normal/SAN artifact, catalog, installation, assembly,
reservation, staging, compatibility, removal and setup controls. Exact successful
hosted revision: `b08b4f7d25577199ba07a702e0b401b59fbd81d3`, run39054802760,
macOS arm64 and Ubuntu x86_64. This is native control/build coverage, not actual
Linux model execution.

Actual published embedding recovery additionally observes the owned installer
alive at a durable partial checkpoint, checks concurrent refusal, kills and reaps
that exact process, corrupts/restores only its held registered inode, and resumes
through the native installer. Its receipts are committed in the embedding catalog;
[the exact qualification](EMBEDDING_PACKAGE_INSTALLATION_VERIFICATION.md#real-url-crash-contention-and-corrupt-prefix-recovery)
records the scope. No synthetic fixture is counted as model quality.

Historical pending checkpoint, superseded by the closure below:

- Finish Ornith text cold published-URL qualification. Its exact seven-file
  installed package passes normal/SAN managed/registry quality, cancellation,
  owner isolation, shutdown and exact offline replay gates.
- Molmo2 now passes both cold published-URL modes with524778 common-registry
  checks each and complete cache/cleanup gates; exact receipts are committed in
  `catalog/molmo2-native-v1`. Text is the only remaining distribution acceptance.
- Recheck this full requirement/evidence map against the resulting current
  implementation, exact receipts and final independent CI revision. Any missing
  requirement remains open rather than being inferred from unrelated green tests.

## Current binding audit — 2026-10-17

Rechecked all nine existing production catalog directories against their actual
index/catalog/sources bytes and both remote receipts. Every index hash/size and
platform/minimum-OS/recipe tuple matches its catalog; every source map binds that
index; both normal/SAN receipts report `passed`, matching catalog/source/index
hashes and successful temporary-installation removal. File counts are Whisper6,
T5 9, embedding10, OpenDecision text16/image16, VisionPsy17, Pocket27, Kokoro375
and Molmo2 37. This validates receipt bindings, not an unrecorded new model run.
All102 stored metadata files across those nine payloads also match their
indexed byte counts and SHA256 pins; none is a symlink. Model/runtime bytes are
qualified by the existing exact installed-package and remote receipts, not
retained as source-repository weights.

Ornith text normal cold qualification now passes all original actual-model and
offline reinstall/removal/cleanup gates in
`/private/tmp/inference-text-resume-normal.json`. Its original seven-file index
pin is reproduced exactly. Sequential sanitizer collection50277 is active;
see TEXT_PACKAGE_INSTALLATION_VERIFICATION.md for the exact command/report paths.
Text remains outside the production catalog until that report passes.

Hosted run39054802760 was rechecked as completed/success atb08b4f7d25577199ba07a702e0b401b59fbd81d3.
Current tracked `src` and the installation fixture/reservation/identity tests have
no diff against that tested revision. Current reservation97 and identity61
checks also pass normally and with sanitizers. Newly extracted host-library
sources have their own independent platform tests and do not alter installation
logic. Area2 stays active pending sanitizer text qualification and final closure
record; areas3–7 retain every original requirement.

## Area2 closure — 2026-10-17

Text sanitizer collection50277 terminated0. Both exact cold receipts now reside
in `catalog/ornith-text-cpu-v1`, along with the unchanged seven-file index,
catalog/source/host policy, five pinned metadata files and qualification pins.
Both report `passed` and successful temporary-installation removal. Actual
normal/SAN logs confirm every original text quality/control scenario and full
cache-only resume/removal/reinstallation; no checkpoint or skipped prerequisite
is counted as a pass.

All four original requirements are satisfied by the table above: trusted pinned
catalogs and complete packages; credential/redirect/quota/concurrency/crash/
resume/corruption/offline/last-good controls; native platform/OS/loader/backend
compatibility before activation; and catalog admission after actual-package
qualification. Original native installation logic is unchanged from the passing
independent macOS ARM64/Ubuntu x86_64 normal/SAN CI revision, verified against
the current source. Current targeted reservation/identity gates also pass.
All ten receipt/index/catalog/source bindings and107 metadata payload files
match their pins. Curated actual-model distributions are macOS ARM64; native
Linux control/format tests do not claim Linux model availability.

Area2 is COMPLETE; area3 engine lifecycle/readiness is ACTIVE. Areas4–7, full
module completion, assembled integration and retirement retain their original
requirements. No loaded readiness, global scheduling, streaming or session
closure is inferred from installation. Existing native installers and engines
were reused; no alternative acquisition system or model conversion was added.
