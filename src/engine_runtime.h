#ifndef SAMOSA_ENGINE_RUNTIME_H
#define SAMOSA_ENGINE_RUNTIME_H
/* Direct loader dependencies only. OS runtime is declared trusted. This does
 * not prove arbitrary executable code cannot dynamically open other files. */
const char *engine_system_runtime(int fd);
const char *engine_runtime_environment(void);
/* Shared OS trust boundary for private native package graph inspection. */
int engine_trusted_system_library(int mach_o, const char *name);
#endif
