# Read-only native package installation status v1

`package_assembly_status` takes trusted directory/index/payload-path configuration
and an output `PackageAssemblyStatus`. It never creates state, reserves capacity,
acquires artifacts, copies files or repairs metadata. Source descriptors/callbacks,
copy ceilings and reservation configuration are not required for inspection.
Request controls remain authoritative; errors leave output untouched.

| State | Observed meaning | Package receipt |
| --- | --- | --- |
| `unregistered` | Assembly lease file or persistent identity is absent; existing files are not qualified | Empty |
| `busy` | A held, bound private assembly lock currently refuses exclusive acquisition | Empty |
| `incomplete_unverified` | Persistent index/root identity exists, but completion is absent | Empty |
| `installed_verified` | Identity, complete payload, concrete backend compatibility and completion record were reverified | Verified tree observations |

A stale lease file alone never indicates active work. Corrupt or foreign metadata,
changed identity, unsafe paths and corrupted completed payloads return errors rather
than a verified state. `incomplete_unverified` does not promise safe resumption or
claim that a worker is running. `unregistered` does not mean the directory is empty.
Busy state makes no payload-integrity claim. Completed queries recheck named root
bindings and persistent identity after controlled whole-payload verification.

All results are observations under cooperating assembly/publication leases, not
future immutability against arbitrary same-user writers. `installed_verified`
proves package bytes/static compatibility, never runtime loading or model readiness.
This native API is a prerequisite for public catalog/operation status, not the
complete authenticated installation service. The caller retains private owner and
named-package-root configuration.
