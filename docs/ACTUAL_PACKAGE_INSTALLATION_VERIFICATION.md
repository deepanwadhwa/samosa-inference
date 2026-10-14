# Actual complete package installation

## Pocket, macOS arm64

The normal native/HTTPS/public-setup gate passed against the retained actual
Pocket package on 2026-10-14. The original package was read only; all destinations,
caches, reservation pools and certificates were disposable test state.

- Native manifest: `4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16`
- Complete index: `7dabd32730879d00d88ef6c9b477a9a861e0009e2af7ebb6c383de95740fd3e9`
- Complete payload: 19 files, 265,042,042 bytes.
- Recipe: `sherpa-speech-package-v1`; host `macos-arm64`; minimum OS declaration `14.0`.
- Log: `/private/tmp/samosa-pocket-complete-install.log`.

The gate checks native manifest/closed loader graph/host compatibility, staging
and retained-file recovery, local acquisition and verified-cache assembly,
corrupt cache/payload refusal, completion recovery, persistent copy quotas,
complete HTTPS index/payload acquisition and public setup cold installation.
Untrusted TLS refuses. Offline status/resume issue no extra downloads. Empty
reservation release is idempotent and refuses installed payloads. The fixture
asserts the exact unique pinned objects served for each cold cache.

```sh
python3 tests/package_compatibility_fixture.py \
  build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so \
  --https --setup build/samosa-inference-setup \
  --actual-package /private/tmp/samosa-pocket-pinned.0mx98nek \
  --manifest-sha256 4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16 \
  --minimum-os 14.0 --timeout 900
```

The initial run exposed `artifact_changed` during copying. Syncing completed
writes before the hash verifier's metadata snapshots lets the full gate pass;
normal and ASan/UBSan staging checks also pass. The complete actual Pocket
native/HTTPS/setup gate also passes with both native harness and setup built under
ASan/UBSan; log `/private/tmp/samosa-pocket-complete-install-sanitize.log`.
Kokoro's first run passed native installation but exposed a public setup manifest-parser budget
mismatch. Speech manifests now own their 8,192-token bound instead of inheriting
the public request parser's 256-token default or caller build flags.
This proves observed installation and static compatibility only. It does not
publish a production distribution catalog, load a model, prove readiness,
qualify another platform, or close remaining component-2 requirements.

## Kokoro, macOS arm64

The complete normal native/HTTPS/public-setup gate passed after the manifest
budget fix. It exercises the same checks listed above, including all nested
espeak resources and duplicated content identities in the verified cache.

- Native manifest: `8d3ce27844a7147201dbb0520d7407d861de52b9c729e7140175484a7a848aa7`
- Complete index: `abacddef8cd0e33e7d4d07c0bc434c0996a2b4ab4429fece432897582bb88c81`
- Complete payload: 365 files, 218,337,007 bytes.
- Recipe/host/minimum OS declaration: `sherpa-speech-package-v1`, `macos-arm64`, `14.0`.
- Log: `/private/tmp/samosa-kokoro-complete-install.log`.

Use the command above with `--actual-package /private/tmp/samosa-kokoro-pinned.s5_d_czq`
and the Kokoro native manifest pin. The complete actual Kokoro native/HTTPS/setup
gate also passed under ASan/UBSan with the executable-mapping changes at
`92e8a74`; log `/private/tmp/samosa-kokoro-complete-install-sanitize.log`.
After extraction of the shared dependency resolver, direct actual Pocket normal
and actual Kokoro sanitizer compatibility gates passed again; see
[shared graph evidence](../contracts/native-loader-graph-v1.md).
No other platform, runtime readiness or production distribution catalog is
claimed from these gates.
