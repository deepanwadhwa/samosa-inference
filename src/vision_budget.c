#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/vision_budget.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <notify.h>
#endif

static double vision_available_memory_gb(void) {
#if defined(__APPLE__)
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_statistics64_data_t vm;
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                          (host_info64_t)&vm, &count) != KERN_SUCCESS) return 0.0;
    double pages = (double)vm.free_count + (double)vm.inactive_count + (double)vm.purgeable_count;
    return pages * (double)sysconf(_SC_PAGESIZE) / 1e9;
#else
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 0.0;
    char line[256]; double kb = 0.0;
    while (fgets(line, sizeof(line), f))
        if (sscanf(line, "MemAvailable: %lf", &kb) == 1) break;
    fclose(f);
    return kb / 1e6;
#endif
}

static int vision_memory_pressure(void) {
#if defined(__APPLE__)
    int level = 0; size_t length = sizeof(level);
    if (!sysctlbyname("kern.memorystatus_vm_pressure_level", &level, &length, NULL, 0)) return level;
#endif
    return 0;
}

long long samosa_vision_total_memory_bytes(void) {
#if defined(__APPLE__)
    uint64_t bytes = 0; size_t length = sizeof(bytes);
    if (!sysctlbyname("hw.memsize", &bytes, &length, NULL, 0) &&
        bytes <= (uint64_t)LLONG_MAX) return (long long)bytes;
#elif defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_PHYS_PAGES), size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && size > 0 && pages <= LLONG_MAX / size)
        return (long long)pages * size;
#endif
    return 0;
}

static int vision_thermal_pressure(void) {
#if defined(__APPLE__)
    int token = 0; uint64_t state = 0;
    if (notify_register_check("com.apple.system.thermalpressurelevel", &token) == NOTIFY_STATUS_OK) {
        (void)notify_get_state(token, &state);
        notify_cancel(token);
        return (int)state;
    }
#endif
    return 0;
}

SamosaVisionResourceSample samosa_vision_host_sample(void) {
    SamosaVisionResourceSample sample;
    sample.available_gb = vision_available_memory_gb();
    sample.memory_pressure = vision_memory_pressure();
    sample.thermal_pressure = vision_thermal_pressure();
    sample.total_bytes = samosa_vision_total_memory_bytes();
    return sample;
}

static long long budget_now(const SamosaVisionBudgetControl *control) {
    if (control && control->now_ms) return control->now_ms(control->context);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}
static void budget_sleep(const SamosaVisionBudgetControl *control, long ms) {
    if (control && control->sleep_ms) { control->sleep_ms(control->context, ms); return; }
    if (ms <= 0) return;
    struct timespec pause = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    while (nanosleep(&pause, &pause) && errno == EINTR) {}
}
static int budget_stopping(const SamosaVisionBudgetControl *control) {
    return control && control->stopping && control->stopping(control->context);
}
static void budget_trace(const SamosaVisionBudgetControl *control, const char *event, const char *fields) {
    if (control && control->event) control->event(control->context, event, fields);
}

