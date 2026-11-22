import fs from 'node:fs';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
const file='../../src/samosa_gateway.c',original=execFileSync('git',['-C','../..','show','abfd79f:src/samosa_gateway.c'],{encoding:'utf8',maxBuffer:8*1024*1024});
function block(source,name){const a=source.indexOf('static ',source.indexOf('\nstatic ',source.indexOf(name)));
 const n=source.indexOf(name+'(');assert(n>=0);const begin=source.lastIndexOf('\n',n)+1,end=source.indexOf('\n}\n',n)+3;assert(end>begin);return source.slice(begin,end);}
const release=block(original,'vision_turn_release_runtime'),start=block(original,'vision_turn_start'),lower=block(original,'vision_lower_side'),inspect=block(original,'vision_turn_inspect');
const gateRelease=`        pthread_mutex_lock(&g->generation_gate_mu);
        g->specialist_generation_active = 0;
        g->specialist_provider = 0;
        pthread_mutex_unlock(&g->generation_gate_mu);`;
const gateStart=`    pthread_mutex_lock(&g->generation_gate_mu);
    if (g->specialist_generation_active || atomic_load(&g->generating)) {
        pthread_mutex_unlock(&g->generation_gate_mu);
        path_copy(out_code, code_cap, "vision_specialist_busy");
        return 0;
    }
    g->specialist_generation_active = 1;
    g->specialist_provider = turn->provider;
    turn->generation_gate_held = 1;
    pthread_mutex_unlock(&g->generation_gate_mu);`;
assert(release.includes(gateRelease)&&start.includes(gateStart));
export function adapt(body){return body
.replace('static void vision_turn_release_runtime(Gateway *g, VisionTurnContext *turn)', 'void samosa_visual_turn_release(const SamosaVisualTurnControl *control, SamosaVisualTurn *turn)')
.replace('static int vision_turn_start(Gateway *g, VisionTurnContext *turn,','int samosa_visual_turn_start(const SamosaVisualTurnControl *control, SamosaVisualTurn *turn,')
.replace('static int vision_turn_inspect(Gateway *g, VisionTurnContext *turn,','int samosa_visual_turn_inspect(const SamosaVisualTurnControl *control, SamosaVisualTurn *turn,')
.replace(gateRelease,'        visual_end(control->gate);')
.replace(gateStart,`    if (!visual_begin(control->gate, turn->provider)) {
        path_copy(out_code, code_cap, "vision_specialist_busy");
        return 0;
    }
    turn->generation_gate_held = 1;`)
.replaceAll('turn->plan.extended_visual','turn->extended_visual').replaceAll('turn->plan.detail_needed','turn->detail_needed')
.replaceAll('visionpsy_session_close(g,','control->session_close(control->context,')
.replaceAll('atomic_load(&g->stopping)','control->stopping(control->context)')
.replaceAll('backend_start(g)','control->primary_start(control->context)').replaceAll('backend_stop(g)','control->primary_stop(control->context)').replaceAll('backend_probe(g)','control->primary_probe(control->context)')
.replaceAll('monotonic_millis()','control->now_ms(control->context)').replaceAll('sleep_millis(100)','control->sleep_ms(control->context, 100)')
.replaceAll('molmo2_available(g)','control->available(control->context, 2)').replaceAll('visionpsy_available(g)','control->available(control->context, 1)')
.replaceAll('molmo2_required_available_gb(g)','control->molmo_required_gb(control->context)').replaceAll('vision_total_memory_bytes()','control->total_memory_bytes(control->context)')
.replace('        SamosaBackendOwner primary_owner = backend_owner(g);\n        int running = samosa_backend_process_pid(&primary_owner) > 0;','        int running = control->primary_running(control->context);')
.replaceAll('vision_resource_budget(g,','control->resource_budget(control->context,')
.replace('vision_wait_for_molmo_memory(\n            g,','control->wait_for_molmo_memory(\n            control->context,')
.replaceAll('vision_turn_release_runtime(g,','samosa_visual_turn_release(control,').replaceAll('vision_turn_release_runtime(g,','samosa_visual_turn_release(control,')
.replaceAll('molmo2_session_start(g,','control->session_start(control->context, 2,').replaceAll('visionpsy_session_start(g,','control->session_start(control->context, 1,')
.replaceAll('vision_turn_start(g,','samosa_visual_turn_start(control,')
.replaceAll('visionpsy_session_inspect(g,','control->inspect(control->context,')
.replace(`molmo2_session_analyze(g, &turn->session, runtime_image, "image", prompt,
                                      max_tokens, 0.0, 0.0, 1, out_obs, out_cap,
                                      out_prompt_tokens, out_gen_tokens,
                                      NULL, NULL,`,`control->analyze_image(control->context, &turn->session, runtime_image, prompt,
                                      max_tokens, out_obs, out_cap,
                                      out_prompt_tokens, out_gen_tokens,`);}
