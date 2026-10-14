# Provider deployment platform policy

The registry's private host schema accepts an optional four-target `platform`
declaration. Discovery and adapter descriptors report the executing binary's
OS/architecture, declaration and compatible/incompatible/unspecified policy.
The declaration grants no package, model or readiness qualification. No public
request can override it. Omitted declarations retain legacy behavior.

Fresh incompatible inference/health requests refuse before opening an engine,
package or endpoint. The gate lives inside core execution after tracked replay,
so imported retained receipts do not require the original execution platform.
Audio range/delete and owner-bound local operations/sessions remain usable.
Policy changes do not alter normalized retry identity or silently retry a failed
operation. This host policy supplements package/runtime validation, including
native architecture/minimum OS checks where that package profile supports them.

Local macOS normal and ASan/UBSan results:

- Base registry177 checks per mode: target metadata, incompatible new-work
  refusal, exact completed replay after a target change, durable failed replay
  after restoring compatibility, a fresh survivor and invalid target rejection.
- Combined registry309 checks per mode: all seven actual adapter kinds together,
  including native text endpoint and speech output. Each incompatible kind
  refuses before model dispatch; discovery retains its existing limits and IDs.
  These controlled requests do not qualify actual models.
- Native CLI584 normal/587 sanitizer checks: strict private configuration,
  metadata without state creation, endpoint-independent incompatibility refusal,
  null/empty/unknown/numeric/array target rejection and public override refusal,
  plus the previous engine/replay/owner/cancellation gates. Poll counts vary.
- Public speech166 checks per mode: foreign-target completed replay and retained
  PCM retrieval, tracked new-work refusal, compatible descriptor, unchanged failed
  retry, plus streaming/cancellation/deadline/disconnect/backpressure recovery.

Commands: `make test-registry sanitize-registry test-registry-modalities
sanitize-registry-modalities test-registry-cli sanitize-registry-cli`, and
`make test-speech-public sanitize-speech-public`. Logs are
`/private/tmp/samosa-platform-final-tests.log`,
`/private/tmp/samosa-platform-modalities-final.log` and
`/private/tmp/samosa-platform-speech-tests.log`. The final combined gate adds speech
to the previously six-kind matrix without using its fake native runtime as
actual-model evidence. Installed actual-model evidence remains in the separate
modality qualification records. Neither this declaration nor controlled Linux
execution proves actual-model Linux support or complete release acceptance.
