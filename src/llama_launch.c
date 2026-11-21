#define _POSIX_C_SOURCE 200809L
#include <samosa_inference/llama_launch.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Existing Chat llama-server launch flags, extracted without a new supervisor. */
int samosa_llama_launch(const SamosaLlamaLaunch *config) {
    char port[16];
    snprintf(port, sizeof(port), "%d", config->port);
    char *argv[24]; int a = 0;
    /* Machine safety (CLAUDE.md): llama.cpp defaults to every
       performance core and a full-size KV cache, which on a fanless
       M3 Air means sustained heat and a large resident footprint.
       Both are now tunable without editing code:
         SAMOSA_LLAMA_THREADS  cap CPU threads (unset = llama.cpp default)
         SAMOSA_LLAMA_CTX      KV cache size in tokens (default 8192)
         SAMOSA_LLAMA_NGL      layers offloaded to Metal (default 99)
       Values are passed through only when they parse as positive
       integers, so a typo cannot turn into a malformed argv. */
    const char *env_ctx = config->context_override;
    const char *env_ngl = config->gpu_override;
    const char *env_thr = config->threads_override;
    char ctx_buf[16], ngl_buf[16], thr_buf[16];
    snprintf(ctx_buf, sizeof(ctx_buf), "%d", config->context_tokens);
    const char *ctx_val = ctx_buf, *ngl_val = "99", *thr_val = NULL;
    if (config->threads > 0) {
        snprintf(thr_buf, sizeof(thr_buf), "%d", config->threads);
        thr_val = thr_buf;
    }
    /* An explicit value always wins: the machine is a starting point,
       not a policy the user cannot escape. */
    if (env_ctx && atoi(env_ctx) > 0) { snprintf(ctx_buf, sizeof(ctx_buf), "%d", atoi(env_ctx)); ctx_val = ctx_buf; }
    if (env_ngl && atoi(env_ngl) >= 0) { snprintf(ngl_buf, sizeof(ngl_buf), "%d", atoi(env_ngl)); ngl_val = ngl_buf; }
    if (env_thr && atoi(env_thr) > 0) { snprintf(thr_buf, sizeof(thr_buf), "%d", atoi(env_thr)); thr_val = thr_buf; }
    fprintf(stderr, "[samosa] backend sizing: %.0f GB RAM, %d performance cores -> -c %s%s%s\n",
            config->ram_bytes / 1073741824.0, config->perf_cores,
            ctx_val, thr_val ? " -t " : "", thr_val ? thr_val : "");

    argv[a++] = (char *)config->engine; argv[a++] = (char *)"-m"; argv[a++] = (char *)config->model;
    argv[a++] = (char *)"-ngl"; argv[a++] = (char *)ngl_val;
    argv[a++] = (char *)"-c"; argv[a++] = (char *)ctx_val;
    if (thr_val) { argv[a++] = (char *)"-t"; argv[a++] = (char *)thr_val; }
    argv[a++] = (char *)"-np"; argv[a++] = (char *)"1";
    argv[a++] = (char *)"--cache-ram"; argv[a++] = (char *)"0";
    argv[a++] = (char *)"--host"; argv[a++] = (char *)"127.0.0.1";
    argv[a++] = (char *)"--port"; argv[a++] = port;
    argv[a++] = (char *)"--no-ui"; argv[a++] = (char *)"--alias"; argv[a++] = (char *)config->alias;
    /* Bonsai is image-capable via its optional mmproj vision pack; load it
       only when present so image jobs reach a fast local vision backend.
       Text-only serving (and Ornith) skips it. */
    if (config->projector && *config->projector) {
        argv[a++] = (char *)"--mmproj"; argv[a++] = (char *)config->projector;
    }
    argv[a] = NULL;
    return execv(config->engine, argv);
}
