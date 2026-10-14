# Native package assembly evidence

The native assembly API connects whole-source verification, a package-wide
assembly lease, bounded verified per-file staging and concrete speech completion
publication. The controlled fixture checks copy-ceiling refusal before creating
payload, concurrent assembly refusal, retained verified-file inode preservation,
completion revalidation/idempotency, cancellation and refusal to repair a corrupted
completed payload. Normal and ASan/UBSan fixture runs passed on macOS arm64.

Actual Pocket and Kokoro assembly passed against fresh destinations in normal
and ASan/UBSan runs on macOS arm64. These checks copy acquired native assets; they do not
run model inference, qualify actual models on Linux, or supply acquisition,
shared reservation or crash recovery. The original acquired roots are read only;
corruption checks modify only disposable copies owned by the harness.


| Acquired package | Indexed files | Declared payload bytes | Pinned generic index SHA-256 |
| --- | ---: | ---: | --- |
| Pocket | 19 | 265042042 | `ce8bdc28ae0dbd8923b792669d595039c599547a8cfadf8accc3804accde1673` |
| Kokoro | 365 | 218337007 | `068182b8a7de69cd9cbda4d2ac7436e9f1650a35de8d9ebec4bc8175037f3f75` |

These indexes retain the previously pinned native manifest identities and use
`minimum_os: 1.0` as a harness declaration, not a production catalog claim. Each
run verifies the original acquired closed tree, resumes one retained destination
file without replacing its inode, copies all remaining files, invokes native
manifest/graph/host inspection, publishes completion, repeats assembly and checks
cancellation and refusal to repair deliberate corruption in the disposable copy.
The harness deletes its own copied destinations after the checks. Logs were
retained locally as `/private/tmp/samosa-assembly-{pocket,kokoro}-{normal,sanitize}.log`.
Clang static analysis of `src/package_assemble.c` completed without diagnostics.

Independent CI run 38589615774 passed macOS arm64 and Linux x86_64 for preceding
revision `330baf7` (connected fixture publication). The new assembly revision's
independent CI must be checked separately before moving Chat's pin again.


## Direct artifact-store assembly

The fixture additionally acquires every indexed file with `artifact_install`,
then supplies held cache descriptors through `artifact_open_verified` to a fresh
package assembly. No intermediate closed-tree copy is needed. The fixture checks
successful assembly/revalidation, corrupted-cache refusal for a new package
without a completion record, and independent verification of the previously
completed package after that cache corruption. Normal and ASan/UBSan fixture
runs passed on macOS arm64. The native install suite also passed with 2349 normal
and 2351 sanitizer checks, including held read-only output, cancellation and
corrupt-input refusal with untouched output.

The cache bridge does not change shared disk reservation or transaction recovery
requirements. This turn's controlled gates do not establish actual-model Linux
qualification or model readiness.


Clang analysis of the modified assembly code emitted no diagnostics. Analysis of
`install.c` reported a possible `fstat(-1)` in the existing local-acquisition
post-read branch (line 238): that branch normally follows successful source open
and requires an unchanged trusted acquisition configuration. The runtime suites
pass; this analysis result is not a clean whole-install static-analysis claim.


Actual Pocket cache-to-package checks passed in normal and ASan/UBSan runs with
19 indexed files and index SHA-256
`ce8bdc28ae0dbd8923b792669d595039c599547a8cfadf8accc3804accde1673`.
Both runs first verified the acquired native source, installed all indexed files
into the artifact store, assembled a second package directly from verified cache
descriptors, refused a corrupted cache for a new package, and revalidated the
already completed package independently. Original assets were unchanged; all
cache corruption occurred in harness-owned disposable stores. Logs:
`/private/tmp/samosa-cache-assembly-pocket-{normal,sanitize}.log`.
This cache revision was not separately exercised against actual Kokoro assets;
Kokoro's preceding closed-tree assembly evidence remains as recorded above.

Independent CI run 38590513601 passed macOS arm64 and Linux x86_64 for `68ca629`
(leased assembly), now pinned in Chat. The direct-cache revision requires its own
CI outcome before that pin moves again.


## Production local cache acquisition

The controlled 11-file native fixture now uses production `package_cache_open`
for the direct cache path and assembles a further fresh package through actual
local cache misses. The adapter invokes the existing artifact installer for every
missing indexed identity, then passes held verified descriptors to assembly.
Checks cover early cancellation with no store creation, local acquisition,
cache hits after the configured local source becomes unavailable, completed
package verification, corrupted-cache refusal and retained-copy behavior.
Normal and ASan/UBSan fixture runs passed on macOS arm64; log:
`/private/tmp/samosa-package-local-acquisition.log`. Static analysis of the new
cache adapter emitted no diagnostics. These are controlled native-format fixture
checks, not a new actual-model or Linux qualification claim. Previous actual
Pocket direct-descriptor results remain tied to their prior revision.


## Controlled final artifact hashes

Installation now uses controlled artifact verification for both completed staged
payloads before publication and verified offline installation retries. Status and
executable-permission inspection retain their existing read-only inspection
behavior and do not inherit an install cancellation hook. A targeted local test
finishes four copy chunks, cancels on the second final-hash read, verifies that no
artifact was published, and resumes successfully. A separate installed retry
cancels during hashing with the source removed; completion/status remain intact.
This resolves the final-hash control gap recorded in the preceding local adapter
checkpoint. Blocking filesystem calls still have platform latency.

Local macOS arm64 gates passed:

- Native installation: 2366 normal / 2368 sanitizer checks.
- Package dispatch and 11-file local/cache assembly fixture: normal and sanitizer.
- Remote artifacts: 167 checks in each normal and sanitizer run.
- Activation: 110 normal / 118 sanitizer checks.
- Artifact public CLI: 26 checks.

Logs: `/private/tmp/samosa-controlled-install-hash.log` and
`/private/tmp/samosa-controlled-install-integrations.log`. This change concerns
acquisition control rather than model inference; it does not close the larger
module qualification or package reservation/recovery requirements.
