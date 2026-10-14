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