const prologue=`#define _POSIX_C_SOURCE 200809L
#include <samosa_inference/turn_control.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

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

`;
const owner=prologue+[release,start,lower,inspect].map(adapt).join('\n\n')+'\n';assert(!owner.includes('Gateway')&&!owner.includes('g->'));
assert.equal(fs.readFileSync('src/turn_control.c','utf8'),owner,'Original coordination changed beyond owner/state/callback adaptation');
// Root adapters borrow the same original host policies and tracing wrappers.
const adapters=`static int turn_available(void *context, int provider) {
    return provider == 2 ? molmo2_available(context) : visionpsy_available(context);
}
static double turn_molmo_required(void *context) { return molmo2_required_available_gb(context); }
static long long turn_total_memory(void *context) { (void)context; return vision_total_memory_bytes(); }
static int turn_primary_running(void *context) {
    SamosaBackendOwner owner = backend_owner(context);
    return samosa_backend_process_pid(&owner) > 0;
}
static void turn_primary_stop(void *context) { backend_stop(context); }
static int turn_primary_start(void *context) { return backend_start(context); }
static int turn_primary_probe(void *context) { return backend_probe(context); }
static int turn_stopping(void *context) { return atomic_load(&((Gateway *)context)->stopping); }
static long long turn_now(void *context) { (void)context; return monotonic_millis(); }
static void turn_sleep(void *context, long ms) { (void)context; sleep_millis(ms); }
static VisionResourceBudget turn_budget(void *context, int detail) { return vision_resource_budget(context, detail); }
static VisionResourceBudget turn_wait(void *context, int detail, double required, VisionResourceBudget initial) {
    return vision_wait_for_molmo_memory(context, detail, required, initial);
}
static int turn_session_start(void *context, int provider, VisionPsySession *session) {
    return provider == 2 ? molmo2_session_start(context, session) : visionpsy_session_start(context, session);
}
static void turn_session_close(void *context, VisionPsySession *session) { visionpsy_session_close(context, session); }
static int turn_inspect(void *context, VisionPsySession *session, const char *image, const char *prompt,
    int side, int tokens, char *observation, size_t capacity, int *prompt_tokens, int *generated_tokens, char *error, size_t error_capacity) {
    return visionpsy_session_inspect(context, session, image, prompt, side, tokens, observation, capacity, prompt_tokens, generated_tokens, error, error_capacity);
}
static int turn_analyze_image(void *context, VisionPsySession *session, const char *image, const char *prompt,
    int tokens, char *observation, size_t capacity, int *prompt_tokens, int *generated_tokens, char *error, size_t error_capacity) {
    return molmo2_session_analyze(context, session, image, "image", prompt, tokens, 0.0, 0.0, 1, observation, capacity, prompt_tokens, generated_tokens, NULL, NULL, error, error_capacity);
}
static SamosaVisualTurnControl visual_turn_control(Gateway *g) {
    return (SamosaVisualTurnControl){&g->turn_gate, turn_available, turn_molmo_required, turn_total_memory,
        turn_primary_running, turn_primary_stop, turn_primary_start, turn_primary_probe, turn_stopping,
        turn_now, turn_sleep, turn_budget, turn_wait, turn_session_start, turn_session_close,
        turn_inspect, turn_analyze_image, g};
}
static void vision_turn_release_runtime(Gateway *g, VisionTurnContext *turn) {
    if (!turn) return;
    SamosaVisualTurnControl control = visual_turn_control(g);
    samosa_visual_turn_release(&control, &turn->runtime);
}`;
const startWrapper=`static int vision_turn_start(Gateway *g, VisionTurnContext *turn,
                             char *out_code, size_t code_cap) {
    SamosaVisualTurnControl control = visual_turn_control(g);
    turn->runtime.detail_needed = turn->plan.detail_needed;
    turn->runtime.extended_visual = turn->plan.extended_visual;
    return samosa_visual_turn_start(&control, &turn->runtime, out_code, code_cap);
}`;
const inspectWrapper=`static int vision_turn_inspect(Gateway *g, VisionTurnContext *turn,
                               const char *image_path, int prepared_owned, const char *prompt,
                               int max_tokens, char *out_obs, size_t out_cap,
                               int *out_prompt_tokens, int *out_gen_tokens,
                               char *out_code, size_t code_cap) {
    SamosaVisualTurnControl control = visual_turn_control(g);
    turn->runtime.detail_needed = turn->plan.detail_needed;
    turn->runtime.extended_visual = turn->plan.extended_visual;
    return samosa_visual_turn_inspect(&control, &turn->runtime, image_path, prepared_owned,
        prompt, max_tokens, out_obs, out_cap, out_prompt_tokens, out_gen_tokens, out_code, code_cap);
}`;
let root=original.replace(release,adapters).replace(start,startWrapper).replace(lower+'\n','').replace(inspect,inspectWrapper);
root=root.replace('#include <samosa_inference/vision_budget.h>','#include <samosa_inference/vision_budget.h>\n#include <samosa_inference/turn_control.h>')
.replace('    atomic_int generating;\n','').replace('    pthread_mutex_t generation_gate_mu;\n    int specialist_generation_active;\n    int specialist_provider; /* 1 = VisionPsy, 2 = Molmo2 */','    SamosaTurnGate turn_gate;')
.replace('    VisionResourceBudget budget;\n    VisionPsySession session;\n    int provider; /* 1 = VisionPsy, 2 = Molmo2 */','    SamosaVisualTurn runtime;').replace('    int generation_gate_held;\n    int backend_paused;\n','');
for(const field of ['budget','session','provider','generation_gate_held','backend_paused']){
 root=root.replaceAll('vision_turn->'+field,'vision_turn->runtime.'+field).replaceAll('vision_turn.'+field,'vision_turn.runtime.'+field).replaceAll('turn->'+field,'turn->runtime.'+field).replaceAll('turn.'+field,'turn.runtime.'+field);
}
root=root.replaceAll('    pthread_mutex_lock(&g->generation_gate_mu);\n    if (g->specialist_generation_active) {\n        pthread_mutex_unlock(&g->generation_gate_mu);','    if (!samosa_turn_primary_begin(&g->turn_gate)) {').replaceAll('    atomic_fetch_add(&g->generating, 1);\n    pthread_mutex_unlock(&g->generation_gate_mu);\n','');
root=root.replace(`        pthread_mutex_lock(&g->generation_gate_mu);
        int specialist_active = g->specialist_generation_active;
        int specialist_provider = g->specialist_provider;
        pthread_mutex_unlock(&g->generation_gate_mu);`,`        SamosaTurnStatus turn_status = samosa_turn_status(&g->turn_gate);
        int specialist_active = turn_status.specialist_active;
        int specialist_provider = turn_status.specialist_provider;`);
