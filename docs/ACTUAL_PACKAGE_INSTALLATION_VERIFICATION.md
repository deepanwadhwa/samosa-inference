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
Kokoro complete installation is being rerun and is not yet qualified. Its first
run passed native installation but exposed a public setup manifest-parser budget
mismatch. Speech manifests now own their 8,192-token bound instead of inheriting
the public request parser's 256-token default or caller build flags.
This proves observed installation and static compatibility only. It does not
publish a production distribution catalog, load a model, prove readiness,
qualify another platform, or close remaining component-2 requirements.
