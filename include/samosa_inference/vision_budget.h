#ifndef SAMOSA_INFERENCE_VISION_BUDGET_H
#define SAMOSA_INFERENCE_VISION_BUDGET_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int max_side_len, admitted, memory_pressure, thermal_pressure;
    double available_gb;
    char reason[96];
} SamosaVisionResourceBudget;
typedef struct {
    double available_gb;
    int memory_pressure, thermal_pressure;
    long long total_bytes;
} SamosaVisionResourceSample;
/* Trusted synchronous host hooks, borrowed only until return. Omitted hooks
 * use the original native sampling, monotonic clock, sleep and no stop/trace.
 * Keep the context alive and do not reenter the same host operation in hooks.
 * Telemetry is decimal GB; total_bytes keeps the original host byte value. */
typedef struct {
    SamosaVisionResourceSample (*sample)(void *);
    long long (*now_ms)(void *);
    void (*sleep_ms)(void *, long);
    int (*stopping)(void *);
    void (*event)(void *, const char *, const char *);
    void *context;
} SamosaVisionBudgetControl;
long long samosa_vision_total_memory_bytes(void);
SamosaVisionResourceSample samosa_vision_host_sample(void);
SamosaVisionResourceBudget samosa_vision_resource_budget(const SamosaVisionBudgetControl *, int detail_needed);
SamosaVisionResourceBudget samosa_vision_wait_for_molmo_memory(const SamosaVisionBudgetControl *,
    int detail_needed, double required_gb, SamosaVisionResourceBudget initial);
/* Original positive resident-estimate plus adaptive reserve, or6.75 fallback.
 * Manifest path overflow retains the caller's distinct7.75 fallback below. */
#define SAMOSA_MOLMO2_MANIFEST_UNAVAILABLE_GB 7.75
double samosa_molmo2_required_available_gb(double resident_bytes, long long total_bytes);
#ifdef __cplusplus
}
#endif
#endif
