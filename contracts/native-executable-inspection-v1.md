# Native executable installation inspection

`library_runtime_inspect_executable` shares the bounded loader parser and observed
host policy with library inspection. It inspects a held native file without
executing it, contacting the loader or loading a model. The library entry point
retains its original shared-image requirements.

The supported executable profiles are thin 64-bit little-endian Mach-O and
dynamic ELF on the declared x86-64 or arm64 host architecture. Mach-O requires
one macOS minimum-version declaration, one `/usr/lib/dyld` interpreter command,
and one bounded nonzero `LC_MAIN` entry offset; dylib identity commands refuse.
That offset must resolve to exactly one executable file-backed segment.
ELF supports ET_EXEC or PIE with a nonzero entry and exactly one architecture-
appropriate trusted system interpreter. SONAME declarations refuse.
The ELF virtual entry must likewise belong to one executable file-backed load
segment; an entry in non-executable data, outside a load segment or in an
ambiguous overlap refuses. Static ELF,
fat Mach-O, legacy entry profiles and custom interpreters are unsupported.

The same dependency-count, table-size, origin-search, macOS/glibc-version and
unsafe loader-directive checks remain enforced. Dependencies are observations:
the caller must bind them to a fully pinned closed package graph before package
publication. Parsing success is not runtime ABI, backend/model compatibility or
readiness. No new package recipe is admitted solely from this parser extension.

Normal and ASan/UBSan runtime gates pass 4,411 checks each, including executable
and PIE acceptance, architecture/OS mismatch, custom or missing interpreters,
missing entry declarations and library/executable type separation. Existing
speech-loader gates pass 2,713 checks each. Log:
`/private/tmp/samosa-executable-entry-mapping.log`.
