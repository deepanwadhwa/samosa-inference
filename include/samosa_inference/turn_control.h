#ifndef SAMOSA_INFERENCE_TURN_CONTROL_H
#define SAMOSA_INFERENCE_TURN_CONTROL_H
#include <samosa_inference/specialist_protocol.h>
#include <samosa_inference/vision_budget.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Original shared Gateway generation gate. One stable instance per host;
 * initialize a zeroed object and join all users before destroy. Do not copy a
 * live gate. This preserves exclusion/counting, not queues or global fairness. */
typedef struct { void *state; } SamosaTurnGate;
typedef struct { int generating, specialist_active, specialist_provider; } SamosaTurnStatus;
int samosa_turn_gate_init(SamosaTurnGate *);
void samosa_turn_gate_destroy(SamosaTurnGate *);
int samosa_turn_primary_begin(SamosaTurnGate *);
void samosa_turn_primary_end(SamosaTurnGate *);
int samosa_turn_generating(const SamosaTurnGate *);
SamosaTurnStatus samosa_turn_status(SamosaTurnGate *);
/* Preserve the direct visual backend's generating health flag, set after its
 * exclusive start and cleared after close. Idempotent; withdrawing this flag
 * preserves text requests admitted after visual exclusion was released. */
void samosa_turn_direct_generating(SamosaTurnGate *, int);

/* Existing turn runtime only. Planning, document evidence and selected model
 * configuration remain caller owned. A single request thread owns this object;
 * keep its gate and borrowed host context alive through release. */
typedef struct {
    SamosaVisionResourceBudget budget;
    SamosaMmSession session;
    int provider, generation_gate_held, backend_paused;
    int detail_needed, extended_visual;
} SamosaVisualTurn;

/* Trusted synchronous adapters to the existing host implementations. All hooks
 * are required. They preserve original availability/trace/probe/selection policy;
 * no public request may provide hooks or engine paths. Hooks must not reenter
 * this turn. now/sleep preserve the original60-second/100-ms resume wait. */
typedef struct {
    SamosaTurnGate *gate;
    int (*available)(void *, int);
    double (*molmo_required_gb)(void *);
    long long (*total_memory_bytes)(void *);
    int (*primary_running)(void *);
    void (*primary_stop)(void *);
    int (*primary_start)(void *);
    int (*primary_probe)(void *);
    int (*stopping)(void *);
    long long (*now_ms)(void *);
    void (*sleep_ms)(void *, long);
    SamosaVisionResourceBudget (*resource_budget)(void *, int);
    SamosaVisionResourceBudget (*wait_for_molmo_memory)(void *, int, double, SamosaVisionResourceBudget);
    int (*session_start)(void *, int, SamosaMmSession *);
    void (*session_close)(void *, SamosaMmSession *);
    int (*inspect)(void *, SamosaMmSession *, const char *, const char *, int, int,
                   char *, size_t, int *, int *, char *, size_t);
    int (*analyze_image)(void *, SamosaMmSession *, const char *, const char *, int,
                         char *, size_t, int *, int *, char *, size_t);
    void *context;
} SamosaVisualTurnControl;

int samosa_visual_turn_start(const SamosaVisualTurnControl *, SamosaVisualTurn *, char *, size_t);
void samosa_visual_turn_release(const SamosaVisualTurnControl *, SamosaVisualTurn *);
/* Only caller-prepared temporary pages set prepared_owned. Durable inputs are
 * borrowed. Retains original one-step lower-side retry and ownership give-back. */
int samosa_visual_turn_inspect(const SamosaVisualTurnControl *, SamosaVisualTurn *,
    const char *, int, const char *, int, char *, size_t, int *, int *, char *, size_t);
#ifdef __cplusplus
}
#endif
#endif
