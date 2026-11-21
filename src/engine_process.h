#ifndef SAMOSA_ENGINE_PROCESS_H
#define SAMOSA_ENGINE_PROCESS_H
#include <sys/types.h>
#include <stddef.h>
typedef struct {pid_t guardian,engine;int server;int control,response;int terminal;char reason[64];} EngineProcess;
/* argv_json is trusted host configuration, never a public model request.
 * Lifecycle ownership does not establish verified artifact identity/readiness. */
const char *engine_process_start(const char *self,const char *argv_json,
                                double lifetime_seconds,size_t log_byte_limit,
                                EngineProcess *);
/* Guardian retains a duplicate of the caller's host residency lease until its
 * engine has been killed/reaped, including after the caller dies. */
const char *engine_process_start_leased(const char *,const char *,double,size_t,int,EngineProcess *);
/* Bound model is inherited read-only as fd5, usable through /dev/fd/5. */
const char *engine_process_start_bound(const char *,const char *,double,size_t,int,int,EngineProcess *);
/* Resident protocol: borrow a read-only input FIFO and write-only output FIFO.
 * Guardian retains model/lease and owns group/parent-death cleanup, separating
 * stdout protocol from bounded stderr. Caller owns framing, request limits and
 * cancellation/deadlines and closes its protocol descriptors after stop.
 * Kernel pipe capacity bounds buffering; startup alone is not readiness. */
const char *engine_process_start_bound_duplex(const char *,const char *,double,size_t,int,int,int,int,EngineProcess *);
/* Same resident duplex ownership plus the existing retained workspace cleanup
 * after reap/parent death. Matches the workspace path to its private directory
 * descriptor. Pipe buffering and caller framing bound protocol output; bounded
 * stderr and lifetime remain guardian-owned. Preserves duplex's file-size policy
 * so OS-managed Metal caches can grow independently of protocol bytes. */
const char *engine_process_start_bound_workspace_duplex(const char *,const char *,double,size_t,int,int,const char *,int,int,int,EngineProcess *);
/* Owned transcription workspace: guardian removes input.wav/result.srt after
 * reaping the engine, including when the requesting process dies. */
const char *engine_process_start_bound_workspace(const char *,const char *,double,size_t,int,int,const char *,int,EngineProcess *);
#define ENGINE_VISUAL_RESULT_MAX 32768u
/* GPU output is copied by the guardian from a dedicated bounded pipe. Preserve OS-managed
 * Metal cache writes while retaining guardian lifetime/log/group ownership. */
const char *engine_process_start_bound_workspace_captured(const char *,const char *,double,size_t,int,int,const char *,int,int,EngineProcess *);
/* Framed native summarizer transport: input read-only fd and output read/write
 * fd, separate from bounded stderr logs. Guardian enforces 8196 output bytes
 * and removes request.frame/result.frame from its retained private workspace. */
const char *engine_process_start_bound_protocol(const char *,const char *,double,size_t,int,int,const char *,int,int,int,EngineProcess *);
/* Trusted opt-in binary protocol, 1..16MiB result cap enforced by RLIMIT_FSIZE
 * in the guardian/worker. Keeps the legacy 8196-byte summary API unchanged. */
const char *engine_process_start_bound_protocol_sized(const char *,const char *,double,size_t,int,int,const char *,int,int,int,size_t,EngineProcess *);
/* Existing primary HTTP backend compatibility profile: no lifetime/log cap.
 * Child receives private bootstrap configuration fd9 and original output/error
 * descriptors. Retained ownership lease protects PID-marker cleanup until reap.
 * Startup identifies the actual worker; it does not establish model readiness. */
typedef struct {int configuration,output,error;const char *marker;} EngineServer;
const char *engine_process_start_server(const char *,const char *,int,const EngineServer *,EngineProcess *);
/* Original on-demand CLI compatibility: no clip/lifetime/file/log cap is
 * invented. stdout/stderr go to /dev/null; host validates its existing result
 * size/format while the guardian reserves PID/group/workspace until stop.
 * Caller must keep the record until stop finishes, including pending cleanup. */
const char *engine_process_start_command(const char *,const char *,const char *,int,EngineProcess *);
/* Original self-executable discovery; caller supplies PATH_MAX bytes. */
int engine_process_executable(char *);
/* Nonblocking terminal observation; server identity stays reserved until stop. */
int engine_process_running(EngineProcess *);
const char *engine_process_stop(EngineProcess *);
/* Returns -1 for normal command dispatch, otherwise hidden guardian exit code. */
int engine_guardian_dispatch(int argc,char **argv);
#endif
