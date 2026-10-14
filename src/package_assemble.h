#ifndef SAMOSA_INFERENCE_PACKAGE_ASSEMBLE_H
#define SAMOSA_INFERENCE_PACKAGE_ASSEMBLE_H
#include "package_tree.h"
typedef struct {
    int directory,source;
    const PackageIndex *index;
    const char *payload_path;
    uint64_t copy_ceiling;
    const char *(*control)(void *);
    void *context;
} PackageAssembly;
/* Assemble an already acquired, closed source tree into an installer-owned
 * private package directory. Serializes cooperating assemblies, resumes verified
 * files, and publishes only through the concrete backend compatibility gate.
 * copy_ceiling bounds payload copies only: the caller owns global reservation,
 * acquisition, directory identity and recovery of interrupted temporary files. */
const char *package_assemble(const PackageAssembly *,PackageTreeResult *);
#endif
