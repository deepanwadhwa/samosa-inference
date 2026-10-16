#ifndef SAMOSA_DECISION_PACKAGE_H
#define SAMOSA_DECISION_PACKAGE_H
#include "decision_package_descriptor.h"
typedef struct {
    unsigned profile;
    const char *runtime_revision;
    const char *pins[DECISION_ASSET_COUNT];
} DecisionPackagePolicy;
int decision_package_policy_valid(const DecisionPackagePolicy *);
/* Independent host pins, package-wide verification and the existing flat
 * execution-asset checks. Caller retains the package lease. No engine loads;
 * output remains unchanged on failure. */
const char *decision_package_verify(int,const char *,const PackageIndex *,const DecisionPackagePolicy *,
                                    const char *(*)(void *),void *,DecisionPackageDescriptor *);
#endif
