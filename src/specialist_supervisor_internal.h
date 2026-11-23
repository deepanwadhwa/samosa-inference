#ifndef SAMOSA_SPECIALIST_SUPERVISOR_INTERNAL_H
#define SAMOSA_SPECIALIST_SUPERVISOR_INTERNAL_H
#include <samosa_inference/specialist_supervisor.h>
#include <samosa_inference/specialist_protocol.h>
#include "engine_process.h"

int mm_session_start_admitted(SamosaMmSupervisor *, const SamosaMmProvider *,
    const SamosaMmControl *, SamosaMmSession *, EngineProcess *, int lease,
    const char *workspace_root, char *, size_t);
void mm_session_publish(SamosaMmSession *);
int mm_session_probe_file(SamosaMmSession *, const void *, size_t, char *, size_t);
void mm_session_probe_control(SamosaMmSession *, const SamosaMmControl *);
void mm_session_set_verifier(SamosaMmSession *, const char *(*)(void *), void *);
SamosaMmProvider mm_visual_profile(int molmo, const char *engine, const char *model);
void mm_visual_trace_start(int molmo, const SamosaMmTrace *, const char *engine, const char *model);
void mm_visual_trace_failed(int molmo, const SamosaMmTrace *, const char *error);
void mm_visual_trace_ready(int molmo, const SamosaMmTrace *, pid_t);

#endif
