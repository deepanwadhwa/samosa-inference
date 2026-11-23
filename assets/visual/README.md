# Native visual qualification inputs

- `visionpsy.ppm.gz` is the original synthetic test page with a large HELLO
  heading. It expands to 9,270,017 bytes and has SHA-256
  `ba35794c95632ba6e34044f1397a23ba4f5eff8d6064c1bd4eef029efc19f7d6`.
- `molmo.ppm` is the original synthetic red pixel. Its 14 bytes have SHA-256
  `8dee30e2caa26a23f15a2e903788b34b4bd247a15ccb4feed7ec8b3db1bc71ee`.

The Mac release stages the uncompressed PPM inputs and records their byte counts
and digests in the release manifest. Inference uses the images to check the
specialist's startup response before the worker can handle a user request.
Both images come from the existing native turn test.
[Native turn qualification](../../docs/qualification/life07n-turn-control/README.md).
