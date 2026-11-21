#ifndef SAMOSA_INFERENCE_SPECIALIST_PROTOCOL_H
#define SAMOSA_INFERENCE_SPECIALIST_PROTOCOL_H

#include <samosa_inference/specialist_supervisor.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Host-owned diagnostics. The existing protocol owner borrows these callbacks
 * synchronously; it retains neither their context nor payload storage. */
typedef struct {
    void (*event)(void *, const char *, const char *);
    void (*payload)(void *, const char *, const char *, const char *, size_t);
    void *context;
} SamosaMmTrace;

enum { SAMOSA_MM_MOLMO2_MAX_IMAGES = 2, SAMOSA_MM_MOLMO2_MAX_FRAMES = 16 };

/* Original gateway launch profiles and diagnostic events. Paths/availability
 * are trusted caller configuration; selection/resource policy stays outside
 * this adapter. Start uses the inherited transport handshake, not public
 * actual-generation readiness (the resident registry owner qualifies that).
 * Session serialization and final close remain the caller's responsibility. */
int samosa_mm_visionpsy_start(SamosaMmSupervisor *, SamosaMmSession *,
    const SamosaMmTrace *, const char *engine, const char *model, int available);
int samosa_mm_molmo2_start(SamosaMmSupervisor *, SamosaMmSession *,
    const SamosaMmTrace *, const char *engine, const char *model, int available);
/* Default-launch sessions only; external lifecycle owners use close_owned and
 * retain their context until cleanup is confirmed, as documented there. */
void samosa_mm_visual_close(SamosaMmSession *, const SamosaMmTrace *);

/* Original Chat Molmo2 analyze operation, including its existing "gateway"
 * correlation ID, bounds, defaults, numeric validation and error vocabulary. */
int samosa_mm_molmo2_analyze_request(
    SamosaMmSession *, const SamosaMmTrace *, const char *media_path,
    const char *const *media_paths, int image_count, const char *media_kind,
    const char *prompt, int max_tokens, double start_seconds, double end_seconds,
    int max_frames, char *observation, size_t observation_capacity,
    int *prompt_tokens, int *generated_tokens, double *duration_seconds,
    int *frames, int *images, char *error_code, size_t error_capacity);

/* Original VisionPsy inspect operation. Session serialization and selected
 * image-path ownership remain with the caller, as before extraction. */
int samosa_mm_visionpsy_inspect(
    SamosaMmSession *, const SamosaMmTrace *, const char *image_path,
    const char *prompt, int max_side_len, int max_tokens,
    char *observation, size_t observation_capacity, int *prompt_tokens,
    int *generated_tokens, char *error_code, size_t error_capacity);

#ifdef __cplusplus
}
#endif
#endif
