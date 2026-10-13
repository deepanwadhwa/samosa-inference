#ifndef SAMOSA_ENGINE_PROCESS_H
#define SAMOSA_ENGINE_PROCESS_H
#include <sys/types.h>
#include <stddef.h>
typedef struct {pid_t guardian;int control,response;int terminal;char reason[64];} EngineProcess;
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
/* Owned transcription workspace: guardian removes input.wav/result.srt after
 * reaping the engine, including when the requesting process dies. */
const char *engine_process_start_bound_workspace(const char *,const char *,double,size_t,int,int,const char *,int,EngineProcess *);
/* Framed native summarizer transport: input read-only fd and output read/write
 * fd, separate from bounded stderr logs. Guardian enforces 8196 output bytes
 * and removes request.frame/result.frame from its retained private workspace. */
const char *engine_process_start_bound_protocol(const char *,const char *,double,size_t,int,int,const char *,int,int,int,EngineProcess *);
/* Trusted opt-in binary protocol, 1..16MiB result cap enforced by RLIMIT_FSIZE
 * in the guardian/worker. Keeps the legacy 8196-byte summary API unchanged. */
const char *engine_process_start_bound_protocol_sized(const char *,const char *,double,size_t,int,int,const char *,int,int,int,size_t,EngineProcess *);
const char *engine_process_stop(EngineProcess *);
/* Returns -1 for normal command dispatch, otherwise hidden guardian exit code. */
int engine_guardian_dispatch(int argc,char **argv);
#endif
