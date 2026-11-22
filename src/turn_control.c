#define _POSIX_C_SOURCE 200809L
#include <samosa_inference/turn_control.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    pthread_mutex_t mutex;
    atomic_int generating;
    int specialist_active, specialist_provider, direct_generating;
} TurnGate;
int samosa_turn_gate_init(SamosaTurnGate *gate) {
    if (!gate || gate->state) return 0;
    TurnGate *state = calloc(1, sizeof(*state));
    if (!state) return 0;
    if (pthread_mutex_init(&state->mutex, NULL)) { free(state); return 0; }
    atomic_init(&state->generating, 0);
    gate->state = state;
    return 1;
}
void samosa_turn_gate_destroy(SamosaTurnGate *gate) {
    if (!gate || !gate->state) return;
    TurnGate *state = gate->state;
    pthread_mutex_destroy(&state->mutex);
    free(state); gate->state = NULL;
}
int samosa_turn_primary_begin(SamosaTurnGate *gate) {
    TurnGate *state = gate->state;
    pthread_mutex_lock(&state->mutex);
    if (state->specialist_active) { pthread_mutex_unlock(&state->mutex); return 0; }
    atomic_fetch_add(&state->generating, 1);
    pthread_mutex_unlock(&state->mutex);
    return 1;
}
void samosa_turn_primary_end(SamosaTurnGate *gate) {
    TurnGate *state = gate->state;
    atomic_fetch_sub(&state->generating, 1);
}
int samosa_turn_generating(const SamosaTurnGate *gate) {
    TurnGate *state = gate->state;
    return atomic_load(&state->generating);
}
void samosa_turn_direct_generating(SamosaTurnGate *gate, int value) {
    TurnGate *state = gate->state;
    pthread_mutex_lock(&state->mutex);
    int active = value != 0;
    /* Direct visual close releases exclusion before clearing this health
       flag. Withdraw only its own count: a new primary may already be active. */
    if (active != state->direct_generating) {
        atomic_fetch_add(&state->generating, active ? 1 : -1);
        state->direct_generating = active;
    }
    pthread_mutex_unlock(&state->mutex);
}
SamosaTurnStatus samosa_turn_status(SamosaTurnGate *gate) {
    TurnGate *state = gate->state;
    pthread_mutex_lock(&state->mutex);
    SamosaTurnStatus result = {atomic_load(&state->generating), state->specialist_active, state->specialist_provider};
    pthread_mutex_unlock(&state->mutex);
    return result;
}
static int visual_begin(SamosaTurnGate *gate, int provider) {
    TurnGate *state = gate->state;
    pthread_mutex_lock(&state->mutex);
    if (state->specialist_active || atomic_load(&state->generating)) {
        pthread_mutex_unlock(&state->mutex); return 0;
    }
    state->specialist_active = 1;
    state->specialist_provider = provider;
    pthread_mutex_unlock(&state->mutex);
    return 1;
}
static void visual_end(SamosaTurnGate *gate) {
    TurnGate *state = gate->state;
    pthread_mutex_lock(&state->mutex);
    state->specialist_active = 0;
    state->specialist_provider = 0;
    pthread_mutex_unlock(&state->mutex);
}
static int path_copy(char *out, size_t cap, const char *value) {
    int n = snprintf(out, cap, "%s", value ? value : "");
    return n >= 0 && (size_t)n < cap;
}

void samosa_visual_turn_release(const SamosaVisualTurnControl *control, SamosaVisualTurn *turn) {
    if (!turn) return;
    control->session_close(control->context, &turn->session);
    if (turn->backend_paused && !control->stopping(control->context)) {
        if (control->primary_start(control->context)) {
            long long deadline = control->now_ms(control->context) + 60000;
            while (!control->stopping(control->context) && control->now_ms(control->context) < deadline) {
                if (control->primary_probe(control->context)) break;
                control->sleep_ms(control->context, 100);
            }
        }
        turn->backend_paused = 0;
    }
    if (turn->generation_gate_held) {
        visual_end(control->gate);
        turn->generation_gate_held = 0;
    }
}


