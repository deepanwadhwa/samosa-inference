# Embedding generation boundary — implementation target

Status: public C dispatch and normalized durable operation path implemented;
qualification in progress. Host CLI setup and optional managed owned runtime/model pin binding are implemented.
Basic actual-model text generation/retrieval gates pass on macOS/Linux with normal
and sanitizer adapters. Sources text-query integration is implemented; automatic
document publication and broader actual-model qualification remain pending.
The pinned external runtime is not sanitizer-built.

Inference owns execution and pinned model/runtime artifacts. Sources owns evidence
selection, chunking with retained selectors, durable queue phases and publication.
Chutni already owns representation storage and cosine search. Reuse its public
put_representation_bound/search_semantic operations and the existing isolated worker;
do not introduce another vector store or search implementation.

The public operation is embed_text. Requests contain contract_version1, text,
role (query or document), optional document title, and existing operation controls.
Public JSON cannot choose a model, executable, endpoint, profile or dimension.
Document role uses the host's document formatting; query role uses its search
instruction. Empty input, unknown/duplicate fields, invalid UTF-8 and input beyond
the configured token budget refuse. Never silently truncate the supplied text.
Long evidence is partitioned by Sources with exact chunk selectors.

Host configuration pins provider, runtime and model digests, model/revision,
tokenizer, pooling/projection, instruction recipe, output dimensions and numeric
normalization. All affect representation identity. SDK profile fields remain the
existing representation_kind/model_id/model_revision/dimensions/dtype/
normalization/tokenizer_hash/projector_hash. Bind preprocessing and quantization
identity into the revision rather than dropping it or extending the SDK ad hoc.
Query and document prompts are complementary roles of the same retrieval profile;
their distinct role/input identities belong in operation receipts.

Responses bind provider, input digest/bytes, role, complete profile, f32 vector and
runtime/model provenance. Dimensions must match the host profile; values must be
finite after f32 conversion and have nonzero norm. L2 output must pass the existing
Sources norm tolerance. Parent validation independently binds input, role, model
and profile. Existing admission, deadlines, cancellation, parent-death isolation
and owner-specific durable receipts apply. Offline replay must not run a model.

Sources publishes vectors against the exact artifact whose text was embedded.
Literal evidence and generated summary representations remain distinct. Reuse
the SDK's payload hash and put_representation_bound precondition; recover SDK
commit before local acknowledgment using its exact artifact/profile/vector
deduplication, without another model call. Do not fall back to an unchecked write
when the SDK lacks that operation.
Text-query search first obtains a validated Inference vector, then uses the existing
memory_search_semantic adapter with the same profile. Profile mismatch never
falls back to another model or mixes vector spaces.

Acceptance requires public Inference/Sources actual model generation, artifact
publication, meaningful positive/negative retrieval and profile isolation on
macOS/Linux; normal and sanitizer controls/recovery; long-input refusal or exact
chunk coverage; finite normalized outputs; offline replay; changed-artifact
refusal; SIGKILL before/after SDK commit; and pinned independent release gates.
Fixture vectors establish orchestration only, not retrieval model quality.
EmbeddingGemma2 remains an evaluation candidate, not a selected default.
