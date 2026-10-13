# Inference owned engine lifecycle v1

The native Inference process library starts only trusted host-configured argv,
not public request-supplied commands. At most32 arguments/16KiB encoded JSON,
4KiB per decoded argument, absolute executable/self paths. A hidden self worker
requires its inherited private fd3 lifeline. Default and native Inference binaries
dispatch this worker before normal CLI parsing; no stdin model request starts it.

Start handshake is bounded2 seconds and proves exec, not model readiness.
The guardian starts its direct engine child in a new process group. It owns the
group until its leader is reaped, polling waitid/WNOWAIT. It kills the group before
reaping, so group identifiers cannot be recycled before cleanup. Parent lifeline
closure/explicit stop, child failure/exit, configured lifetime<=86400s or log
byte refusal triggers group cleanup. No external/discovered PID is adopted.
Logs are drained/discarded in fixed4KiB reads, with configured processed byte
limit<=1MiB. No unbounded log file or retained model output lives in this layer.

Stop closes the lifeline and observes a bounded2-second terminal response plus
bounded2-second reap. Terminal reason is retained in the caller-owned handle;
repeated stop describes the same terminal result. A stalled guardian returns
engine_shutdown_pending and keeps its handle/descriptors for later observation.
Start failure may also return pending ownership; callers must retain that handle.
Blocking host filesystem/exec/kill syscalls have host responsiveness limitations.

Parent SIGKILL and engine SIGKILL are separate tests; descendants holding actual
leases must die while an unrelated fixture remains alive. SIGKILL of the guardian
itself is not yet qualified for cleanup. There is no durable activation record,
verified engine/model input binding, readiness proof or residency switch in this
primitive. Those remain required S13a integration gates. All public provider
status still reports artifact_identity=unqualified_external_engine/ready=false.
