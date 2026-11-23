#ifndef SAMOSA_INFERENCE_VISUAL_LIFECYCLE_H
#define SAMOSA_INFERENCE_VISUAL_LIFECYCLE_H
#include <samosa_inference/specialist_protocol.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SamosaVisualLifecycle SamosaVisualLifecycle;
typedef struct {
    const char *path, *sha256;
    uint64_t size;
} SamosaVisualInput;

enum { SAMOSA_VISUAL_VISIONPSY = 1, SAMOSA_VISUAL_MOLMO = 2 };
typedef struct {
    int profile;
    const char *state, *model_directory;
    SamosaVisualInput engine, kernel, anchor, probe;
    /* VisionPsy's config anchor is config.json. Supply the other pins in
     * this order: model.safetensors, tokenizer.json, preprocessor_config.json,
     * processor_config.json, tokenizer_config.json, chat_template.jinja.
     * Molmo's manifest.json anchor supplies the file list and has no extra
     * model_files entries. probe names a trusted, bounded prepared image. */
    const SamosaVisualInput *model_files;
    size_t model_file_count;
    const char *probe_prompt, *probe_expected;
} SamosaVisualLifecycleConfig;

/* The owning thread serializes operations. Start takes a zeroed or closed
 * original protocol session. Configuration backing storage stays valid and
 * immutable through destroy. Supervisor and session remain valid through
 * stop and confirmed cleanup. A supplied start control
 * and trace are borrowed through start's synchronous return. NULL denotes
 * success in lifecycle operations. */
const char *samosa_visual_lifecycle_create(const SamosaVisualLifecycleConfig *, SamosaVisualLifecycle **);
const char *samosa_visual_lifecycle_start(SamosaVisualLifecycle *, SamosaMmSupervisor *,
    SamosaMmSession *, const SamosaMmControl *, const SamosaMmTrace *);
const char *samosa_visual_lifecycle_status(const SamosaVisualLifecycle *);
const char *samosa_visual_lifecycle_stop(SamosaVisualLifecycle *);
const char *samosa_visual_lifecycle_destroy(SamosaVisualLifecycle *);

#ifdef __cplusplus
}
#endif
#endif
