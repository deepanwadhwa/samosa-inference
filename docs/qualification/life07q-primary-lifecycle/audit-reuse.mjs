// Run from Chat. Compare original blobs as source; no legacy interpreter runs.
import fs from 'node:fs';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
const root='components/samosa-inference';const old=execFileSync('git',['-C',root,'show','db99bf7:src/backend_process.c'],{encoding:'utf8'});let source=fs.readFileSync(root+'/src/backend_process.c','utf8');
const cut=(s,a,b)=>{const i=s.indexOf(a),j=s.indexOf(b,i);assert(i>=0&&j>i,a);return s.slice(i,j);};
source=source.replace('#include "backend_process_internal.h"\n','');
const guard=cut(source,'typedef struct {\n    EngineProcess child;','typedef struct {\n    uint32_t magic;');source=source.replace(guard,cut(old,'typedef struct { EngineProcess child; } BackendGuard;','typedef struct {\n    uint32_t magic;'));
// Typed observation/cleanup and retained-fence behavior are the intended boundary
// changes. The separate tests exercise them against real processes, not a model.
source=source.replace(cut(source,'const char *samosa_backend_process_status(','int samosa_backend_process_probe('),cut(old,'int samosa_backend_process_running(','int samosa_backend_process_probe('));
source=source.replace(cut(source,'const char *samosa_backend_process_stop_owned(','static const char *start_owned('),cut(old,'void samosa_backend_process_stop(','int samosa_backend_process_start('));
let start=cut(source,'static const char *start_owned(','static void configuration_free(');const wrappers=start.indexOf('const char *samosa_backend_process_start_owned(');assert(wrappers>0);start=start.slice(0,wrappers);
start=start.replace('static const char *start_owned(const SamosaBackendOwner *owner, const SamosaBackendLaunch *config,EngineProcess *delegated) {','int samosa_backend_process_start(const SamosaBackendOwner *owner, const SamosaBackendLaunch *config) {');
start=start.replace('return "backend_port_occupied";','return 0;').replace('return "backend_ownership_unavailable";','return 0;').replace('return "engine_io_failed";','return 0;');
start=start.replace(`    BackendGuard *retained=owner->process->guardian;
    const char *retained_error=retained?(retained->status_error?retained->status_error:"backend_already_started"):NULL;
    pthread_mutex_unlock(owner->mutex);
    if(retained)return retained_error;`,`    int retained=owner->process->guardian!=NULL;
    pthread_mutex_unlock(owner->mutex);
    if(retained)return 0;`);
start=start.replace('    if(guard){guard->process=delegated?delegated:&guard->child;guard->lease=lease;}\n','').replace('&server,guard->process)','&server,&guard->child)').replace('free(command);fclose(configuration);if(!guard)close(lease);','free(command);fclose(configuration);close(lease);');
const newError=cut(start,'    if(error){','    pthread_mutex_lock(owner->mutex);\n    owner->process->guardian=guard;');assert(newError.includes('"engine_guardian_failed"')&&newError.includes('cleanup_retained(error)'));start=start.replace(newError,cut(old,'    if(error){','    pthread_mutex_lock(owner->mutex);\n    owner->process->guardian=guard;'));
start=start.replaceAll('guard->process->engine','guard->child.engine').replace('    return NULL;\n}','    return 1;\n}');
assert.equal(start,cut(old,'int samosa_backend_process_start(','static void configuration_free('),'Original start/port/marker/bootstrap launch changed beyond typed returns, canonical storage and retained cleanup fence');source=source.replace(cut(source,'static const char *start_owned(','static void configuration_free('),start);
assert.equal(source,old,'Other original backend implementation changed');
assert.equal(execFileSync('git',['-C',root,'diff','db99bf7','--','engines','src/activation.c','src/activation.h','src/engine_process.c','src/engine_process.h','src/backend_json.c','src/turn_control.c','src/llama_launch.c','tests/test_backend_process.c'],{encoding:'utf8'}),'','Original engines/Activation/guardian/transport/turn/launch code or process assertions changed');
for(const p of ['tools/package_hf.sh','tests/test_package_pdfium.sh','dist/install.sh']){const restored=fs.readFileSync(p,'utf8').replaceAll('src/backend_process.c src/backend_process_internal.h','src/backend_process.c').replaceAll('engine/inference/src/backend_process.c engine/inference/src/backend_process_internal.h','engine/inference/src/backend_process.c');assert.equal(restored,execFileSync('git',['show','8aab8f9:'+p],{encoding:'utf8'}),'Packaging changed beyond private header membership');}
assert.equal(fs.readFileSync('src/samosa_gateway.c','utf8'),execFileSync('git',['show','8aab8f9:src/samosa_gateway.c'],{encoding:'utf8',maxBuffer:8*1024*1024}),'Gateway implementation changed');
console.log('PASS entire original backend launch/bootstrap/health/environment/marker implementation preserved outside typed status/stop, retained unknown-cleanup fence and one borrowed canonical process record');
console.log('PASS original engines, Activation, guardian, JSON/turn/llama code and all original primary process assertions unchanged');
console.log('PASS Gateway unchanged; package/install logic and assertions differ only by the required private owner header membership');
