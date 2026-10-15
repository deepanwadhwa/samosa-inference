#ifndef SAMOSA_INFERENCE_MOLMO_PACKAGE_DESCRIPTOR_H
#define SAMOSA_INFERENCE_MOLMO_PACKAGE_DESCRIPTOR_H
#include "package_index.h"
#include "temporal_package.h"
typedef struct {size_t engine,kernels,manifest;} MolmoPackageDescriptor;
/* Metadata resolution only. Does not hash files, qualify a runtime or load MLX.
 * Fixed model layout matches the existing native temporal provider. */
const char *molmo_package_descriptor_parse(const char *,size_t,const PackageIndex *,MolmoPackageDescriptor *);
/* Bind the existing temporal verifier's receipt to the complete generic index.
 * Every model-directory file must have the same name, size, digest and role.
 * Call only after temporal_package_verify succeeds on the bound model directory. */
const char *molmo_package_bind_model(const PackageIndex *,const TemporalPackageReceipt *);
#endif