root=root.replaceAll('atomic_load(&g->generating)','samosa_turn_generating(&g->turn_gate)').replaceAll('atomic_fetch_sub(&g->generating, 1)','samosa_turn_primary_end(&g->turn_gate)').replaceAll('atomic_store(&g->generating, 1)','samosa_turn_direct_generating(&g->turn_gate, 1)').replaceAll('atomic_store(&g->generating, 0)','samosa_turn_direct_generating(&g->turn_gate, 0)')
.replace('    pthread_mutex_init(&g->generation_gate_mu, NULL);','    if (!samosa_turn_gate_init(&g->turn_gate)) return 0;').replace('    atomic_init(&g->generating, 0);\n','').replaceAll('pthread_mutex_destroy(&gateway.generation_gate_mu)','samosa_turn_gate_destroy(&gateway.turn_gate)');
assert(!root.includes('g->generating')&&!root.includes('generation_gate_mu'));
assert.equal(fs.readFileSync(file,'utf8'),root,'Gateway changed beyond extraction adapters and gate access');
assert.equal(execFileSync('git',['diff','be6df7c','--','engines','src/specialist_protocol.c','src/specialist_supervisor.c','src/engine_process.c','src/backend_process.c','src/vision_budget.c','src/whisper_process.c'],{encoding:'utf8'}),'','Existing models/protocols/processes/budgets changed');
console.log('PASS original availability/resource/pause/resume/60-second wait/lower-side/owned-page retry bodies moved through explicit callbacks; all other Gateway code unchanged except dedicated runtime/gate access');
assert(original.includes('vision_turn_close(g, &turn);\n    atomic_store(&g->generating, 0);'),'Original direct-close reset window missing');
console.log('PASS sole gate behavior fix: direct health withdrawal preserves a newly admitted primary count; the original reset window is present in the baseline');
console.log('PASS existing model engines, protocols, process guardians, backend/Whisper ownership and resource-budget implementations unchanged');
const oldBudget=execFileSync('git',['-C','../..','show','abfd79f:tests/test_prompt_budget.c'],{encoding:'utf8'});
const newBudget=fs.readFileSync('../../tests/test_prompt_budget.c','utf8');
const domain=source=>source.slice(source.indexOf('    TextBuffer evidence = {0}, payload = {0}, question = {0};'),source.indexOf('    free(evidence.data); free(payload.data);'));
assert(domain(oldBudget).length>2000);assert.equal(domain(newBudget),domain(oldBudget),'Original C budgeting cases/assertions changed');
assert.equal(block(newBudget,'cancel_review'),block(oldBudget,'cancel_review').replace('atomic_load(&g->generating)','samosa_turn_generating(&g->turn_gate)'),'Original cancellation assertion changed');
const oldDriver=execFileSync('git',['-C','../..','show','abfd79f:tests/test_prompt_budget.py'],{encoding:'utf8'});
const originalModes=[...oldDriver.match(/for mode in \(([^\n]+)\):/)[1].matchAll(/'([^']+)'/g)].map(m=>m[1]);
const portedModes=[...fs.readFileSync('../../tests/test_prompt_budget.mjs','utf8').match(/const modes=\[([^\n]+)\];/)[1].matchAll(/'([^']+)'/g)].map(m=>m[1]);
assert.deepEqual(portedModes,originalModes);assert.equal(originalModes.length,15);
assert(!fs.existsSync('../../tests/test_prompt_budget.py'));assert(!fs.readFileSync('../../Makefile','utf8').includes('tests/test_prompt_budget.py'));
console.log('PASS original C prompt-budget inputs/domain assertions and cancellation retained; all15 original driver scenarios ported to Node and old Python driver/caller retired');