int samosa_visual_turn_start(const SamosaVisualTurnControl *control, SamosaVisualTurn *turn,
                             char *out_code, size_t code_cap) {
    if (turn->session.active) return 1;
    int requested_provider = turn->provider;
    if (!turn->provider && turn->extended_visual) {
        requested_provider = 2;
        turn->provider = control->available(control->context, 2) ? 2 : 0;
    } else if (!turn->provider) {
        /* Molmo is the primary on-demand visual intelligence runtime once its
           verified Q4 package is installed. VisionPsy remains a smaller
           fallback for hosts without Molmo; it must not silently win merely
           because both packages happen to be present. */
        turn->provider = control->available(control->context, 2) ? 2 : control->available(control->context, 1) ? 1 : 0;
    }
    else if ((turn->provider == 2 && !control->available(control->context, 2)) ||
             (turn->provider == 1 && !control->available(control->context, 1)))
        turn->provider = 0;
    if (!turn->provider) {
        path_copy(out_code, code_cap, requested_provider == 2
                                   ? "molmo2_model_required"
                                   : "vision_model_required");
        return 0;
    }
    if (!visual_begin(control->gate, turn->provider)) {
        path_copy(out_code, code_cap, "vision_specialist_busy");
        return 0;
    }
    turn->generation_gate_held = 1;

    double molmo_required_gb = turn->provider == 2
        ? control->molmo_required_gb(control->context) : 0.0;

    /* On the reference 16-GiB machine, reclaim the primary model before the
       4B specialist. This is a reversible stop/start, preserves the selected
       model, and prevents two Metal decoders from being resident together. */
    if (turn->provider == 2 && control->total_memory_bytes(control->context) > 0 &&
        control->total_memory_bytes(control->context) <= 18LL * 1024 * 1024 * 1024) {
        int running = control->primary_running(control->context);
        if (running) {
            if (!control->primary_stop(control->context)) {
                path_copy(out_code, code_cap, "primary_cleanup_pending");
                samosa_visual_turn_release(control, turn);
                return 0;
            }
            turn->backend_paused = 1;
        }
        turn->budget = control->resource_budget(control->context, turn->detail_needed);
        /* waitpid() proves the primary process is gone, but macOS releases its
           Metal/wired pages asynchronously. On the qualified 16-GiB machine
           the safe manifest estimate plus adaptive 3-GiB reserve appears
           shortly after exit. Poll only while pressure remains non-critical;
           larger-memory hosts retain the 4-GiB reserve. */
        turn->budget = control->wait_for_molmo_memory(
            control->context, turn->detail_needed, molmo_required_gb, turn->budget);
    }
    if (!turn->budget.admitted ||
        (turn->provider == 2 && turn->budget.available_gb > 0.0 &&
         turn->budget.available_gb < molmo_required_gb)) {
        path_copy(out_code, code_cap, turn->provider == 2
                                   ? "molmo2_resource_pressure"
                                   : "vision_resource_pressure");
        samosa_visual_turn_release(control, turn);
        return 0;
    }
    if (!(turn->provider == 2 ? control->session_start(control->context, 2, &turn->session)
                              : control->session_start(control->context, 1, &turn->session))) {
        path_copy(out_code, code_cap, "vision_session_failed");
        samosa_visual_turn_release(control, turn);
        return 0;
    }
    return 1;
}


static int vision_lower_side(int side) {
    if (side > 1536) return 1536;
    if (side > 1024) return 1024;
    if (side > 512) return 512;
    return 0;
}


int samosa_visual_turn_inspect(const SamosaVisualTurnControl *control, SamosaVisualTurn *turn,
                               const char *image_path, int prepared_owned, const char *prompt,
                               int max_tokens, char *out_obs, size_t out_cap,
                               int *out_prompt_tokens, int *out_gen_tokens,
                               char *out_code, size_t code_cap) {
    if (!samosa_visual_turn_start(control, turn, out_code, code_cap)) return 0;
    char owned_image[PATH_MAX];const char *runtime_image=image_path;
    if(prepared_owned){
        if(!samosa_mm_session_take_temporary_image(&turn->session,image_path,owned_image,sizeof(owned_image))){path_copy(out_code,code_cap,"vision_temp_image_handoff_failed");return 0;}
        runtime_image=owned_image;
    }
    if (turn->provider == 2)
        return control->analyze_image(control->context, &turn->session, runtime_image, prompt,
                                      max_tokens, out_obs, out_cap,
                                      out_prompt_tokens, out_gen_tokens,
                                      out_code, code_cap);
    int side = turn->budget.max_side_len;
    for (int attempt = 0; ; ++attempt) {
        if (out_obs && out_cap) out_obs[0] = 0;
        if (out_prompt_tokens) *out_prompt_tokens = 0;
        if (out_gen_tokens) *out_gen_tokens = 0;
        if (control->inspect(control->context, &turn->session, runtime_image, prompt, side,
                             max_tokens, out_obs, out_cap, out_prompt_tokens, out_gen_tokens,
                             out_code, code_cap)) {
            if (!out_obs || !out_cap || out_obs[strspn(out_obs, " \t\r\n")]) return 1;
            path_copy(out_code, code_cap, "vision_empty_observation");
        }
        if (attempt) return 0;

        /* Recreate a failed helper once at the next safe tile budget. Close
           its session and release exclusion before reacquiring the lease. */
        side = vision_lower_side(side);
        if (prepared_owned && !samosa_mm_session_return_temporary_image(&turn->session,image_path)) {
            samosa_visual_turn_release(control,turn);
            path_copy(out_code,code_cap,"vision_temp_image_return_failed");
            return 0;
        }
        samosa_visual_turn_release(control, turn);
        if (!side) return 0;
        turn->budget.max_side_len = side;
        if (!samosa_visual_turn_start(control, turn, out_code, code_cap)) return 0;
        if (prepared_owned) {
            if (!samosa_mm_session_take_temporary_image(&turn->session,image_path,owned_image,sizeof(owned_image))) {
                path_copy(out_code,code_cap,"vision_temp_image_handoff_failed");
                return 0;
            }
            runtime_image = owned_image;
        }
    }
}

