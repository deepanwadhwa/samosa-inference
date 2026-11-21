// Run from the owning Inference checkout. Recoverable source blobs are baseline.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
const old=file=>execFileSync('git',['show',`1194d31:${file}`],{encoding:'utf8',maxBuffer:8*1024*1024});
const before=old('src/whisper_process.c'),after=fs.readFileSync('src/whisper_process.c','utf8');
for(const name of ['path_copy','mkdirs','read_file_bytes_limit','read_file_limit','utf8_scalar_count','trim_ascii_ws']){
  const start=before.indexOf('static ',before.lastIndexOf('\n\n',before.indexOf(name+'(')));
  const finish=before.indexOf('\n}\n',start)+3;
  assert(start>=0&&finish>start);assert(after.includes(before.slice(start,finish)),name+' changed');
}
const start=before.indexOf('    *output = read_file_limit('),finish=before.indexOf('\ndone:',start);
assert(after.includes(before.slice(start,finish)),'Original result/trim/UTF-8/16-MiB/error semantics changed');
const preflight=before.slice(before.indexOf('    if (!output)'),before.indexOf('    int result ='));
assert(after.includes(preflight),'Original public validation/acquire behavior changed');
const compact=s=>s.replace(/\s+/g,'');
for(const flag of ['-osrt','-otxt'])assert(compact(before).includes(compact(`execl(config->engine,config->engine,"-m",config->model,"-f",wav_path,"-l","en","-nt","${flag}","-of",out_base,(char*)NULL)`)));
assert(compact(after).includes(compact('const char *args[]={config->engine,"-m",config->model,"-f",input,"-l","en","-nt",timestamped?"-osrt":"-otxt","-of",out_base}')));
assert(!/\bfork\s*\(/.test(after));assert(after.includes('engine_process_start_command('));
const oldBackend=old('src/backend_process.c');const a=oldBackend.indexOf('static int executable_path('),b=oldBackend.indexOf('static int owner_lease(',a);
const helper=oldBackend.slice(a,b).replace('static int executable_path(','int engine_process_executable(');
assert(fs.readFileSync('src/engine_process.c','utf8').includes(helper),'Self helper move changed behavior');
const expectedBackend=(oldBackend.slice(0,a)+oldBackend.slice(b)).replace('#ifdef __APPLE__\n#include <mach-o/dyld.h>\n#endif\n','').replace('executable_path(self)','engine_process_executable(self)');
assert.equal(fs.readFileSync('src/backend_process.c','utf8'),expectedBackend,'Primary backend changed beyond self-helper move');
const parent=execFileSync('git',['-C','../..','show','77e9127:src/samosa_gateway.c'],{encoding:'utf8',maxBuffer:8*1024*1024});
let expectedParent=parent.replace('static void whisper_process_reaped(void *context, pid_t pid) {\n    track_job_pid(context, pid, 0);',
  'static void whisper_process_reaped(void *context, pid_t pid) {\n    Gateway *g = context;\n    pthread_mutex_lock(&g->voice_mu);\n    if (g->voice_transcription_pid == pid) g->voice_transcription_pid = 0;\n    pthread_mutex_unlock(&g->voice_mu);\n    track_job_pid(context, pid, 0);');
expectedParent=expectedParent.replace('                                   int preserve_cancel) {\n    *output = NULL;','                                   int preserve_cancel, int prepared_owned) {\n    *output = NULL;')
  .replace('    return samosa_whisper_process_run(&config, &control, wav_path, timestamped,\n                                      preserve_cancel, output);','    return prepared_owned\n        ? samosa_whisper_process_run_owned(&config, &control, wav_path, timestamped, preserve_cancel, output)\n        : samosa_whisper_process_run(&config, &control, wav_path, timestamped, preserve_cancel, output);')
  .replace('used_fingerprint, sizeof(used_fingerprint), 1);','used_fingerprint, sizeof(used_fingerprint), 1, compressed && decoded_path[0]);')
  .replace('sizeof(used_fingerprint), 1);','sizeof(used_fingerprint), 1, 1);')
  .replace('fingerprint, sizeof(fingerprint), 0);','fingerprint, sizeof(fingerprint), 0, 1);');
assert.equal(fs.readFileSync('../../src/samosa_gateway.c','utf8'),expectedParent,'Gateway changed beyond PID withdrawal and explicit prepared-input ownership');
assert(after.includes('const char *input=wav_path;'));
assert(after.includes('return whisper_process_run(config,control,wav_path,timestamped,preserve_cancel,output,0);'));
assert(after.includes('return whisper_process_run(config,control,wav_path,timestamped,preserve_cancel,output,1);'));
function assertions(source){const result=[];const re=/\bassert\s*\(/g;let m;while((m=re.exec(source))){let i=re.lastIndex,begin=i,depth=1;for(;i<source.length&&depth;i++){const c=source[i];if(c==='"'||c==="'"){const quote=c;for(i++;i<source.length;i++){if(source[i]==='\\'){i++;continue;}if(source[i]===quote)break;}}else if(c==='(')depth++;else if(c===')')depth--;}
  assert(depth===0);result.push(compact(source.slice(begin,i-1)));re.lastIndex=i;}return result;}
const oldAssertions=assertions(old('tests/test_whisper_process.c')),newAssertions=assertions(fs.readFileSync('tests/test_whisper_process.c','utf8'));
for(const expression of oldAssertions){if(expression==="argc==2&&argv[1][0]=='/'"){assert(newAssertions.includes("(argc==2||argc==3)&&argv[1][0]=='/'"));continue;}const at=newAssertions.indexOf(expression);assert(at>=0,'Original assertion missing: '+expression);newAssertions.splice(at,1);}
const oldReal=old('tests/test_whisper_process_real.c');
assert.equal(fs.readFileSync('tests/test_whisper_process_real.c','utf8'),oldReal.replace('int main(int argc,char **argv) {','int main(int argc,char **argv) {\n    int dispatched=samosa_whisper_process_dispatch(argc,argv);if(dispatched>=0)return dispatched;'));
console.log(`PASS six original helpers, full result validation/return mapping, original txt/SRT CLI argv, ${oldAssertions.length} original fixture assertions and unchanged actual-recognition assertions`);
console.log('PASS existing guardian reused; complete primary owner unchanged except self-helper move; entire Gateway unchanged except PID withdrawal before guardian acknowledgement and explicit prepared-input ownership');
