#ifndef SAMOSA_INFERENCE_LIBRARY_RUNTIME_H
#define SAMOSA_INFERENCE_LIBRARY_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
#define LIBRARY_DEPENDENCIES 64
/* Private deterministic parser policy. The production host fills this from
 * the compiled architecture and observed OS, never from public requests. */
typedef struct {
    int mach_o;
    uint32_t cpu, os_version, glibc_version;
} LibraryRuntimePolicy;
typedef struct {
    int mach_o, origin_search;
    uint32_t minimum_os, minimum_glibc, current_version;
    size_t count;
    struct { char name[256]; uint32_t compatibility_version; } dependencies[LIBRARY_DEPENDENCIES];
} LibraryRuntimeImage;
const char *library_runtime_host_policy(LibraryRuntimePolicy *);
/* No execution, shell tools or loader contact. Bounded native image parsing.
 * Accepts thin 64-bit little-endian shared images and a closed sibling-library
 * search profile (@loader_path / $ORIGIN), plus trusted system dependencies. */
const char *library_runtime_inspect(int fd, const LibraryRuntimePolicy *, LibraryRuntimeImage *);
/* Executable counterpart for closed native engine packages. Requires a host
 * interpreter/entry profile; observes dependencies without loading or launching. */
const char *library_runtime_inspect_executable(int fd,const LibraryRuntimePolicy *,LibraryRuntimeImage *);
enum LibraryRuntimeKind {LIBRARY_RUNTIME_OTHER,LIBRARY_RUNTIME_LIBRARY,LIBRARY_RUNTIME_EXECUTABLE};
typedef struct {const char *name;enum LibraryRuntimeKind kind;} LibraryRuntimeFile;
/* Resolve inspected dependencies against a verified closed package file list.
 * Caller verifies every native image and revalidates package bytes afterwards. */
const char *library_runtime_graph(const LibraryRuntimePolicy *,const LibraryRuntimeFile *,
    const LibraryRuntimeImage *,size_t,const char *(*)(void *),void *);
#endif
