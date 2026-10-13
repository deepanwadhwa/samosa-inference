#ifndef SAMOSA_ENGINE_RUNTIME_H
#define SAMOSA_ENGINE_RUNTIME_H
/* Direct loader dependencies only. OS runtime is declared trusted. This does
 * not prove arbitrary executable code cannot dynamically open other files. */
const char *engine_system_runtime(int fd);
const char *engine_runtime_environment(void);
#endif
