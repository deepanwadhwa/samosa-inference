# Component 2 acceptance audit

Status: active. This audit preserves all four component-2 requirements in
[the module plan](INFERENCE_MODULE_PLAN.md#2-model-acquisition-and-installation--active-production-acceptance-pending).
It does not replace the remaining lifecycle, scheduling, generation/session,
authenticated service or final-release requirements.

| Requirement | Existing implementation and authoritative evidence | Current result |
| --- | --- | --- |
| Trusted catalogs using existing artifact/HTTPS/setup primitives | `provider_catalog`, `private_config`, `package_catalog`, `package_installer`; [catalog installation evidence](PACKAGE_INSTALLER_VERIFICATION.md); actual `catalog/*/remote-{normal,sanitize}.json` | Eight production entries qualified; general text and Molmo2 distribution acceptance pending |
| Full engine/model packages, provenance and verified acquisition | Native generic index plus existing backend descriptors/manifests; recipe-specific native compatibility checks; `tools/distribution_candidate.py` prepares metadata only | Complete independently pinned packages; new text/Molmo candidates are not counted as published qualified entries |
| Credentials and redirects | Existing remote artifact/key/TLS tests, private source configuration, separate index/payload credentials in `llama_setup_https.py --per-file-sources --credential-isolation`; [remote cache evidence](PACKAGE_REMOTE_CACHE_VERIFICATION.md) | Shared native gates included in successful independent macOS/Linux normal/SAN CI at b08b4f7; actual package TLS/credential gates also run per distribution |
| Quota, concurrent installs and reservations | Existing artifact/cache bounds, mandatory shared package reservation pool, cancellation and contention tests; [reservations](PACKAGE_RESERVATIONS_VERIFICATION.md) | Native bounded admission/refusal and preserved state; runtime memory/residency scheduling remains component4 |
| Resumable interruption and corrupt partial state | Existing checkpoint, registered copy/intent, reservation and metadata-publication recovery; [copy recovery](PACKAGE_COPY_RECOVERY_VERIFICATION.md) | Controlled normal/SAN coverage plus actual published embedding installer SIGKILL/contention/corrupt-prefix recovery in both modes |
| Offline retry and last-good preservation | Actual source configuration removed after cold installation; native cache-only resume/removal/reinstall; wrong runtime/model policy must retain completed installation | Passed for eight published entries; same complete gate running for text/Molmo candidates |
| Architecture, minimum OS and declared native loader/backend compatibility before activation | Existing `package_backend_compatible`, per-recipe model binding, native Mach-O/ELF inspection and closed dependency graph; [compatibility](PACKAGE_COMPATIBILITY_VERIFICATION.md) | Current curated entries are macOS arm64 only; actual execution scope is recorded individually. Installation receipts remain explicitly unqualified for loaded readiness |
| Publish catalog entries only after actual-package gates | Production catalog directories carry exact installed-package qualifications and cold published-URL receipts | Eight entries accepted; Molmo draft and text candidate remain outside production catalog until all required gates pass |

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

Pending before closing component2:

- Finish the seven-file Ornith text candidate's exact installed normal/SAN
  managed/registry quality, cancellation, owner isolation, shutdown and offline
  replay gates, then cold published-URL qualification.
- Finish Molmo2's source-backed distribution staging and cold published-URL
  normal/SAN video and common-registry checks. Its new37-file exact installed
  package already passes both modes and 524671 bounded temporal checks each.
- Recheck this full requirement/evidence map against the resulting current
  implementation, exact receipts and final independent CI revision. Any missing
  requirement remains open rather than being inferred from unrelated green tests.
