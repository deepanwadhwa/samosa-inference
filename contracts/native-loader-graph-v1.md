# Shared native package dependency resolution

`library_runtime_graph` resolves previously inspected native images against the
caller's verified, bounded closed file list (at most 512 entries). File kinds
distinguish executable, library and other payloads. Every declared native image
must be inspected by the caller before graph resolution; package bytes and
identities must be revalidated afterwards. The resolver never opens a loader,
launches code or establishes model/backend readiness.

Private dependencies must resolve to a sibling declared library. Mach-O supports
`@loader_path/NAME`, or `@rpath/NAME` with the qualified loader-origin search
profile, and checks dependency compatibility versions against the inspected
library's current version. ELF private names require qualified `$ORIGIN` search.
Escapes, missing files, executable/data targets and unqualified search paths
refuse. Trusted ELF system libraries cannot be shadowed by any sibling payload.
Request controls are checked before resolution and between nodes/dependencies.

Speech package verification now uses this resolver after inspecting every native
library and before its final package revalidation. Its existing closed manifest,
architecture/OS, environment, full hash and inode checks remain in place.
Executable packages can reuse the same resolver; no new backend recipe is
admitted by this helper alone.

Normal and ASan/UBSan runtime gates pass 4,431 checks each, including parsed
executable plus transitive library fixtures, missing siblings, wrong file kinds,
version mismatch, path escape, malformed dependency strings, cancellation and
ELF system-library shadows. Existing speech-loader gates pass 2,713 checks each.
Complete controlled HTTPS gates pass in both builds. Logs:
`/private/tmp/samosa-shared-native-graph-final.log`.

The new resolver also passes direct pinned complete-package compatibility against
actual Pocket normally and actual Kokoro under ASan/UBSan. These are static graph
checks, not new full installation/readiness claims. Logs:
`/private/tmp/samosa-shared-graph-pocket.log` and
`/private/tmp/samosa-shared-graph-kokoro-sanitize.log`.
