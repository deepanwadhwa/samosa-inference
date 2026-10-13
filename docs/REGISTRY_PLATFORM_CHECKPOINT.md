# Independent registry platform checkpoint

Code revision `a9129183bde3c99d8cc44f5fb525545a649e0fcc` passed GitHub Actions
run [38486726588](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38486726588)
on macOS14 ARM64 and Ubuntu24.04 x86_64. Each job builds a clean `git archive`
export without Chat or Sources code. Both normal and ASan/UBSan stages passed
core primitives, all seven linked modality adapters, combined registry/CLI,
speech public controls, controlled text/acquisition and summary/Whisper/vision.
Optional actual-model Whisper/Vision/temporal harnesses compile independently;
this CI run does not execute installed actual models.

The combined registry passed219 assertions in each platform/build mode. Linux
native CLI passed430 normal/508 sanitizer checks; variable counts include polling
assertions. Linux visual adapter passed639 normal/769 sanitizer checks. Actual
embedding and temporal macOS evidence is separately recorded in
[REGISTRY_EMBEDDING_TEMPORAL_VERIFICATION.md](REGISTRY_EMBEDDING_TEMPORAL_VERIFICATION.md).
Actual text evidence is recorded in
[REGISTRY_TEXT_VERIFICATION.md](REGISTRY_TEXT_VERIFICATION.md).

The preceding CI run at `13e5143` passed Linux but failed a macOS sanitizer lease
observation. Its fixture lifetime was100ms including sanitizer startup; the test
now allows one second and prints the observed path on failure, retaining its
five-second terminal bound. The next run at `53b2805` passed macOS but failed Linux
while parsing a visual fixture's readiness marker. That fixture exposed the file
before completing JSON; it now publishes only closed JSON with atomic rename.
No model-engine runtime behavior or refusal contract was weakened by these test
fixes. The final successful run includes both fixes.

Local retained CI logs are `/private/tmp/samosa-registry-qualified-ci-mac.log`
and `/private/tmp/samosa-registry-qualified-ci-linux.log`. This checkpoint proves
the stated independent build and controlled-test coverage, not actual-model Linux
qualification, complete engine readiness, global resource scheduling, public
incremental events, network authentication or legacy Chat retirement. The full
seven-area goal and provider-integration acceptance remain active.
