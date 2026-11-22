# LIFE-07o — reuse structured Whisper through common lifecycle controls

Qualified on this Mac, 2026-11-21; checkpoint recorded 2026-11-22. Inference area 3
and the full repository split remain open. No model implementation was replaced.

## Ownership and preservation

The missing boundary was lifecycle management for the existing **structured,
on-demand** Whisper adapter. `WhisperHost` wraps the existing `whisper_run`, WAV
validator, admission, request guardian and Activation transaction. Optional private
`on-demand-whisper-v1` configuration binds engine/model and a canonical speech
probe by hash/size, plus expected text. Public start runs that real transcription
before publishing last-good. Status reports qualified unchanged assets with
`execution_mode: on_demand`, `resident: false`; no idle engine is invented.
Stop joins users before releasing ownership; restart performs qualification again.
See [boundary contract](../../ON_DEMAND_LIFECYCLE.md).

This is deliberately the original structured 30-second canonical-WAV/timed-SRT
contract. The **separate long-window txt/SRT legacy CLI remains unchanged** and
still needs common controls under its own original contract. No Gateway route is
replaced by a short-clip profile. Primary/default visual common controls,
pre-admission recovery and general unknown-group recovery remain required.

The source audit compares full original files against `dbb680a`, removing only
the explicitly documented adaptations. It preserves original Whisper algorithms,
CLI arguments, language, normalization, input/SRT limits, receipts/replay, original
Activation transactions/resident policy, other registry branches and original
fixture assertions. Model engines, long-window runner, primary/turn-control and
other modality owners are byte-unchanged. `source-audit.txt` records the result;
`audit-reuse.mjs` is its executable comparison, not a claim based on file names.

## Demonstrated defects and necessary adaptations

1. A stopped request guardian exposed loss of synchronous ownership after a
   deadline: the old request returned while cleanup was pending. The reproduction
   is `pending-cleanup-baseline-failure.log`. Managed execution now keeps its
   request/registry scope until cleanup is confirmed. Stop stays busy through a
   six-second suspension, then the resumed guardian reaps before the original
   deadline error returns. Unmanaged behavior remains unchanged.
2. Killing a guardian gives an unknown cleanup result. The managed host withdraws
   qualification and **retains Activation's fence**, refusing false stop/restart
   success. Tests prove exclusion while that host remains alive. Recovery of the
   unknown group, including after that host dies, is still open; no force reset is
   supplied and these tests do not prove generic guardian-death recovery.
3. Real native execution failed with `provider_process_failed`. Private diagnostic
   builds showed `CLD_KILLED`, signal 25 (`SIGXFSZ`) during Metal initialization;
   `actual-open-files.txt` shows Apple's shared shader cache `libraries.data` was
   223739904 bytes. The inherited unsized workspace cap was 67108864 bytes.
   `native-cache-signal.txt` and the `actual-*-diagnostic.log` files retain this
   investigation. Direct execution and a previously warmed copied engine worked;
   they did not qualify a fresh copied engine under the full host.
   The managed adapter now opts into the existing bound-workspace guardian without
   a process-wide file cap, as existing native duplex/captured/legacy command
   profiles already do. Lease/model binding, workspace/group/parent-death cleanup,
   bounded logs, deadlines and the original 16KiB SRT validation/watch remain.
   Capped and explicitly sized profiles are unchanged. No engine flags, model,
   GPU policy, output validator or qualification threshold was changed.

## Final native evidence

All commands below ran under Chat's `tools/without_python.sh`. No Python engine,
helper, dependency installation or interpreter ran; no Docker/VM/Linux execution,
model download/conversion, catalogue update or release/tag was performed.

| Evidence | Verified scope and terminal result |
| --- | --- |
| `native-final-qualified-macos.log` | Exit 0: common Whisper lifecycle 469 normal / 469 SAN; original guardian 525 / 525; Activation 158 / 168; original public Whisper 508 / 511. |
| `standalone-qualified-macos.log` | Exit 0: independently copied 189 source files, common lifecycle 469 / 469 and Activation 158 / 168. No parent implementation or prebuilt objects copied. |
| `standalone-macos.log`, `standalone-final-macos.log` | Earlier source-only preparation rebuilt the original Rust 1.90.0 tokenizer archive offline from cached sources in the independent directory. Final source refresh preserves that independent native build. These earlier passes predate the final cache adaptation. |
| `native-compatibility-macos.log` | Exit 0 before the final cache-only opt-in: registry CLI 584 / 587, text lifecycle 260 / 260, embedding 332, managed readiness 152 / 152, managed host 71 / 71, owned summary 281 / 354, speech runtime 324 / 324, speech worker 493 / 494, Whisper 508 / 511, visual 661 / 671 and summary 478 / 478. |
| `native-legacy-compatibility-macos.log` | Exit 0 after final cache adaptation: primary 676 / 676, long-window Whisper 522 / 522 and both original VisionPsy/Molmo launch profiles normal/SAN. |
| `actual-qualified-macos.log`, `actual-qualified-sanitize-macos.log` | Both exit 0, 7023 assertions each using retained installed native engine/model and public C registry dispatch. Actual qualification, transcript, stop, fresh requalification, last-good identity and no idle engine are checked. C host instrumented in SAN; external installed engine remains its original release build. |
| `source-audit.txt` | Original implementation/assertion preservation passes after documented boundary adaptations. |
| `static-analysis-macos.log`, `static-analysis-baseline-macos.log` | New host has no diagnostic. Activation reports one json_quote uninitialized-value warning, also reproduced from the original Activation source; not a clean-analysis claim or an unrelated JSON rewrite. |