SamosaVisionResourceBudget samosa_vision_resource_budget(const SamosaVisionBudgetControl *control, int detail_needed) {
    SamosaVisionResourceBudget b;
    memset(&b, 0, sizeof(b));
    b.admitted = 1;
    SamosaVisionResourceSample sample = control && control->sample ? control->sample(control->context) : samosa_vision_host_sample();
    b.available_gb = sample.available_gb;
    b.memory_pressure = sample.memory_pressure;
    b.thermal_pressure = sample.thermal_pressure;
    long long total_bytes = sample.total_bytes;
    double total_gb = total_bytes > 0 ? (double)total_bytes / 1e9 : 0.0;

    /* Available memory already includes the active LLM and desktop workload.
       The total-memory cap keeps a 16 GB machine away from the 17-tile peak
       while leaving the 2048 standard path available on roomier hosts. */
    int side = 2048;
    if ((total_gb > 0.0 && total_gb <= 18.0) || (b.available_gb > 0.0 && b.available_gb < 8.0)) side = 1536;
    if ((b.available_gb > 0.0 && b.available_gb < 5.5) || b.memory_pressure >= 2) side = 1024;
    if ((b.available_gb > 0.0 && b.available_gb < 3.5) || b.memory_pressure >= 4 || b.thermal_pressure >= 2) side = 512;
    if (detail_needed && side == 512 && b.available_gb >= 4.0 && b.memory_pressure < 4) side = 1024;
    b.max_side_len = side;
    /* Do not load the measured ~1.1 GB helper into a critically pressured
       Mac. A text/OCR result can still continue as an honest partial answer;
       a visual-only turn receives a retryable error instead of forcing swap
       or risking an OS-level process kill. Unknown telemetry stays usable. */
    if ((b.available_gb > 0.0 && b.available_gb < 2.5) ||
        b.memory_pressure >= 4 || b.thermal_pressure >= 3)
        b.admitted = 0;
    snprintf(b.reason, sizeof(b.reason), "available=%.2fGB memory_pressure=%d thermal=%d",
             b.available_gb, b.memory_pressure, b.thermal_pressure);
    char fields[320];
    snprintf(fields, sizeof(fields),
             "\"admitted\":%s,\"max_side_len\":%d,\"detail_needed\":%s,"
             "\"available_gb\":%.3f,\"total_gb\":%.3f,\"memory_pressure\":%d,"
             "\"thermal_pressure\":%d,\"reason\":\"%s\"",
             b.admitted ? "true" : "false", b.max_side_len,
             detail_needed ? "true" : "false", b.available_gb, total_gb,
             b.memory_pressure, b.thermal_pressure, b.reason);
    budget_trace(control, "vision_resource_budget", fields);
    return b;
}

SamosaVisionResourceBudget samosa_vision_wait_for_molmo_memory(
    const SamosaVisionBudgetControl *control, int detail_needed, double required_gb,
    SamosaVisionResourceBudget initial) {
    SamosaVisionResourceBudget current = initial;
    if (required_gb <= 0.0 || current.available_gb <= 0.0 ||
        current.available_gb >= required_gb ||
        current.memory_pressure >= 4 || current.thermal_pressure >= 3)
        return current;

    const long long started = budget_now(control);
    const long long deadline = started + 3000;
    double peak_gb = current.available_gb;
    int samples = 0;
    while (!budget_stopping(control) && budget_now(control) < deadline) {
        budget_sleep(control, 100);
        current = samosa_vision_resource_budget(control, detail_needed);
        ++samples;
        if (current.available_gb > peak_gb) peak_gb = current.available_gb;
        if (current.available_gb <= 0.0 || current.available_gb >= required_gb ||
            current.memory_pressure >= 4 || current.thermal_pressure >= 3)
            break;
    }

    char fields[256];
    snprintf(fields, sizeof(fields),
             "\"required_gb\":%.3f,\"available_gb\":%.3f,\"peak_gb\":%.3f,"
             "\"waited_ms\":%lld,\"samples\":%d,\"admitted\":%s",
             required_gb, current.available_gb, peak_gb,
             budget_now(control) - started, samples,
             current.admitted &&
                 (current.available_gb <= 0.0 || current.available_gb >= required_gb)
                 ? "true" : "false");
    budget_trace(control, "molmo2_memory_reclaim_wait", fields);
    return current;
}

double samosa_molmo2_required_available_gb(double resident_bytes, long long total_bytes) {
    double total_gb = total_bytes > 0 ? (double)total_bytes / 1e9 : 0.0;
    /* A fixed 4-GB reserve made the verified 3.76-GB Q4 package impossible to
       admit on the qualified 16-GiB Mac even after the primary model had fully
       exited: normal-pressure telemetry stabilized around 7.2 GB available.
       Keep 3 GB on that tightly bounded profile (the general vision gate still
       rejects critical pressure/thermal state), and retain 4 GB on roomier
       hosts. This is never co-resident with the primary backend. */
    double reserve_gb = total_gb > 0.0 && total_gb <= 18.0 ? 3.0 : 4.0;
    double required = resident_bytes > 0
        ? resident_bytes / 1e9 + reserve_gb : 6.75;
    return required;
}
