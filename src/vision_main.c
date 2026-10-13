#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "engine_process.h"
#include "inference.h"
#include "vision.h"
#include <limits.h>
int main(int argc, char **argv) {
    int guardian = engine_guardian_dispatch(argc, argv);
    if (guardian >= 0)
        return guardian;
    int bridge = vision_bridge_dispatch(argc, argv);
    if (bridge >= 0)
        return bridge;
    char self[PATH_MAX], model_identity[65];
    if (!realpath(argv[0], self))
        return 2;
    VisionConfig vision = {.self = self};
    InferenceConfig cfg = {.provider_id = "visionpsy-native-v1",
                           .context_tokens = 131072,
                           .owner = "local",
                           .vision = &vision};
    for (int i = 1; i < argc; i++) {
        if (i + 1 >= argc)
            return 2;
        const char *flag = argv[i], *value = argv[++i];
        if (!strcmp(flag, "--vision-engine"))
            vision.executable = value;
        else if (!strcmp(flag, "--vision-model"))
            vision.model = value;
        else if (!strcmp(flag, "--vision-engine-sha256"))
            vision.executable_sha256 = value;
        else if (!strcmp(flag, "--vision-config-sha256"))
            vision.config_sha256 = value;
        else if (!strcmp(flag, "--vision-tokenizer-sha256"))
            vision.tokenizer_sha256 = value;
        else if (!strcmp(flag, "--vision-weights-sha256"))
            vision.weights_sha256 = value;
        else if (!strcmp(flag, "--provider-id"))
            cfg.provider_id = value;
        else if (!strcmp(flag, "--admission-state"))
            cfg.admission_state = value;
        else if (!strcmp(flag, "--owner"))
            cfg.owner = value;
        else if (!strcmp(flag, "--queue-capacity") || !strcmp(flag, "--admission-wait-ms")) {
            char *end;
            errno = 0;
            unsigned long n = strtoul(value, &end, 10);
            int queue = !strcmp(flag, "--queue-capacity");
            if (errno || end == value || *end || n > (queue ? 8u : 60000u) || (!queue && !n))
                return 2;
            if (queue)
                cfg.queue_capacity = (size_t)n;
            else
                cfg.admission_wait_ms = (size_t)n;
        } else
            return 2;
    }
    if (!vision.config_sha256 || !vision.tokenizer_sha256 || !vision.weights_sha256)
        return 2;
    vision_model_identity(&vision, model_identity);
    if (!*model_identity)
        return 2;
    vision.model_sha256 = model_identity;
    char *request = malloc(INFERENCE_REQUEST_MAX + 1);
    if (!request)
        return 2;
    size_t n = fread(request, 1, INFERENCE_REQUEST_MAX + 1, stdin);
    int rc = ferror(stdin) ? 2 : inference_dispatch_config(&cfg, request, n, stdout);
    free(request);
    return rc;
}
