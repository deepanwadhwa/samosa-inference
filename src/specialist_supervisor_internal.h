#ifndef SAMOSA_SPECIALIST_SUPERVISOR_INTERNAL_H
#define SAMOSA_SPECIALIST_SUPERVISOR_INTERNAL_H
#include <samosa_inference/specialist_supervisor.h>
#include "engine_process.h"

int mm_session_start_admitted(SamosaMmSupervisor *, const SamosaMmProvider *,
    const SamosaMmControl *, SamosaMmSession *, EngineProcess *, int lease, char *, size_t);
void mm_session_publish(SamosaMmSession *);
int mm_session_probe_file(SamosaMmSession *, const void *, size_t, char *, size_t);
void mm_session_probe_control(SamosaMmSession *, const SamosaMmControl *);
void mm_session_set_verifier(SamosaMmSession *, const char *(*)(void *), void *);
SamosaMmProvider mm_visual_profile(int molmo, const char *engine, const char *model);

#endif
