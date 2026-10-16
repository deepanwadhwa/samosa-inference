#ifndef SAMOSA_DECISION_ASSETS_H
#define SAMOSA_DECISION_ASSETS_H
#include <sys/stat.h>
enum {DECISION_ENGINE,DECISION_RUNTIME,DECISION_MODEL,DECISION_TOKENIZER,DECISION_ASSET_COUNT};
typedef struct {
    int active,root,files[DECISION_ASSET_COUNT];
    char path[4096],pins[DECISION_ASSET_COUNT][65];
    struct stat directory,identities[DECISION_ASSET_COUNT];
} DecisionAssets;
const char *decision_asset_name(unsigned);
/* Private flat execution assets, not an installer/catalog. Zero-initialize out.
 * Every file is pinned and held; engine/runtime loader closure uses the existing
 * native graph policy. Failures leave out unchanged and close temporary fds. */
const char *decision_assets_open(const char *,const char *const[DECISION_ASSET_COUNT],
    const char *(*)(void *),void *,DecisionAssets *out);
const char *decision_assets_recheck(const DecisionAssets *,const char *(*)(void *),void *);
void decision_assets_close(DecisionAssets *);
#endif
