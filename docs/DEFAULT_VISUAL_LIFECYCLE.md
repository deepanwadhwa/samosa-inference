# Default visual lifecycle

The native VisionPsy and Molmo sessions can now start through Activation
using the original Specialist supervisor and protocol transports. Start
verifies the system loader, pinned engine, colocated Metal library, model
files and a caller-supplied prepared image. The original transports run a
bounded image probe and check an expected excerpt in generated evidence.
Activation commits last-good only after that succeeds.

The same guarded process handles the turn after qualification. It retains
the Activation lease until process-group reaping and workspace cleanup.
The supervisor reserves the specialist during start and publishes its PID
after qualification commits. Requests recheck pinned input identities
before the exchange and before returning a reply. Stop uses the original
grace and reaping behavior. Unknown cleanup keeps its reservation and
activation fence.

The managed profiles use the original VisionPsy line protocol, 64 KiB
limit, 120-second request deadlines and adaptive sides up to 2,048. Molmo
keeps its 1 MiB framed transport, 600-second request deadlines and original
single image, joint image and native-video inputs. Prepared PDF pages still
use the original temporary-image handoff and guardian cleanup. The existing
structured visual owner shares Molmo's cheap live manifest/file check.

The existing native turn driver also exercises the managed start and stop.
Build "build/test-default-visual-lifecycle-real" and its "-sanitize"
counterpart. The arguments are absolute VisionPsy executable, VisionPsy
model directory, Molmo executable, Molmo model directory, directory with
"hello.ppm", "red.ppm" and "blue.ppm", and the retained native video.
Set "SAMOSA_VISUAL_ACTUAL_RECEIPT" to the output JSON path. The existing
source pins and native inputs are recorded in the previous turn evidence.
[Original native turn fixture](qualification/life07n-turn-control/README.md).

The Mac installer builds this owner as part of the Inference archive. In a
versioned release, Gateway passes the manifest pins for the default runtime,
MLX kernel and startup probe to the owner. VisionPsy receives model file pins
from the release's verified catalog. Molmo receives a pin for its local
native package manifest, which Inference checks against the full file
inventory. Explicit executable overrides use the compatibility path.
[Packaged startup probes](../assets/visual/README.md).

The owner keeps a model verification receipt across lower-resolution
retries. It can reuse the receipt only while the directory and each
input retain their verified identity and metadata. A mismatch forces
full digest verification before launch. Gateway forwards cancellation
during preparation and uses the same turn and shutdown callbacks.

The specialist uses the shared process owner's preparation lock and
admission receipt. The versioned deployment stages its workspace in the
provider's Activation state directory. Compatibility launches use a private
per-user directory under the system temporary directory. The next launch
reclaims an abandoned preparation from that root. Live locks and admitted
workspaces exclude them from the preparation sweep. The original guardian
still cleans prepared pages and the workspace after it can confirm reaping.
Recovery after losing a guardian stays open.
