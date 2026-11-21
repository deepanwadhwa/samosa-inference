import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
const owner = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const parent = path.resolve(process.argv[2] ?? path.join(owner, '../..'));
const original = execFileSync('git', ['-C', parent, 'show', 'ce06f94:src/samosa_gateway.c'], { encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
const current = fs.readFileSync(path.join(owner, 'src/vision_budget.c'), 'utf8');
const host = fs.readFileSync(path.join(parent, 'src/samosa_gateway.c'), 'utf8');
function body(source, name) {
  const m = new RegExp(`^(?:static )?(?:long long|double|int|void|VisionResourceBudget|SamosaVisionResourceBudget)\\s+${name}\\s*\\([^;]*?\\)\\s*\\{`, 'm').exec(source); assert(m, name);
  const begin = m.index + m[0].length; let depth = 1, state = '';
  for (let i = begin; i < source.length; i++) {
    const c = source[i], n = source[i + 1];
    if (state === 'line') { if (c === '\n') state = ''; continue; }
    if (state === 'comment') { if (c === '*' && n === '/') { state = ''; i++; } continue; }
    if (state) { if (c === '\\') i++; else if (c === state) state = ''; continue; }
    if (c === '/' && n === '/') { state = 'line'; i++; continue; }
    if (c === '/' && n === '*') { state = 'comment'; i++; continue; }
    if (c === '"' || c === "'") { state = c; continue; }
    if (c === '{') depth++; if (c === '}' && --depth === 0) return source.slice(begin, i);
  }
  throw new Error(`Unclosed ${name}`);
}
function normalize(source) {
  return source.match(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[^\s]/g)
    .filter(token => !token.startsWith('/*') && !token.startsWith('//')).join('');
}
console.log('Original Chat baseline: ce06f94');
for (const name of ['vision_available_memory_gb', 'vision_memory_pressure', 'vision_total_memory_bytes', 'vision_thermal_pressure']) {
  assert.equal(normalize(body(current, name === 'vision_total_memory_bytes' ? `samosa_${name}` : name)), normalize(body(original, name)));
  console.log(`${name}: original native probe body preserved`);
}
let budget = body(original, 'vision_resource_budget').replace('VisionResourceBudget b', 'SamosaVisionResourceBudget b');
const sample = '    b.available_gb = vision_available_memory_gb();\n    b.memory_pressure = vision_memory_pressure();\n    b.thermal_pressure = vision_thermal_pressure();\n    long long total_bytes = vision_total_memory_bytes();';
assert(budget.includes(sample));
budget = budget.replace(sample, '    SamosaVisionResourceSample sample = control && control->sample ? control->sample(control->context) : samosa_vision_host_sample();\n    b.available_gb = sample.available_gb;\n    b.memory_pressure = sample.memory_pressure;\n    b.thermal_pressure = sample.thermal_pressure;\n    long long total_bytes = sample.total_bytes;')
  .replaceAll('developer_trace_event(g,', 'budget_trace(control,');
assert.equal(normalize(body(current, 'samosa_vision_resource_budget')), normalize(budget));
console.log('Budget thresholds/detail/unknown/reason/trace: original body preserved after explicit sample/trace substitutions');
let wait = body(original, 'vision_wait_for_molmo_memory').replaceAll('VisionResourceBudget current', 'SamosaVisionResourceBudget current')
  .replaceAll('monotonic_millis()', 'budget_now(control)').replaceAll('sleep_millis(100)', 'budget_sleep(control, 100)')
  .replaceAll('atomic_load(&g->stopping)', 'budget_stopping(control)').replaceAll('vision_resource_budget(g,', 'samosa_vision_resource_budget(control,')
  .replaceAll('developer_trace_event(g,', 'budget_trace(control,');
assert.equal(normalize(body(current, 'samosa_vision_wait_for_molmo_memory')), normalize(wait));
console.log('Reclaim wait: original100-ms/3-second/peak/stop/trace body preserved after explicit hooks');
assert.equal(normalize(body(current, 'budget_now').replace('if (control && control->now_ms) return control->now_ms(control->context);', '')), normalize(body(original, 'monotonic_millis')));
assert.equal(normalize(body(current, 'budget_sleep').replace('if (control && control->sleep_ms) { control->sleep_ms(control->context, ms); return; }', '')), normalize(body(original, 'sleep_millis')));
console.log('Default clock/sleep: original bodies preserved');
const required = body(original, 'molmo2_required_available_gb');
const first = required.indexOf('    double total_gb ='), last = required.indexOf('    json_free(root);'); assert(first >= 0 && last > first);
const arithmetic = required.slice(first, last).replace('resident && resident->t == J_NUM && resident->num > 0', 'resident_bytes > 0').replaceAll('resident->num', 'resident_bytes');
assert.equal(normalize(body(current, 'samosa_molmo2_required_available_gb')), normalize(arithmetic + 'return required;'));
const wrapper = required.slice(0, first).replace('return 7.75;', 'return SAMOSA_MOLMO2_MANIFEST_UNAVAILABLE_GB;') +
  '    double required = samosa_molmo2_required_available_gb(\n        resident && resident->t == J_NUM ? resident->num : 0, total_bytes);\n' + required.slice(last);
assert.equal(normalize(body(host, 'molmo2_required_available_gb')), normalize(wrapper));
assert(fs.readFileSync(path.join(owner, 'include/samosa_inference/vision_budget.h'), 'utf8').includes('#define SAMOSA_MOLMO2_MANIFEST_UNAVAILABLE_GB 7.75'));
console.log('Resident-estimate/reserve arithmetic and caller manifest/fallback: original behavior preserved');
