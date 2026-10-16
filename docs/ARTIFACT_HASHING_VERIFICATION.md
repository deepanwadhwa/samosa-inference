# Artifact hashing on macOS

Sampling the actual Molmo2 installation showed repeated model verification
spending its CPU time in the portable SHA-256 compressor. macOS now uses the
system CommonCrypto SHA-256 implementation. Other platforms retain the existing
portable implementation. No additional library package is required on macOS.

The local streaming API, digest format and every artifact verification/control
point remain the same. CommonCrypto's value context preserves checkpoint copies;
updates split `size_t` lengths into its 32-bit length parameter. Defining
`SAMOSA_SHA256_PORTABLE` retains the portable implementation for comparison.

On macOS 26.5.1 arm64, the artifact tests passed 1034 checks in normal and
ASan/UBSan modes for both implementations. Existing empty, `abc`, multi-block and
million-`a` known vectors remain covered. Added checks finalize a copied prefix
context and then continue the original stream. The isolated export also passed
`make CC=clang CURL_LIBS=-lcurl test-artifact sanitize-artifact
test-remote-artifact sanitize-remote-artifact`; remote tests passed 188 checks
per mode, including verified-prefix recovery, integrity, controls and redirects.

A QA throughput probe hashed 4096 repeated 65536-byte blocks containing `0x5a`
(268435456 bytes), compiled with `cc -O2`, using monotonic elapsed time:

| Implementation | Seconds | SHA-256 |
| --- | ---: | --- |
| macOS system | 0.161511 | `d4e0d5a6082e9536f1ff4fbc69855d8b3e458328f27af8d72cb104d8e81b5bc2` |
| Portable | 1.368615 | Same |
| Python hashlib reference | Not measured | Same |

This is an approximately 8.5-fold hashing speedup on that host and input, not an
end-to-end installation benchmark. Full local CI and actual-model qualification
remain separate gates. Existing running qualification binaries were preserved.
