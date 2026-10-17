# Native VisionPsy distribution

The existing independently built engine, colocated MLX kernels and unchanged
VisionPsy Nano460M MLX BF16 model are reused. The package uses the existing
`visionpsy-package-v1` recipe and native installer. No inference implementation,
model conversion or second installer is introduced.

The 17-file index is
`36d4ef385f5ea420fbf487d4f4018a31f82e2cb59e7dcb1dfd86dcf0a208f71a`,
with 1199764987 payload bytes. Only metadata is stored here. Model weights use the
App catalog's immutable converted-model revision
`ca7a1bad19a08fe8e474bee24275a2e377bbabba` directly from Hugging Face. Runtime
release assets are streamed from the retained independent build without model
copies or hardlinks. Original source/model cards and full model/runtime dependency
licenses accompany the package, including MLX, fmt, json, metal-cpp and stb_image.

The [package prerelease](https://github.com/deepanwadhwa/samosa-inference/releases/tag/inference-vision-package-2026-10-17.1)
contains 16 unique assets: the model is upstream, and identical license files share
a content-addressed asset. The draft uploader verified every uploaded asset by
streaming it back to a hash before publication.

Full exact-package HTTPS installation and actual inference pass in normal and
sanitizer host modes. Independent cold native installation from the published
URLs also passes both modes, including all 15882 existing heading-recognition
checks, derived observation provenance, durable replay, exact installed hashes,
cache-only status/resume/removal/reinstall, and temporary cleanup. The observed
heading contained `hello`; this is bounded fixture recognition rather than a
claim of general visual accuracy. Engine, MLX and kernels are release builds.

Execution and engine minimum OS are macOS26.5.1 arm64 only. Other-platform model
execution and loaded readiness are not established. Public requests cannot select
private paths or credentials. App provisioning must copy catalog/source policy
into owned private configuration; checkout files are not themselves trusted
configuration. Component2 and the full module release remain incomplete.
