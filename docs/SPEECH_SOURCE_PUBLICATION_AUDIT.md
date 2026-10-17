# Speech source publication audit

The retained C API library's SHA-256 is
`c8d9e266ab5de81372e9d1a275b4d42c4bace1e55e7359ab1227de5914fd21e1`.
Loading only its existing metadata exports reports Sherpa version `1.13.4` and
Git prefix `14280725`, matching the pinned full source revision
`142807252687d81b40d6315f23470a1512a00de3`. Mach-O import inspection finds only
the colocated versioned ONNX Runtime library and system libraries. The package's
original runtime and source files are unchanged.

The original source's CMake dependency graph was inspected, including transitive
core links and speech options:

| Runtime source dependency | Supplied source |
| --- | --- |
| Sherpa core and C API | Complete pinned Sherpa source, CMake scripts and macOS workflow |
| Feature extraction | kaldi-native-fbank and its pinned kissfft |
| Decoder | kaldi-decoder, kaldifst, OpenFST and Eigen |
| Tokenization | simple-sentencepiece |
| Speech phonemization | Piper phonemize, eSpeak and Unicode sources |
| Headers and clustering | JSON, hclust and Eigen |
| Separate shared runtime | Complete original ONNX Runtime 1.27.0 source archive and build scripts |

Sherpa sets `ESPEAK_NG_DIR` to its pinned eSpeak source before adding Piper, so
Piper's alternate external eSpeak download is not the selected speech source.
The Sherpa build disables the dependencies' Python/test targets. The supplied
source includes original conditional build rules; the source inventory also
retains both encountered Eigen variants rather than silently dropping one.

The 13 static speech source archives and full license notices are supplied in
`speech-corresponding-sources-v1.tar.gz`, SHA-256
`42b582ec9ec9812567a2888909cd0cbd13f251f431fb7d6d4a655f2bee4a4656`.
All enclosed archive hashes were independently read back and matched the pinned
inventory. Original LICENSE/COPYING/NOTICE files, including GPL-3.0, BSD2, Unicode
and permissive dependency terms, are retained in the source archives and notice
collection. The object-code packages include their full source notices.

The separate `onnxruntime-1.27.0-source.tar.gz` is 285714570 bytes, SHA-256
`b41d09905a3c2f3a25709d1dcce8ef3942a4c2799d1046f74be7b6bbebc45e6a`.
Its original `VERSION_NUMBER`, LICENSE and build.sh were checked. Both source
assets accompany the object-code downloads and are named in the release notes.
The source archives remain outside the repository; only their inventory and
[audit receipt](../catalog/candidates/speech-source-publication-audit-v1.json)
are committed.

This review establishes source-version binding and distribution of the inspected
source graph and build rules. It does not establish byte-identical reproduction
of upstream binaries or model execution on another platform. Those claims are
not used to close component 2 or the full seven-component goal. Exact installed
speech tests already pass; published-URL qualification remains separate.
