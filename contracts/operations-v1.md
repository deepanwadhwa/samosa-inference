# Owner-scoped operations v1

Count_tokens and generate accept optional operation_id (opaque ASCII<=128).
Host-configured owner and private admission directory grant authority. Normalized
request identity includes op, provider, context/model alias, decoded prompt,
output reserve and session/revision. Reordered JSON retries are equivalent;
changed parameters conflict. Tracked responses carry operation_id and are saved
before stdout. Exact finished retry returns the original response without queue,
provider or session work. Invalid/conflicting requests cannot adopt another owner.

When operation_parents is advertised, tracked count/generate also accept optional
parent_operation_id (nonempty opaque ASCII<=128, different from operation_id).
It requires operation_id, participates in normalized request identity and is
atomically bound to the operation. Qualified results and operation_get echo it.
Changing a parent conflicts; a parent grants no authority and cannot cancel a
group. Valid references accompany early tracked admission refusals as well.

operation_get/operation_cancel take contract_version1,op,operation_id. Get returns
running/completed/failed/cancelled/interrupted and cancel_requested. Cancel targets
only the configured owner's operation; unknown/foreign IDs refuse. Cancellation
requests may race completion; the reported final status is authoritative. Queue
waiters poll their own descriptor and remove only their own ticket. Native curl
progress aborts only that transfer. Failure/cancellation preserves last-good
semantic session data. A committed success is not rolled back by later cancel.

Kernel per-operation lease proof identifies worker death without PID assumptions.
Dead running operations become interrupted and release byte reservations; retry
never silently reissues model work. New operation IDs express new work. Histories
remain retained. Physical effect records and conversation truth are outside this
provider-owned state.

Bounds128 histories,8MiB aggregate results including512KiB reservations per live
operation,16MiB SQLite, fixed private lease/cancel files, no eviction. Result
digests and version/operation envelopes are checked on hits. Dead reservations
are reaped before new admission. Queue/native HTTP limits remain separately
applicable. No public caller reads this private database or chooses descriptor
paths, endpoints, model files or another owner's cancellation target.
