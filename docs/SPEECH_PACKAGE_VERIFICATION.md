# Native speech package verification

2026-10-13, macOS ARM64. This checkpoint extends speech extraction toward area 1
and acquisition/lifecycle prerequisites; all seven areas retain their full open
acceptance rows. No public speech route, catalog installation, guardian isolation,
platform-wide compatibility, quality benchmark or legacy retirement is claimed.

`make test-speech-package sanitize-speech-package` passes 2,330 checks each. Tests
cover Pocket and Kokoro required roles/options, multiple rechecks on the same
retained descriptor, equal-content inode replacement, changed payload/manifest,
checksum refusal, writable files/directories, hardlinks, symlinks, unlisted files,
empty directories, missing files, nested alias replacement, unknown/duplicate
keys and roles, wrong platform/ABI, unsafe names, the complete 512-file boundary,
513-file/total-byte refusal, cancellation/deadlines during
verification and mutation from a control callback. Initial refusal clears the
verified flag; failed rechecks retain the earlier diagnostic observation.
The new verifier reuses controlled hashing and identity rules from existing
artifact/temporal package code. See [schema and limits](../contracts/speech-package-v1.md).

Actual package was copied from the existing local Pocket runtime/model install
into `/private/tmp/samosa-pocket-pinned.0mx98nek`. A developer-only manifest lists
all 18 regular files (265,039,183 bytes), including all native libraries and both
voice references. This observed local manifest is not a production catalog pin.
Original artifacts were not modified. Manifest SHA-256:
`4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16`.
Derived model identity:
`fbea86958d24bf60912644ff5bbe5bb2ba92f4070a7c59ba27731b0931c6ff38`.

Commands:

```
make build/test-speech-real build/test-speech-real-sanitize
build/test-speech-real --package /private/tmp/samosa-pocket-pinned.0mx98nek 4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16
build/test-speech-real-sanitize --package /private/tmp/samosa-pocket-pinned.0mx98nek 4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16
```

Both runs pass package verification, actual Caro/Stuart generation, partial-output
cancellation, same-runtime recovery and full retained-identity recheck after use.
PCM counts and hashes match [the existing actual speech evidence](SPEECH_RUNTIME_VERIFICATION.md).
External Sherpa/ORT libraries are not sanitizer-built. This proves that the
observed declared macOS ARM64 package loads and produces bounded matching PCM;
it does not prove intelligibility, every loader dependency resolution path,
minimum OS coverage, actual Kokoro or Linux support.

The private verifier checks a declared platform and ABI version. Parsing native
images, resolving the allowed full loader graph, staging immutable execution
inputs and guardian integration remain required. The successful package receipt
must never be treated as engine readiness or concurrent-write prevention.

A clean indexed-source export at
`/private/tmp/samosa-inference-speech-package-independent.x5yrfv9z` independently
built and passed package normal/sanitizer (2,330 each), speech runtime
normal/sanitizer (320 each), and actual pinned-package normal/sanitizer gates.
No parent source paths or prebuilt inference binaries were used. The module's
default `make test` also passed its existing artifact/install, guardian,
activation, runtime, managed stream, registry, catalog, operations, admission,
core and session gates with the new speech prerequisites included.
