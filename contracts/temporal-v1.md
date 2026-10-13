# Native sampled temporal analysis contract1

`make temporal` builds optional `samosa-inference-temporal`. It adapts the
existing native Molmo2 temporal processor and model; it does not implement a
decoder or another model engine. Public callers supply prepared pixels, not
filesystem paths. Sources owns source selection, decoding, sampling and citations.
The current actual MLX/Metal dependency is qualified on macOS. Linux C adapter
and boundary checks do not assert that this installed model runs on Linux.
No Python runtime is required or shipped.

Host flags: `--temporal-engine`, `--temporal-model`,
`--temporal-engine-sha256`, `--temporal-manifest-sha256`, `--admission-state`,
and optional `--provider-id`, `--owner`, `--queue-capacity`,
`--admission-wait-ms`. Paths are absolute. Default provider is `molmo2-native-v1`.
Model identity is SHA256 of ASCII `molmo2-package-v1:MANIFEST_SHA256`.
The configured manifest binds all declared file names, sizes and SHA256 hashes.
The supported package is `samosa.molmo2.mlx.v1` / `molmo2-4b-mlx-q4-v1`, with
the existing upstream revision, processor, affine Q4/group64 and resident budget.
Manifest<=1MiB,4..64 listed files and aggregate<=4GiB; required config/tokenizer/
processor pins match the native dependency. The executable is<=64MiB.
Artifacts must be owner-held regular single-link files without group/other write;
symlinks refuse. Directory identity, manifest and all files are checked before
execution and again after reaping, with bounded controlled reads. This proves
observations, not future filesystem immutability.

`analyze_video_frames` requires contract_version=1, provider_id, start_us,
end_us, frames, prompt and max_output_tokens. The selected interval is
0<=start_us<end_us<=14400000000. `frames` has1..16 objects with timestamp_us,
width, height and rgba_base64. Each axis is1..512, pixels are canonical packed
RGBA8, and timestamps are strictly increasing inside `[start_us,end_us)`.
Base64 padding and unused bits are canonical. Prompt is nonempty UTF-8<=4096
bytes; output budget32..1024 tokens. Requests<=24MiB for this executable only;
other executables retain their existing limits. Unknown root/frame fields refuse.

Optional operation_id/parent_operation_id/timeout_ms use existing durable
operations. Parents require IDs. One120s default budget spans admission,
verification, preparation, execution and output validation; timeout_ms1..120000
may reduce it. The original Molmo2 supervisor also declares120000ms. Filesystem
operations, parsing and cleanup are not hard real-time guarantees.
One active job and0..8 FIFO waiters use existing admission. Normalized operation
identity binds ordered pixel/time/dimension hashes, selection, prompt, output
budget, configured provider and engine/model pins. Raw frames are not retained
in operation identity. Completed retries replay before model/runtime file access;
owner scope remains mandatory.

Input SHA256 hashes canonical JSON in this exact order:
`recipe=molmo2-temporal-rgba8-v1`,start_us,end_us,frames; each frame has
timestamp_us,width,height,rgba_sha256. The adapter composites alpha on white,
writes exact native P6 images, and calls the existing `video_frames` protocol.
Each prepared hash covers `P6\nWIDTH HEIGHT\n255\n` plus packed RGB bytes.
The aggregate prepared hash covers an ordered JSON array of timestamp_us and
ppm_sha256. The native temporal processor retains its378px resize and timestamp
labels formatted to one decimal second. Exact input timestamps remain in receipts.

The existing guardian retains the admission lease, model directory and private
workspace until reaping. It cleans frame-00.ppm through frame-15.ppm and owned
request/result files after normal completion, failure, cancellation or requester
death. Logs<=256KiB and result<=32KiB. Only fixed owned names are removed.
Native analyze/quit replies must validate input identity, frame count, timestamps,
nonempty UTF-8 observation and token budgets. Changed artifacts or malformed
responses publish no successful observation.

Success is kind=derived, artifact_type=video_observation, with text,input_sha256,
range in input_video_us, ordered per-frame timestamp/dimension/pixel/prepared
hashes,usage and model/runtime/manifest provenance. Coverage is
partial/sampled_video_analysis_uncertain. Timestamp policy is
molmo2_one_decimal_seconds; alpha policy is rgba8_composited_on_white.
Observations can infer transitions between sparse samples. They are not literal
evidence or proof of exhaustive temporal coverage.

The actual macOS checkpoint passes ordered red/blue input at512px, a>2MiB
public request, exact pixel/PPM/time/model/runtime bindings, foreign-owner refusal
and model/runtime-free completed replay, normally and with the C adapter/harness
under ASan/UBSan. The external MLX runtime is not sanitizer-built. Dedicated
temporal lifecycle/corruption/admission fixtures and Sources capture/cache
integration pass native normal/sanitizer gates on macOS/Linux. Sources actual
model gates also cover selected late ranges and retained origin/helper-free
replay. Final release acceptance remains open; the architecture verification
document records the finite area acceptance and actual-model bounds.
