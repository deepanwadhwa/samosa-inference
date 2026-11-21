// Run from the owning Inference checkout. Baseline blobs are recoverable Git input.
import fs from 'node:fs';
import assert from 'node:assert/strict';
import {execFileSync} from 'node:child_process';
const before=execFileSync('git',['show','20832f5:src/backend_process.c'],{encoding:'utf8'});
const after=fs.readFileSync('src/backend_process.c','utf8');
const launch=before.slice(before.indexOf('        /* SAMOSA_BIND'),before.indexOf('        _Exit(127);')).replace(/^        /gm,'    ');
assert(after.includes(launch),'Original complete engine env/log/argv/exec body changed');
for(const name of ['backend_write_small_file','backend_tcp_connect','backend_send_all','samosa_backend_process_probe']){
  const declaration=name==='backend_write_small_file'?'static int ':name==='backend_tcp_connect'?'static int ':name==='backend_send_all'?'static int ':'int ';
  const start=before.indexOf(declaration+name+'(');assert(start>=0);
  const finish=before.indexOf('\n}\n',start)+3;assert(finish>start);
  assert(after.includes(before.slice(start,finish)),name+' changed');
}
assert.equal(fs.readFileSync('src/llama_launch.c','utf8'),execFileSync('git',['show','20832f5:src/llama_launch.c'],{encoding:'utf8'}));
assert(!/\bfork\s*\(/.test(after),'Primary owner must reuse the existing guardian');
assert(after.includes('engine_process_start_server('));
console.log('PASS original primary environment/log/argv/exec, llama launch, marker writer and bounded HTTP probe preserved; guardian reused');
const parent=execFileSync('git',['-C','../..','show','eed930b:src/samosa_gateway.c'],{encoding:'utf8',maxBuffer:8*1024*1024});
const expected=parent.replace('    char chats[PATH_MAX];\n    if (!path_join(chats, sizeof(chats), g->home, "chats") || !mkdirs(chats)) return 0;',
  '    char chats[PATH_MAX], run[PATH_MAX];\n    if (!path_join(chats, sizeof(chats), g->home, "chats") || !mkdirs(chats) ||\n        !path_join(run, sizeof(run), g->home, "run") || !mkdirs(run)) return 0;')
  .replace('int main(int argc, char **argv) {','int main(int argc, char **argv) {\n    int dispatched=samosa_backend_process_dispatch(argc,argv);if(dispatched>=0)return dispatched;');
assert.equal(fs.readFileSync('../../src/samosa_gateway.c','utf8'),expected,'Gateway changed beyond dispatch/directory setup boundary');
console.log('PASS complete Gateway unchanged except early private dispatch and existing run-directory creation before ownership acquisition');
