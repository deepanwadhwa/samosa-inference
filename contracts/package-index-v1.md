# Pinned multi-file package index v1

This private acquisition index describes a complete bounded payload set. Its
exact bytes are pinned by trusted catalog/host configuration before parsing.
It contains no URL, credential, command, owner, state path or activation request.

```
{
  "format": "samosa.inference.package.v1",
  "platform": "macos-arm64",
  "minimum_os": "14.0",
  "recipe": "fixture-v1",
  "files": [
    {
      "name": "models/model.gguf",
      "role": "model",
      "bytes": 3,
      "sha256": "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    }
  ]
}
```

Maximum1MiB index bytes,1..512 files,255 bytes per relative name, eight path
components and64GiB aggregate declared payload. Each file is nonempty and has
an exact lowercase SHA-256 pin. Roles are `engine`, `library`, `model`, `data`
and `metadata`. Duplicate names, ASCII case aliases, file/parent-directory
collisions and inconsistent size declarations for the same content identity
refuse. Repeated content at distinct paths is allowed when its size agrees.
Unknown fields, duplicate JSON keys, invalid integer sizes and malformed pins
refuse; the parser leaves its output untouched on any failure.

Names are canonical relative paths with ASCII letters/digits and `-_.` inside
components. Absolute paths, hidden/dot components, empty components, trailing
slashes, backslashes and deeper paths refuse. The existing literal eSpeak data
variant component `!v` and its `Mr serious` filename are accepted only for `data`.
These names cannot escape an installer-owned root; concrete filesystem binding,
symlink/link refusal and complete-tree publication remain the installer's job.

Platforms are the same four native targets as the provider registry.
`minimum_os` reuses catalog version parsing and comparison (macOS product version,
Linux kernel version). Recipe is a bounded nonempty identifier. Recipe/backend
qualification is separate from syntactic parsing and deployment policy. A valid
index does not prove acquired payload hashes, native loader/ABI compatibility,
installed state, readiness, model quality or an executable trust boundary.

Native API: `package_index_parse` in `package_index.h` validates the complete
manifest pin and metadata; `package_index_qualify` checks explicit deployment/OS
policy. These are acquisition foundations. No public package-install operation
or multi-file publisher is claimed by this schema alone. Existing v1 single-file
catalog/install contracts retain their16-artifact store limit.
