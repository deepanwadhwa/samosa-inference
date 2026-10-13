# Private native speech package v1

Host-side package verification contract; not public ingress, an installer, or
engine readiness. Public callers never select a root, manifest pin or file path.
`SpeechPackageReceipt` is a retained observation, not an immutable snapshot.

The host supplies an open root directory descriptor, its canonical absolute path
and a trusted lowercase SHA-256 of `manifest.json`. The root and every descendant
must belong to the current effective user and deny group/other writes. Regular
files must have one hardlink. Symlinks, special files, unlisted files and unneeded
empty directories refuse. The root path must resolve to the bound descriptor.
Nested directories are opened relative to retained parent descriptors with
`O_NOFOLLOW`; identities are checked before and after traversal.

Manifest schema (unknown keys, duplicate keys and unknown roles refuse):

```json
{
  "format": "samosa.sherpa-speech.v1",
  "abi_version": "1.13.4",
  "platform": "macos-arm64",
  "kind": "pocket",
  "threads": 2,
  "files": [
    {"name": "runtime/lib/libsherpa-onnx-c-api.dylib", "role": "library", "bytes": 4102752, "sha256": "<trusted lowercase SHA-256>"}
  ]
}
```

The snippet is schematic: a valid Pocket package requires exactly one file for
each of `library`, `pocket_lm_flow`, `pocket_lm_main`, `pocket_encoder`,
`pocket_decoder`, `pocket_text_conditioner`, `pocket_vocab`, `pocket_token_scores`,
`reference_caro`, `reference_stuart`. Kokoro instead requires `library`,
`kokoro_model`, `kokoro_voices`, `kokoro_tokens`, and `data_dir` naming an included
nonempty directory. Its data files may use `kokoro_data` only under `data_dir`.
The other model kind's roles refuse. Additional complete pinned files can use
`dependency` or `metadata`. Every regular file except the manifest must appear
exactly once, including licenses, vocabulary, references and native libraries.
The manifest cannot list itself. A reserved role is not accepted.

Platform declarations must equal the compiled verifier's `macos-arm64`,
`macos-x86_64`, `linux-arm64` or `linux-x86_64`; other builds refuse. The declaration
and ABI version are pinned manifest claims, not inspection of actual image
architecture or proof of loader compatibility. Pocket thread range is 1..8,
Kokoro 1..12. Actual native dependency graph resolution, minimum OS, runtime ABI
compatibility, allowed backend and production catalog provenance remain separate
qualification gates before activation. Native code may dynamically open files;
a closed manifest does not constrain arbitrary code behavior.

Limits: manifest at most 1 MiB, 8,192 JSON tokens, 512 regular payload files,
512 descendant directories, 8 GiB total payload, relative names at most 255 bytes
and eight path components. Components use ASCII letters, numbers, `-_.`, do not
begin with a dot, and cannot be empty; absolute/traversal/alias spelling refuses.
For `kokoro_data` only, direct files under `<data_dir>/voices/!v/` preserve
eSpeak's literal `!v` component and its exact filename `Mr serious`. All other
space-containing names, nested variant files and `!` spelling refuse. Libraries,
metadata, Pocket files and `data_dir` itself do not receive this exception.
Every payload is nonempty. Root paths are canonical and at most 3,800 bytes.
Model identity is SHA-256 of `sherpa-speech-package-v1:` plus the manifest hash.

Verification reuses controlled artifact hashing: at most 65,536 bytes between
control checks, exact size/hash and file identity observations. Cancellation or
deadline errors propagate unchanged. Verification opens read-only descriptors,
does not launch engines, write files, delete entries or contact a network.

Initial verification publishes a receipt only on complete success; an initial
failure clears its verified flag. Recheck requires a successful previous receipt,
compares retained root/manifest/payload/directory identities and rehashes every
file. Failed rechecks preserve the previous observation for diagnostics but do
not authorize use: callers must act on the returned error. Even equal-content
replacement invalidates retained identity. Rechecks belong immediately before
and after engine use; they do not prevent a concurrent writer modifying files
between observations. Guardian isolation and package staging are still required.

`speech_package_options` derives all runtime and voice-reference paths from a
verified receipt. They remain valid while that host-owned receipt exists. This
function alone does not recheck disk state. `make test-speech-package
sanitize-speech-package` tests the private contract. Optional actual-model
`test-speech-real --package ROOT MANIFEST_SHA256` verifies the package, generates
both Pocket voices or all six Kokoro voices with cancellation/recovery, and
rechecks retained identity.

Native loader prerequisite: `speech_package_loader_verify` now inspects all
pinned `library`/`dependency` entries and validates the closed sibling-loader
graph, architecture, supported search paths, Mach-O compatibility/minimum OS and
ELF GLIBC requirements. The host obtains its policy from
`library_runtime_host_policy`; policy parameters are trusted private host inputs.
See [loader qualification and remaining limits](../docs/SPEECH_LOADER_VERIFICATION.md).
This is a separate gate from the manifest's declared platform/version fields.
The runtime also checks the loaded Sherpa version before model initialization.
