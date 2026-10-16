#ifndef SAMOSA_DECISION_ASSETS_H
#define SAMOSA_DECISION_ASSETS_H
#include <sys/stat.h>
enum {DECISION_ENGINE,DECISION_RUNTIME,DECISION_MODEL,DECISION_TOKENIZER,DECISION_ASSET_COUNT};
enum {DECISION_ASSETS_TEXT=0,DECISION_ASSETS_IMAGE=1};
typedef struct {
    int active,root,files[DECISION_ASSET_COUNT];
    unsigned profile;
    char path[4096],pins[DECISION_ASSET_COUNT][65];
    struct stat directory,identities[DECISION_ASSET_COUNT];
} DecisionAssets;
const char *decision_asset_name(unsigned);
/* Private flat execution assets, not an installer/catalog. Zero-initialize out.
 * Every file is pinned and held; engine/runtime loader closure uses the existing
 * native graph policy. Failures leave out unchanged and close temporary fds. */
const char *decision_assets_open(const char *,const char *const[DECISION_ASSET_COUNT],
    const char *(*)(void *),void *,DecisionAssets *out);
/* Explicit image profile admits the pinned float32 SigLIP graph (2 GiB cap).
 * The default text profile retains its 1 GiB cap and all shared file policy. */
const char *decision_assets_open_profile(const char *,const char *const[DECISION_ASSET_COUNT],
    unsigned profile,const char *(*)(void *),void *,DecisionAssets *out);
const char *decision_assets_recheck(const DecisionAssets *,const char *(*)(void *),void *);
void decision_assets_close(DecisionAssets *);
#endif
