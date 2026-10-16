# Trusted per-file acquisition configuration

Setup accepts `--file-sources ABSOLUTE_PRIVATE_FILE` with a package directory,
instead of a digest endpoint. This exposes the existing `PackageInstaller`
per-file remote sources; the package index remains URL-free. Public requests
still select only a catalog identity. The source file is provisioned by the host,
not downloaded or accepted as public input.

The exact object is:

```json
{
  "format": "samosa.inference.package-sources.v1",
  "index_sha256": "<selected catalog index SHA-256>",
  "files": [
    {"url": "https://upstream.example/model"},
    {"url": "https://private.example/engine", "bearer_key_file": "/private/engine-key"}
  ]
}
```

The array is in pinned-index order and must contain exactly one entry for each
index file. Index hash binding is checked before acquisition; count agreement is
checked after the verified index is read, before payload assembly. Each payload's
size and hash remain exclusively those of the pinned index. Reordering URLs
cannot substitute bytes or their package names.

One to512 source entries fit within a1MiB configuration bound. Each URL is
1..2048 bytes of HTTPS configuration, without user information, fragments or
control/space characters. Unknown/duplicate fields, unsupported formats, empty
arrays and incomplete objects refuse. The existing native downloader validates
the complete URL/TLS/redirect policy when a cache miss needs acquisition.

The file uses the catalog's private owned regular single-link file snapshot
checks, extracted into `private_config.h`: no symlinks, absolute bounded path,
bounded bytes, and stable held/named inode and nanosecond metadata. Every optional
credential path uses the existing bounded private-key reader. All credential
buffers are cleared on success or failure. Neither catalog nor digest bearer
credentials are inherited; each file opts into its own credential. CA and
redirect allowlist configuration are shared, with the downloader's existing
original-authority credential isolation.

`--file-sources` cannot repeat or coexist with `--digest-endpoint` or
`--digest-bearer-key-file`. Listing, status and removal do not load the sources or
credential files. A cached install/resume can omit this flag and all credentials;
the original cache-only behavior is preserved. Configuration paths, URLs and
credentials never appear in the public receipt. No activation or model readiness
claim follows from installation.
