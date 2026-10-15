# Pinned llama backend/model declarations

`llama_package_descriptor_parse` accepts trusted pinned metadata bytes and a
parsed PackageIndex. Its strict JSON object has exactly these six fields:

```json
{
  "format": "samosa.inference.llama-package.v1",
  "kind": "embedding",
  "runtime_revision": "4fbc76dec51d0add466f0210855c0596589b60d4",
  "engine": "runtime/llama-server",
  "model": "models/model.gguf",
  "architecture": "gemma-embedding2"
}
```

Kinds are `text`, `embedding` or `summary` (native T5 encoder/decoder engine).
Summary policy/model validation requires architecture `t5`. Runtime revision is a lowercase 40-character
hex declaration, architecture is a bounded lowercase identifier, and file names
use the existing canonical package grammar. Exactly one engine and one model
must exist in the index with those roles; declarations return their index positions.
Unknown/duplicate fields, commands, extra engines/models and path substitutions
refuse. The descriptor is bounded to 4KiB and owns a 64-token parser budget.
Failure leaves output untouched.

`llama_package_descriptor_model` compares the observed GGUF v3 architecture and
requires a nonzero declared tensor count. This binds the declared model identity
to inspected metadata. It does not validate tensor layout, verify the engine's
declared revision, establish runtime capabilities or loaded readiness. The
complete verifier must bind descriptor/model/engine bytes to index pins, enforce
trusted runtime qualification and check the native package graph before publication.
No installation recipe is admitted by these declarations alone.

Normal and ASan/UBSan gates pass 466 checks each, covering strict field/type
handling, identity/role mismatch, extra models, duplicate/command fields, every
truncated descriptor prefix, GGUF architecture mismatch, wrong version and zero
tensor counts. Actual pinned EmbeddingGemma metadata (`gemma-embedding2`, 413
tensors) matches the descriptor in both builds. Log:
`/private/tmp/samosa-llama-package-descriptor.log`.
