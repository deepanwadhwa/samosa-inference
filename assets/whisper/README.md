# Whisper qualification recording

The bundled WAV is the 11-second excerpt from President John F. Kennedy's
1961 inaugural address used in the pinned whisper.cpp runtime. The Inference
startup probe checks the words `my fellow Americans`, which both catalog
English models recognized on the target Mac.
[National Archives record](https://www.archives.gov/milestone-documents/president-john-f-kennedys-inaugural-address)
[Pinned whisper.cpp recording](https://github.com/ggml-org/whisper.cpp/blob/f049fff95a089aa9969deb009cdd4892b3e74916/samples/jfk.wav)

The release sample keeps the original 16 kHz mono PCM16 frames and writes a
canonical 44-byte WAV header. The existing Node fixture can reproduce it
with `docs/qualification/life07o-whisper-lifecycle/prepare-actual.mjs`.

| File | SHA-256 |
| --- | --- |
| Upstream `jfk.wav` | `59dfb9a4acb36fe2a2affc14bacbee2920ff435cb13cc314a08c13f66ba7860e` |
| Bundled `qualification.wav` | `d7d4e74b8a333ed02186008bc109a1b1a19d16da668bd56e785d80d69a16a72f` |
