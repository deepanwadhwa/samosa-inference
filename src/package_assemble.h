#ifndef SAMOSA_INFERENCE_PACKAGE_ASSEMBLE_H
#define SAMOSA_INFERENCE_PACKAGE_ASSEMBLE_H
#include "package_tree.h"
#include "llama_package.h"
#include "whisper_package.h"
/* Trusted native acquisition adapter returns an owned read-only descriptor.
 * It must honor request controls during verification; caller closes the result.
 * Never supplied by public JSON or an untrusted package manifest. */
typedef const char *(*PackageSourceOpen)(void *,const PackageFile *,const char *(*)(void *),void *,int *);
typedef struct {
    int directory,source;
    PackageSourceOpen source_open;void *source_context;
    const PackageIndex *index;
    const char *payload_path;
    uint64_t copy_ceiling;
    /* Zero ceiling preserves legacy caller-owned reservations. Nonzero charges
     * the shared pool durably before copying; failed work remains charged. */
    int reservation_directory;uint64_t reservation_ceiling;size_t reservation_capacity;
    const char *(*control)(void *);
    void *context;
    /* Host-qualified policy required by the llama recipe, never manifest policy. */
    const LlamaPackagePolicy *llama_policy;
    const WhisperPackagePolicy *whisper_policy;
} PackageAssembly;
/* Assemble an acquired closed tree or trusted descriptor source into an installer-owned
 * private package directory. Serializes cooperating assemblies, resumes verified
 * files, and publishes only through the concrete backend compatibility gate.
 * copy_ceiling bounds payload copies only: the caller owns global reservation,
 * acquisition, directory identity and recovery of interrupted temporary files. */
const char *package_assemble(const PackageAssembly *,PackageTreeResult *);
typedef struct {const char *state;PackageTreeResult package;} PackageAssemblyStatus;
/* No state creation, reservation, acquisition or copying. Verified installation
 * proves observed bytes/static compatibility, never loaded readiness. Other
 * states carry no package receipt. Output is untouched on failure. */
const char *package_assembly_status(const PackageAssembly *,PackageAssemblyStatus *);
#endif
