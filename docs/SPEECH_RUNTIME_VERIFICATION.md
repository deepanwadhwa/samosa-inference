# Native speech runtime extraction

2026-10-13, macOS ARM64. This is the private native primitive for area 1,
not closure of the public speech provider or any seven-area acceptance gate.

The code reuses `src/samosa_kokoro.h` and the native Pocket/Kokoro loading,
configuration, reference WAV conversion and PCM callback path from Samosa Chat.
The module owns its ABI header and source; independent builds do not include
parent files. No Python, service process or linker-time Sherpa dependency ships.
The ABI is pinned to Sherpa-ONNX 1.13.4. External runtime/model paths are trusted
host inputs. Complete package provenance, native dependency compatibility and
worker isolation must be supplied before this primitive becomes a public provider.

`make test-speech-runtime sanitize-speech-runtime` passes 320 checks each. The
native C-ABI fixture exercises both model configurations, exact PCM clipping and
rounding, two callbacks, final-only fallback without duplicate output, hash and
count agreement, malformed UTF-8, voice refusal, bounded audio, invalid rates,
nonfinite samples, missing/empty/partial/mismatched final audio, sink refusal and
cancellation. Mono and stereo PCM16 references pass; corrupt RIFF sizes, chunk
bounds, format, channels, rate, byte rate, alignment, bit depth and data lengths
refuse before PCM, as do symlinks/directories and unsafe loader environment.
The fixture is separately compiled with sanitizers in the sanitizer gate.

Actual installed Pocket command (substitute the explicit local package paths):

```
make build/test-speech-real build/test-speech-real-sanitize
build/test-speech-real /absolute/runtime/lib/libsherpa-onnx-c-api.dylib /absolute/model
build/test-speech-real-sanitize /absolute/runtime/lib/libsherpa-onnx-c-api.dylib /absolute/model
```

The phrase is `The meeting starts at nine. Please bring your notes.` Both voices
produce 24kHz PCM; Caro produces 78,720 samples in 22 sink chunks / 3 engine
callbacks, Stuart produces 90,240 samples in 25 chunks / 4 callbacks. Delivered
PCM hashes match final buffers and receipts in normal and sanitized adapter runs:

- Caro: `5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4`
- Stuart: `0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc`

A sink cancellation on the second chunk returns `operation_cancelled` after
partial PCM, publishes no successful receipt, and permits subsequent Stuart
synthesis on the same loaded runtime. The external model libraries are not
sanitizer-built. Energy, chunks and hashes prove actual generation and byte
integrity, not intelligibility or general speech quality. Actual Kokoro and Linux
qualification remain open.

The first actual Pocket run failed `speech_stream_mismatch`. Upstream Pocket
1.13.4 calls progress with raw decoder audio, then applies `ScaleSilence` to its
final buffer. The C ABI defaults a zero generation silence scale to 0.2. The
extraction sets Pocket generation silence scale to 1, preserving the raw audio
already delivered by the gateway and disabling this final-only transformation.
See the pinned upstream implementation:
https://github.com/k2-fsa/sherpa-onnx/blob/v1.13.4/sherpa-onnx/csrc/offline-tts-pocket-impl.h
and C ABI default:
https://github.com/k2-fsa/sherpa-onnx/blob/v1.13.4/sherpa-onnx/c-api/c-api.cc
Kokoro retains the existing generation speed 1.15, silence scale 0.12 and named
speaker mapping. Pocket retains CPU execution, 2 threads, voice cache 4,
reference resampling, five flow steps, speed 1 and seed 42.

Observed external artifact SHA-256 (these observations are evidence, not a
public installation manifest or complete dependency qualification):

| Artifact | SHA-256 |
| --- | --- |
| libsherpa-onnx-c-api.dylib | c8d9e266ab5de81372e9d1a275b4d42c4bace1e55e7359ab1227de5914fd21e1 |
| libonnxruntime.1.27.0.dylib | 5b4c4df69e6d1bc10eb0d08da525b11faf8365a0a78cc597a5cc57926330a9ca |
| decoder.int8.onnx | 12b0857402d31aead94df19d6783b4350d1f740e811f3a3202c70ad89ae11eea |
| encoder.onnx | e8f2f6d301ffb96e398b138a7dc6d3038622d236044636b73d920bab85890260 |
| lm_flow.int8.onnx | 8d627d235c44a597da908e1085ebe241cbbe358964c502c5a5063d18851a5529 |
| lm_main.int8.onnx | bfc0c7e7e3d72864fa3bb2ee499f62f21ddc1474b885f5f3ca570f8be73e787e |
| text_conditioner.onnx | 0b84e837d7bfaf2c896627b03e3f080320309f37f4fc7df7698c644f7ba5e6b1 |
| token_scores.json | 5be2f278caf9b9800741f0fd82bff677f4943ec764c356f907213434b622d958 |
| vocab.json | 6fb646346cf931016f70c4921aab0900ce7a304b893cb02135c74e294abfea01 |
| voices/caro_davy.wav | 40c692c005a0268a7a5b6ebae348077d3dca6a86eb6b12bd36e343bbcd71b5f6 |
| voices/stuart_bell.wav | 00c7baeb2fb7a8c1c6198e045b5e853a7ccc04002a51a09b4be3dd7c96994f73 |

Next: verify the closed runtime/model/voice package, run synthesis under the
existing owned guardian, and integrate owner-bound audio artifacts and public
provider operations. Preserve the gateway's 12–45 second bounded PCM allowance;
do not shrink it to fit the existing small JSON operation-result limit. Audio
storage, reads, cancellation/deadlines and streaming terminal events need their
own qualification before retiring gateway paths.

The complete indexed source was exported with `git checkout-index` to
`/private/tmp/samosa-inference-speech-independent.f0hkcej2`. Both native fixture
gates passed 320 checks there without parent source paths or prebuilt binaries.