The native fixtures retain malformed input, actual request replay after stop,
failed probe and exact last-good preservation, checksum/permission mutation,
worker death, scoped cancellation/deadline and busy controls. Owner death is tested
both idle and during a request. A replacement host must wait for the old request's
admission lease, even while its guardian is stopped. Darwin automatically resumes
orphaned stopped groups, so the stalled test keeps that guardian group non-orphaned
with an unrelated peer **outside** the worker group. The peer survives cleanup.
Mac test threads use the existing 4MiB registry-worker stack policy; the default
512KiB stack produced the recorded test-harness SIGBUS during development.

Final source hashes are `source-files.sha256` and `standalone-files.sha256`, with
relative names for their respective roots. `standalone-source.txt` records the
local independent root. Main source was built with the native C toolchain; the
existing Rust archive linker warning about 26.5 objects versus a 26.0 target is
retained. This qualification machine runs macOS 26.5.1; no older-OS claim is made.

## Retained actual inputs and result

No asset in the normal `~/.samosa` installation was modified. The test copies the
borrowed installed assets into its private root and uses original readable-file
permissions. Actual input provenance is recorded in `actual-inputs.sha256`:

- Engine: `~/.samosa/voice/runtime/whisper-cli`, 3220840 bytes.
- Model: `~/.samosa/voice/stt-whisper-base-en/ggml-base.en.bin`, 147964211 bytes.
- Canonical speech: 81100 bytes, 2533 ms, SHA256
  `524c7aa3d6fc457167ffc493fa34100847cb51249620f10c918f06d1def84b94`.

`prepare-actual.mjs` applies the original `test_whisper_real.c` canonical PCM
construction to the retained synthetic Mac say/afconvert WAV, dropping only a
sub-millisecond tail. `actual-source.txt` records its local directory. Expected
`fox` is present; normal/SAN both produce:

> The quick brown fox jumps over the lazy dog.

The unchanged timed result spans 0–2480 ms and retains
`speech_recognition_uncertain` coverage. The memory guard allowed 6144MiB process
tree RSS and zero swap growth; **sampled** RSS maxima were 33MiB normal and 399MiB
SAN, not a continuous footprint bound. Both reported zero swap growth.

## Reproduction and excluded work

`commands.sh` records the scoped C-only owner/independent/actual checks. Read Make
recipes before selecting any aggregate: several historical aggregates still call
Python. `compatibility-macos.log` is explicitly an **excluded partial command**:
the no-Python guard intercepted `tests/summary_registry_lifecycle.py`; no Python
executed and that public driver did not pass. The speech public lifecycle Python
driver was not run either. Their required assertion-preserving native ports remain
open; the native C checks above are scoped evidence, not replacements by assertion.

Other initial/intermediate logs retain expected development failures: missing
header, Mac thread stack, orphan-group suspension assumption, missing fixture state,
stale cleanup-field compilation, overly short sanitizer fixture deadline and
pre-fix native Metal failure. `actual-core-diagnostic.log` got empty stdin because
the memory guard backgrounds its child; `actual-core-input-diagnostic.log` fixes
only the diagnostic shell redirection. These are not final qualification passes.
The derived base64 diagnostic request stays outside the repository; canonical
input and preparation/hash records suffice to regenerate it.

Next use the extracted owners for the **full original default contracts**: primary
Qwen/Maple/llama common controls and successful-generation readiness; long-window
Whisper/common controls; default visual contracts; identity/stale-readiness,
last-good and unknown cleanup recovery. Recheck all area-3 requirements before
closing it. Then continue areas4–7, Agent, App, pinned assembly and qualified legacy
retirement in the agreed order. Sources remains closed. Inkling catalogue work
stays deferred until the entire refactor is finished.
