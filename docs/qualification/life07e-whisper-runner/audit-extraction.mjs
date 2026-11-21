// Verify the extraction against the recoverable original Chat source.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
const owner = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const parent = path.resolve(process.argv[2] ?? path.join(owner, '../..'));
const original = execFileSync('git', ['-C', parent, 'show', 'f2414d7:src/samosa_gateway.c'], { encoding: 'utf8', maxBuffer: 16 * 1024 * 1024 });
const current = fs.readFileSync(path.join(owner, 'src/whisper_process.c'), 'utf8');
const host = fs.readFileSync(path.join(parent, 'src/samosa_gateway.c'), 'utf8');
function body(source, name) {
  const match = new RegExp(`^(?:static )?[^\n;]*?\\b${name}\\s*\\([^;]*?\\)\\s*\\{`, 'm').exec(source);
  assert(match, `Missing function ${name}`);
  const begin = match.index + match[0].length;
  let depth = 1, state = '';
  for (let i = begin; i < source.length; i++) {
    const c = source[i], next = source[i + 1];
    if (state === 'line') { if (c === '\n') state = ''; continue; }
    if (state === 'comment') { if (c === '*' && next === '/') { state = ''; i++; } continue; }
    if (state) { if (c === '\\') i++; else if (c === state) state = ''; continue; }
    if (c === '/' && next === '/') { state = 'line'; i++; continue; }
    if (c === '/' && next === '*') { state = 'comment'; i++; continue; }
    if (c === '"' || c === "'") { state = c; continue; }
    if (c === '{') depth++;
    if (c === '}' && --depth === 0) return source.slice(begin, i);
  }
  throw new Error(`Unclosed function ${name}`);
}
function normalize(source) {
  return source.match(/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[^\s]/g)
    .filter(token => !token.startsWith('/*') && !token.startsWith('//')).join('');
}
console.log('Source baseline: Chat f2414d7');
for (const name of ['path_copy', 'mkdirs', 'read_file_bytes_limit', 'read_file_limit', 'utf8_scalar_count', 'trim_ascii_ws']) {
  assert.equal(normalize(body(current, name)), normalize(body(original, name)), `Changed helper ${name}`);
  console.log(`${name}: original body preserved`);
}
assert.equal(normalize(body(host, 'voice_stt_fingerprint')), normalize(body(original, 'voice_stt_fingerprint')));
console.log('Selected model/fingerprint: original body preserved');
const old = body(original, 'whisper_transcribe_path');
let core = old.slice(old.indexOf('    int result = WHISPER_TRANSCRIBE_PREPARE_FAILED;'));
core = core.replace('char voice_dir[PATH_MAX], out_base[PATH_MAX] = "", out_path[PATH_MAX] = "";', 'char out_base[PATH_MAX] = "", out_path[PATH_MAX] = "";')
  .replace('!path_join(voice_dir, sizeof(voice_dir), g->home, "voice/tmp") ||\n        !mkdirs(voice_dir) ||', '!config->temporary_directory ||\n        !mkdirs(config->temporary_directory) ||')
  .replace('voice_dir) >=', 'config->temporary_directory) >=')
  .replaceAll('g->whisper_cli', 'config->engine').replaceAll('model_path', 'config->model')
  .replaceAll('atomic_load(&g->audio_cancel_requested)', '(control && control->cancelled(control->context))');
const publish = core.indexOf('    pthread_mutex_lock(&g->voice_mu);'), wait = core.indexOf('    int status = 0;', publish);
assert(publish >= 0 && wait > publish);
core = core.slice(0, publish) + '    if (control) control->started(control->context, pid);\n' + core.slice(wait);
core = core.replace('    track_job_pid(g, pid, 0);', '    if (control) control->reaped(control->context, pid);');
const release = core.lastIndexOf('    pthread_mutex_lock(&g->voice_mu);'), result = core.lastIndexOf('    return result;');
assert(release >= 0 && result > release);
core = core.slice(0, release) + '    if (control) control->release(control->context);\n' + core.slice(result);
core = core.replaceAll('WHISPER_TRANSCRIBE_', 'SAMOSA_WHISPER_');
const actual = body(current, 'samosa_whisper_process_run');
assert.equal(normalize(actual.slice(actual.indexOf('    int result = SAMOSA_WHISPER_PREPARE_FAILED;'))), normalize(core));
console.log('Runner core: original argv/fork/wait/read/trim/validate/unlink preserved after explicit configuration/callback substitutions');
const acquire = old.slice(old.indexOf('    pthread_mutex_lock(&g->voice_mu);'), old.indexOf('    int result = WHISPER_TRANSCRIBE_PREPARE_FAILED;'));
assert.equal(normalize(body(host, 'whisper_process_acquire').replace('Gateway *g = context;', '')),
  normalize(acquire.replace('return WHISPER_TRANSCRIBE_BUSY;', 'return 0;') + 'return 1;'));
const start = old.indexOf('    pthread_mutex_lock(&g->voice_mu);', old.indexOf('    pid_t pid = fork();'));
assert.equal(normalize(body(host, 'whisper_process_started').replace('Gateway *g = context;', '')),
  normalize(old.slice(start, old.indexOf('    int status = 0;', start))));
assert.equal(normalize(body(host, 'whisper_process_release').replace('Gateway *g = context;', '')),
  normalize(old.slice(old.lastIndexOf('    pthread_mutex_lock(&g->voice_mu);'), old.lastIndexOf('    return result;'))));
console.log('Host acquire/start/release callbacks: original busy gate, preserve-cancel, locked PID publication, pending cancellation, tracking and reset preserved');
const header = fs.readFileSync(path.join(owner, 'include/samosa_inference/whisper_process.h'), 'utf8');
for (const [name, value] of [['OK', 1], ['NOT_READY', -1], ['BUSY', -2], ['PREPARE_FAILED', -3], ['PROCESS_FAILED', -4], ['OUTPUT_INVALID', -5], ['CANCELLED', -6]])
  assert(header.includes(`SAMOSA_WHISPER_${name} = ${value}`) && original.includes(`WHISPER_TRANSCRIBE_${name} = ${value}`));
console.log('All seven result values preserved');
