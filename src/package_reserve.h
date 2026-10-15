#ifndef SAMOSA_INFERENCE_PACKAGE_RESERVE_H
#define SAMOSA_INFERENCE_PACKAGE_RESERVE_H
#include "package_index.h"
#define PACKAGE_RESERVATION_CAPACITY 1024u
typedef struct {uint64_t reserved_bytes;size_t packages;} PackageReservationResult;
/* Persist a logical payload-copy reservation before assembly writes. Reservations
 * survive crashes and remain charged; nonempty-package reclamation requires an
 * owned cleanup protocol. Caller owns pool identity and the package lease.
 * This excludes cache bytes, filesystem allocation and metadata. */
const char *package_reserve(int,int,const PackageIndex *,uint64_t,size_t,PackageReservationResult *);
/* Poll trusted request control between bounded ledger observations and before
 * publication. Committed reservations remain charged if outer work cancels. */
const char *package_reserve_controlled(int,int,const PackageIndex *,uint64_t,size_t,
    const char *(*)(void *),void *,PackageReservationResult *);
/* Reclaim only a package that never created payload/journal state. Takes the
 * package assembly lease and pool lease; never removes package data. The exact
 * index/root/byte charge is required. Output 1 released, 0 already absent;
 * output untouched on failure. Caller retains named-root bindings. */
const char *package_reservation_release_empty(int,int,const PackageIndex *,
    const char *(*)(void *),void *,int *);
/* Trusted native removal validator, never a public request callback. Runs under
 * the assembly lease and again under both leases before exact-charge unlink. */
typedef const char *(*PackageReservationReleaseCheck)(int,const PackageIndex *,const char *(*)(void *),void *);
const char *package_reservation_release_checked(int,int,const PackageIndex *,PackageReservationReleaseCheck,
    const char *(*)(void *),void *,int *);
#endif
