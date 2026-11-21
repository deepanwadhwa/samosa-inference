# Ornith text CPU distribution

The original Ornith1.0 9B Q4_K_M model and existing static llama.cpp CPU runtime
pass cold published-URL installation, actual inference/control tests, cache-only
resume/removal/reinstallation and cleanup in normal and ASan/UBSan native hosts.
Both exact receipts are retained here. Execution scope is macOS26.5.1 ARM64;
observed engine minimum OS is26.0. The external engine is a release build.

The seven-file index pins5645083556 payload bytes. Model weights are acquired
directly from the original immutable upstream URL; this directory contains only
metadata, original notices/card and qualification receipts. No model conversion
or source-repository weight copy is introduced.

Actual tests preserve the existing corpus: native rendered count24, arithmetic
answer4, active cancellation, survivor, owner-scoped status and foreign-owner
refusal, EOF shutdown, credential cleanup and exact offline replay. These bounded
checks do not establish general reasoning quality, incremental token streaming,
KV-session semantics or common-provider loaded readiness.

See [installation evidence](../../docs/TEXT_PACKAGE_INSTALLATION_VERIFICATION.md).
Host provisioning supplies private catalog/source configuration. Public requests
select IDs; they cannot provide private launch commands or paths. Installation
remains distinct from loaded readiness.
