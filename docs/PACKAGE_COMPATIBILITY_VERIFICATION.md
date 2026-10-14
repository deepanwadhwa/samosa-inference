# Concrete speech recipe checkpoint

The publication compatibility dispatch now implements the existing native speech
package recipe. It reuses complete native manifest verification and the pinned
transitive loader graph, with real host policy and request controls. No model or
library is initialized. Other recipes refuse explicitly pending implementation.

Local normal and ASan/UBSan qualification:

- `make test-package-compatibility sanitize-package-compatibility`: seven focused
  dispatch checks per mode for unknown recipe, missing/wrong-role native manifest,
  cancellation and invalid private context. Each mode also passes a complete
  native-format fixture package exported by the existing speech worker harness,
  exercising the positive concrete recipe on the CI-compatible platform.
- Actual pinned Pocket and Kokoro package/index pairs both pass normal/sanitizer
  complete-manifest, native graph and host compatibility inspection. Controls also
  refuse before reinspection. These are compatibility observations, not new model
  execution or installed readiness claims.

Generic index SHA-256:

- Pocket: `ce8bdc28ae0dbd8923b792669d595039c599547a8cfadf8accc3804accde1673`
- Kokoro: `068182b8a7de69cd9cbda4d2ac7436e9f1650a35de8d9ebec4bc8175037f3f75`

These indices retain the complete payload pins from the prior tree observation,
with recipe set to `sherpa-speech-package-v1`; explicit integrity-fixture minimum
OS remains1.0, while actual native image/graph minimum-OS checks remain enforced
by the reused loader. Paths: `/private/tmp/samosa-package-compatibility-{pocket,
kokoro}-index.json` and the unchanged existing native speech package roots.
Command: `build/test-package-compatibility <payload root> <private index> <pin>`,
or its `-sanitize` build. Logs:
`/private/tmp/samosa-package-compatibility-qualified-tests.log` and
`/private/tmp/samosa-package-compatibility-{pocket,kokoro}-{final-normal,sanitize}.log`.

Initial actual checks refused `invalid_configuration`: the new caller passed an
unverified receipt to a loader function that requires verification first. The
adapter now calls the existing package verifier before loader inspection; neither
validator was relaxed. A positive native-format fixture gate was added so archive
CI can detect that precondition regression without installed actual models.
Actual loaded ABI and model readiness remain separately qualified runtime gates.
All original remaining installation/lifecycle/resource/service/release requirements
remain active; this recipe alone does not close a numbered area.

## Connected staging and publication gate

The controlled native speech fixture now copies all 11 indexed files into a
new private payload through `package_stage_file`, publishes through the concrete
`package_backend_compatible` callback, revalidates completion, and repeats
publication to check idempotency. Cancellation during compatibility still
refuses a previously completed package. Normal and ASan/UBSan runs passed locally
on macOS arm64. This is fixture integration evidence; it does not establish
catalog acquisition, quota reservation, crash recovery, model loading or readiness.

The preceding concrete compatibility revision `7c9ff43` passed independent
macOS arm64 and Linux x86_64 CI in run 38588798514.
