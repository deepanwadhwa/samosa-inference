#ifndef SAMOSA_INFERENCE_PACKAGE_RESERVE_H
#define SAMOSA_INFERENCE_PACKAGE_RESERVE_H
#include "package_index.h"
#define PACKAGE_RESERVATION_CAPACITY 1024u
typedef struct {uint64_t reserved_bytes;size_t packages;} PackageReservationResult;
/* Persist a logical payload-copy reservation before assembly writes. Reservations
 * survive crashes and remain charged; removal/reclamation requires a future owned
 * package cleanup protocol. Caller owns pool identity and the package lease.
 * This excludes cache bytes, filesystem allocation and metadata. */
const char *package_reserve(int,int,const PackageIndex *,uint64_t,size_t,PackageReservationResult *);
#endif
