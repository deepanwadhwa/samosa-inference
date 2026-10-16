#ifndef SAMOSA_DECISION_PACKAGE_DESCRIPTOR_H
#define SAMOSA_DECISION_PACKAGE_DESCRIPTOR_H
#include "decision_assets.h"
#include "package_index.h"
typedef struct {
    unsigned profile;
    char runtime_revision[41];
    size_t files[DECISION_ASSET_COUNT];
} DecisionPackageDescriptor;
/* Strict declaration/index binding only. The fixed assets/ subtree preserves
 * the existing four-file execution layout. Host policy, package tree, runtime
 * and loaded model qualification remain separate. Output untouched on error. */
const char *decision_package_descriptor_parse(const char *,size_t,const PackageIndex *,DecisionPackageDescriptor *);
#endif
