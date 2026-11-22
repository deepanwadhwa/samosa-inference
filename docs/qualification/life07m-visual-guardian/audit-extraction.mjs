// Run from Inference; recoverable owner/Chat blobs pin the working baseline.
import fs from 'node:fs';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
const old=file=>execFileSync('git',['show',`f06d5c1:${file}`],{encoding:'utf8',maxBuffer:8*1024*1024});
function body(source,name){const at=source.indexOf(name+'(');assert(at>=0,name);const begin=source.lastIndexOf('\n',at)+1,end=source.indexOf('\n}\n',at)+3;assert(end>begin);return source.slice(begin,end);}
const before=old('src/specialist_supervisor.c'),after=fs.readFileSync('src/specialist_supervisor.c','utf8');
for(const name of ['mm_error','mm_now_ms','mm_regular_executable','mm_directory_no_symlink','mm_set_nonblock','mm_check','mm_write_deadline','mm_read_exact_deadline','mm_send','mm_receive','mm_reply_ok','mm_supervisor_release','samosa_mm_supervisor_init','samosa_mm_supervisor_destroy','samosa_mm_session_request_controlled','samosa_mm_session_request','samosa_mm_supervisor_active_pgid','samosa_mm_supervisor_is_active'])assert.equal(body(after,name),body(before,name),name+' changed');
const hup='        if (pfd.revents & (POLLERR | POLLNVAL)) return 0;';
assert.equal(body(after,'mm_wait_fd'),body(before,'mm_wait_fd').replace(hup,hup+'\n        if ((events & POLLOUT) && (pfd.revents & POLLHUP)) return 0;'),'Transport changed beyond reproduced Mac write-HUP fix');
const open=source=>source.slice(source.indexOf('    MmRequestControl request = {.control = control};\n    if (lifecycle)'),source.indexOf('    } else {\n        ',source.indexOf('    MmRequestControl request = {.control = control};\n    if (lifecycle)')));
assert(open(before).length>100);assert.equal(open(after),open(before),'External lifecycle ownership changed');
assert.equal(fs.readFileSync('src/specialist_protocol.c','utf8'),old('src/specialist_protocol.c'),'Original launch profiles, codecs, limits/defaults/traces changed');
assert(!/\bfork\s*\(/.test(after));assert(after.includes('engine_process_start_specialist('));
assert.equal(execFileSync('git',['diff','--name-only','f06d5c1','--','engines'],{encoding:'utf8'}).trim(),'','Model engine implementation changed');
for(const file of ['tests/test_multimodal_supervisor.c','tests/test_specialist_launch.c']){
 const expected=old(file).replace(/int main\(int argc,\s*char \*\*argv\) \{/,m=>m+'\n    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;');
 assert.equal(fs.readFileSync(file,'utf8'),expected,'Original assertions changed: '+file);
}
assert.equal(fs.readFileSync('tests/test_specialist_protocol.c','utf8'),old('tests/test_specialist_protocol.c'));
const gateway=execFileSync('git',['-C','../..','show','7f47e25:src/samosa_gateway.c'],{encoding:'utf8',maxBuffer:8*1024*1024});
const start=gateway.indexOf('static int vision_turn_inspect('),finish=gateway.indexOf('\nstatic int vision_read_ocr',start);assert(start>=0&&finish>start);let inspect=gateway.slice(start,finish);
inspect=inspect.replace('const char *image_path, const char *prompt,','const char *image_path, int prepared_owned, const char *prompt,')
 .replace('    if (turn->provider == 2)',`    char owned_image[PATH_MAX];const char *runtime_image=image_path;
    if(prepared_owned){
        if(!samosa_mm_session_take_temporary_image(&turn->session,image_path,owned_image,sizeof(owned_image))){path_copy(out_code,code_cap,"vision_temp_image_handoff_failed");return 0;}
        runtime_image=owned_image;
    }
    if (turn->provider == 2)`)
 .replaceAll('&turn->session, image_path,','&turn->session, runtime_image,')
 .replace('    vision_turn_release_runtime(g, turn);',`    if(prepared_owned&&!samosa_mm_session_return_temporary_image(&turn->session,image_path)){
        vision_turn_release_runtime(g,turn);path_copy(out_code,code_cap,"vision_temp_image_return_failed");return 0;
    }
    vision_turn_release_runtime(g, turn);`)
 .replace('    return visionpsy_session_inspect(g, &turn->session, runtime_image,',`    if(prepared_owned){
        if(!samosa_mm_session_take_temporary_image(&turn->session,image_path,owned_image,sizeof(owned_image))){path_copy(out_code,code_cap,"vision_temp_image_handoff_failed");return 0;}
        runtime_image=owned_image;
    }
    return visionpsy_session_inspect(g, &turn->session, runtime_image,`);
const expected=(gateway.slice(0,start)+inspect+gateway.slice(finish)).replace('vision_turn_inspect(g, vision_turn, blob_path,','vision_turn_inspect(g, vision_turn, blob_path, 0,').replace('vision_turn_inspect(g, vision_turn, tmp_path,','vision_turn_inspect(g, vision_turn, tmp_path, 1,');
assert.equal(fs.readFileSync('../../src/samosa_gateway.c','utf8'),expected,'Gateway changed beyond explicit PDF ownership/give-back for existing retry');
console.log('PASS original framing/codec/limits/timeouts/diagnostics and launch profiles unchanged; Mac write-HUP is the sole transport fix; external lifecycle branch preserved');
console.log('PASS all original supervisor/launch/protocol test assertions preserved; model engine sources unchanged; default launcher reuses the existing guardian');
console.log('PASS complete Gateway unchanged except explicit prepared-PDF-page handoff and ownership return/retake around the original lower-side retry');
