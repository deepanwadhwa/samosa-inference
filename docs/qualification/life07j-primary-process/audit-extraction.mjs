import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {execFileSync} from 'node:child_process';
import {tmpdir} from 'node:os';
const here=path.dirname(fileURLToPath(import.meta.url)),owner=path.resolve(here,'../../..'),root=path.resolve(owner,'../..');
const src=execFileSync('git',['show','7dc5a37:src/samosa_gateway.c'],{cwd:root,encoding:'utf8',maxBuffer:8*1024*1024});
const http=execFileSync('git',['show','2afb6c5:engines/samosa_http.h'],{cwd:owner,encoding:'utf8'});
function func(text,name){const re=new RegExp('^(?:static )?(?:int|void|pid_t) '+name+'\\([^\\n]*\\) \\{[\\s\\S]*?^\\}','m');const m=text.match(re);if(!m)throw Error('missing '+name);return m[0];}
function body(name){const s=func(src,name);return s.slice(s.indexOf('{')+1,-1);}
const helpers=[func(src,'write_small_file').replace('write_small_file','backend_write_small_file'),func(src,'tcp_connect').replace('tcp_connect','backend_tcp_connect'),func(http,'samosa_send_all').replace('samosa_send_all','backend_send_all')];
const state=s=>s.replaceAll('&g->mu','owner->mutex').replaceAll('g->backend_pid_file','owner->pid_file').replaceAll('g->backend_pgid','owner->process->pgid').replaceAll('g->backend_pid','owner->process->pid');
let running=state(body('backend_child_running'));
let probe=body('backend_probe');probe=probe.slice(probe.indexOf('    if (!backend_child_running(g))')).replace('backend_child_running(g)','samosa_backend_process_running(owner)').replaceAll('g->backend_port','port').replaceAll('g->backend','backend').replaceAll('tcp_connect','backend_tcp_connect').replaceAll('samosa_send_all','backend_send_all');
let stop=body('backend_stop').replace('    int upstream = g->upstream_fd;\n','').replace('    g->upstream_fd = -1;\n','').replace('    pthread_mutex_unlock(&g->mu);','    if (control && control->detach) control->detach(control->context);\n    pthread_mutex_unlock(&g->mu);').replace('    if (upstream >= 0) shutdown(upstream, SHUT_RDWR);\n    (void)samosa_mm_supervisor_cancel(&g->multimodal_supervisor);','    if (control && control->interrupted) control->interrupted(control->context);');stop=state(stop);
let start=body('backend_start');start=start.slice(start.indexOf('    /* Never launch'));const rt=start.indexOf('    RuntimeConfig runtime;'),fork=start.indexOf('    pid_t pid = fork();');start=start.slice(0,rt)+'    const SamosaBackendRuntime *runtime = &config->runtime;\n'+start.slice(fork);
const ll=start.indexOf('            int is_ornith = '),end=start.indexOf('            (void)samosa_llama_launch(&launch);',ll);if(ll<0||end<ll)throw Error('missing caller llama inputs');start=start.slice(0,ll)+'            SamosaLlamaLaunch launch = config->llama;\n'+start.slice(end);
start=state(start).replaceAll('g->backend_log','config->log').replaceAll('g->backend_port','config->port').replaceAll('g->qwen_model','config->model').replaceAll('g->maple_model','config->model').replaceAll('g->qwen_engine','config->engine').replaceAll('g->maple_engine','config->engine').replaceAll('g->tokenizer','config->tokenizer').replaceAll('g->backend','config->backend').replace('"SAMOSA_CHATS_DIR", chats','"SAMOSA_CHATS_DIR", config->chats').replaceAll('runtime.','runtime->').replaceAll('effective.','runtime->').replaceAll('tcp_connect','backend_tcp_connect').replaceAll('write_small_file','backend_write_small_file');
probe="\n"+probe; start="\n"+start;
const original=`#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/backend_process.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Existing primary-backend implementation, with explicit trusted owner inputs. */
${helpers.join('\n\n')}

pid_t samosa_backend_process_pid(const SamosaBackendOwner *owner) {
    pthread_mutex_lock(owner->mutex);
    pid_t pid = owner->process->pid;
    pthread_mutex_unlock(owner->mutex);
    return pid;
}

int samosa_backend_process_running(const SamosaBackendOwner *owner) {${running}}

int samosa_backend_process_probe(const SamosaBackendOwner *owner, const char *backend, int port) {${probe}}

void samosa_backend_process_stop(const SamosaBackendOwner *owner, const SamosaBackendStopControl *control) {${stop}}

int samosa_backend_process_start(const SamosaBackendOwner *owner, const SamosaBackendLaunch *config) {${start}}
`;
if(process.argv[2]==='--emit-original'){fs.writeFileSync(process.argv[3],original);console.log('Emitted exact original helpers/lifecycle after explicit owner/config/callback substitutions.');}
else {
    const directory=fs.mkdtempSync(path.join(tmpdir(),'samosa-primary-audit-'));
    const target=path.join(directory,'backend_process.c');
    try {
        fs.writeFileSync(target,original);
        execFileSync('git',['apply',path.join(here,'cleanup-fix.diff')],{cwd:directory});
        if(fs.readFileSync(path.join(owner,'src/backend_process.c'),'utf8')!==fs.readFileSync(target,'utf8'))throw Error('unexpected extraction change');
    } finally {fs.unlinkSync(target);fs.rmdirSync(directory);}
    const current=fs.readFileSync(path.join(root,'src/samosa_gateway.c'),'utf8');
    function policy(text,name){const match=text.match(new RegExp('^static (?:int|void) '+name+'\\([^;{]*\\) \\{[\\s\\S]*?^\\}','m'));if(!match)throw Error('missing policy '+name);return match[0];}
    for(const name of ['backend_available','runtime_config_load','runtime_config_save','runtime_effective','jobs_stop'])if(policy(src,name)!==policy(current,name))throw Error('caller policy changed: '+name);
    const llama=text=>{const start=text.indexOf('int is_ornith = !strcmp(g->backend, "ornith");');if(start<0)throw Error('missing llama selection');return text.slice(start,text.indexOf('};',start)+2).split('\n').map(line=>line.trimStart()).join('\n');};
    if(llama(src)!==llama(current))throw Error('llama selection/override inputs changed');
    if(/g->backend_pid\b|g->backend_pgid\b/.test(current))throw Error('remaining private PID access');
    const originalDriver=execFileSync('git',['show','7dc5a37:tests/test_model_selection.sh'],{cwd:root,encoding:'utf8'});
    let driver=originalDriver.replace('field() { printf \'%s\' "$1" | python3 -c "import json,sys; v=json.load(sys.stdin).get(\'$2\'); print(\'\' if v is None else v)"; }',()=> 'field() { printf \'%s\' "$1" | node tests/model_selection_checks.mjs field "$2"; }');
    driver=driver.replaceAll("python3 -c 'import time; print(int(time.time()*1000))'",()=> 'node tests/model_selection_checks.mjs now');
    if(driver!==fs.readFileSync(path.join(root,'tests/test_model_selection.sh'),'utf8'))throw Error('original selection assertions changed');
    console.log('PASS: original native helpers/launch/probe/group/marker bodies plus only the recorded WNOWAIT cleanup fix; caller availability/settings/llama inputs unchanged; selection assertions preserved.');
}
