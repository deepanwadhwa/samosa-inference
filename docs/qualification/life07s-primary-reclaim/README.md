# Gateway uses Inference primary lifecycle for pinned deployments

LIFE-07s, 2026-11-22. Chat baseline `73da086`, Inference baseline `6a8f215`.

Gateway passes its original selected model, runtime settings, ports, logs, and
turn gate to the existing Inference primary owner. When the deployment supplies
pins, startup verifies and qualifies in the background while the control plane
serves requests. Model selection keeps its existing asynchronous job and
rollback. Its pinned path reports ready only after the worker completes a real
generation request. A failed probe restores the previously active model. The pinned selection job
allows up to ten minutes for hashing and qualification while retaining the
existing twenty-second worker startup limit. The Advanced settings handler waits for its candidate so a verification
or launch failure restores the previous settings.

Visual reclaim uses the existing turn reservation. Pause stops and reaps the
primary worker while the owner retains the verified input identities and
Activation lease. The specialist starts only after cleanup is confirmed.
Resume checks the retained identities, starts a fresh worker, and qualifies it
through the existing transport before releasing the turn reservation. It keeps
the prior last-good revision and avoids hashing the entire model again.
Mutation while paused prevents restart. Cancellation checks the launch flag
before publishing the qualification socket, so shutdown can interrupt it as
soon as the socket is visible.

A trusted native deployment sets `SAMOSA_PRIMARY_PINS` to a manifest owned by
the Gateway user. Use the release manifest row format with resolved absolute
paths. Each row has a lowercase SHA-256, a decimal byte count, and the exact
configured path, separated by tabs. Include each primary executable, model
anchor, model inputs, and the declared native libraries. For the retained Qwen
build the manifest includes the installed libomp at its declared path. A Maple
deployment includes the Metal library beside the executable. The manifest
applies to every primary backend selected in that Gateway process. Invalid
pins or missing required inputs refuse activation. Existing installs without a
manifest use their existing primary startup. Deployments can migrate in place
as their release and local installation metadata supply verified pins.

New verification uses the same native launch/guardian, generation transport,
Activation database, selection route, and visual turn code. Source packaging
stages the original Inference implementations and the Mac installer builds and
links them. The standalone C archive builds separately from Gateway.

| Gate | Evidence |
| --- | --- |
| Native API and retained identity | `native-qualified-macos.log`, 174 primary fixture assertions each normal and ASan/UBSan. Includes visual reclaim, exact last-good preservation, restart refusal after mutation, and original failure cases. |
| Turn and compatibility checks | Same log. Original turn, Activation, and JSON transport suites pass. Extra turn assertion refuses startup while primary shutdown is pending. |
| Independent source build | `standalone-macos.log`, full primary fixture in both modes from 41 copied source and build files. |
| Gateway HTTP fixture | `gateway-fixture-qualified-macos.log`, pinned startup, selected Bonsai fixture, failed Ornith generation and rollback to Bonsai, stale input refusal, and process/marker cleanup. |
| Actual Gateway startup | `gateway-actual-qualified-macos.log`, retained native Qwen generation before ready and graceful worker cleanup. |
| Actual primary reclaim | `actual-reclaim-macos.log`, native Qwen through the existing visual reservation using a controlled specialist fixture. Verifies a reaped PID and new qualified PID at the same revision. The final ASan/UBSan host result is in `actual-reclaim-qualified-sanitize-macos.log`. |
| Original Gateway | `chat-qualified-macos.log` and `selection-macos.log`, runtime settings, 15 prompt-budget cases, and original selection/rollback workflows. |
| Packaging and installation | `package-macos.log` verifies exact staged bytes and hashes. `install-qualified-macos.log` verifies the runtime-only installer build and startup on this Mac. |

Run the Gateway fixture from Chat after building `samosa-gateway` and the
Inference `test-primary-host` fixture. Use `tests/test_primary_gateway.mjs`
under `tools/without_python.sh`. The same script accepts `--actual` under
`tools/run_memory_guarded.sh` with the previously retained Qwen inputs and
OpenMP. The primary archive's actual reclaim test uses the existing
LIFE-07r `qualify-qwen.mjs` driver and the current Inference fixture.

Remaining primary work includes producing pinned inputs for ordinary installed
deployments by reusing their existing installation receipts and qualified
catalog entries. Maple and llama still need actual native model runs. The
actual reclaim gate uses a controlled specialist fixture and qualifies Qwen
process reclamation and resume. Application smoke with actual concurrent
primary and visual workloads remains open. The lifecycle audit also retains
default visual controls, long-window Whisper, and generic unknown-outcome
recovery. Continue the ordered refactor through the later Inference areas,
Agent/App, pinned assembly and qualified legacy retirement.
