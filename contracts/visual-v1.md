# Native visual analysis contract v1

The optional `samosa-inference-vision` executable adapts the existing native
VisionPsy inspect/quit protocol. `make vision` builds it; `make test-vision
sanitize-vision` tests its public boundary with a native protocol fixture.
There is no second vision engine and no dependency on Sources implementation.

Host arguments are `--vision-engine`, `--vision-model`,
`--vision-engine-sha256`, `--vision-config-sha256`,
`--vision-tokenizer-sha256`, `--vision-weights-sha256`, `--admission-state`,
and optional `--owner`, `--provider-id`, `--queue-capacity`,
`--admission-wait-ms`. Default provider is `visionpsy-native-v1`.
Paths are absolute; symlinks refuse. Engine and model files must be regular,
owner-held, single-link and without group/other write permission. The model
directory must also be owner-held without group/other write permission.
Limits: executable 64MiB, config.json 1MiB, tokenizer.json 8MiB,
model.safetensors 8GiB. These are the three files the existing engine reads.
The compound model identity is SHA256 of the ASCII string
`visionpsy-model-v1:CONFIG_SHA:TOKENIZER_SHA:WEIGHTS_SHA`, using lowercase pins.

`describe` reports visual_analysis, operations, cancellation and request_budgets.
`analyze_image` requires contract_version=1, provider_id, image_rgba_base64,
width, height, prompt and max_output_tokens. Pixels are packed row-major RGBA8;
each axis is 1..512, with exactly width*height*4 bytes. Base64 must be canonical,
including padding and unused bits. Prompt is nonempty UTF-8, at most 4096 bytes;
output token budget is 32..1024. Requests are at most 2MiB. Unknown fields refuse.
Public callers cannot provide engine/model/input paths or engine arguments.

Optional operation_id, parent_operation_id and timeout_ms use the existing
durable operation contract. Parents require an operation ID. The default budget
is 60s; timeout_ms 1..60000 can reduce it. One deadline spans admission, bounded
artifact hashing, execution and validation. Hashing checks control between 64KiB
reads. Cleanup and filesystem calls are not hard real-time guarantees.
Exact completed retries replay before model/engine filesystem access. Request
identity includes pixels, dimensions, prompt, token budget, configured provider,
engine/model hashes, parent and explicitly requested timeout.

The adapter composites alpha on white, writes an owned P6 image and inspect/quit
command stream, and invokes the existing engine through the shared guardian.
Linux executes the verified engine descriptor and supplies the bound model
directory as /dev/fd/5. Darwin denies descriptor execution and directory traversal
through /dev/fd; it uses the existing native path launch and resolves the bound
model directory with F_GETPATH. Inode/metadata checks before launch and after
reaping refuse changed artifacts; these are observations, not filesystem locks.
Original model directory/file identity, size, permissions, ownership, link count,
mtime and ctime are checked again, including replacement followed by restoration.

The guardian owns the process group, admission lease and private workspace until
reaping. It removes input.ppm/request.json/result.json on normal exit, deadline,
cancellation and requester death. Engine logs are bounded to 256KiB; result is
bounded to 32KiB. The response must contain exactly a valid inspect reply and
successful quit reply. Echoed input identity, nonempty UTF-8 observation and
token counts are checked. Embedded NUL, malformed replies, wrong identity,
oversized output and exceeded output budget refuse invalid_provider_response.

Success returns kind=derived, artifact_type=visual_observation, input_sha256,
width, height, text and usage. Provenance includes engine=visionpsy,
engine_sha256, model_sha256, prepared_image_sha256,
alpha_policy=rgba8_composited_on_white and coordinate_space=input_pixels.
Coverage is partial/visual_analysis_uncertain. Sources owns original source
identity, preparation provenance and source timeline; observations are derived
model output and are not literal source text or exhaustive frame coverage.

`build/test-vision-real` and its sanitized counterpart accept public adapter,
native engine, actual model directory and rendered P6 fixture paths. The existing
monolith hello.pdf heading expectation passes on macOS with the installed BF16
checkpoint. This proves that bounded native recognition path, not general
captioning/VQA accuracy. Linux protocol/control tests use a native fixture;
they do not assert availability of the macOS MLX/Metal engine on Linux.
Sources visual capture/index/publication integration and broader modality/release
qualification remain open.
