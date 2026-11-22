// Run from Inference. Read old blobs as text; never execute legacy Python.
import fs from 'node:fs';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
const old=name=>execFileSync('git',['show',`dbb680a:${name}`],{encoding:'utf8',maxBuffer:8*1024*1024});
let source=fs.readFileSync('src/whisper.c','utf8');
source=source.replace('#include <time.h>\n','').replace('int whisper_audio_valid(','static int wav(').replace('const char *whisper_run(','static const char *run(').replace('!whisper_audio_valid(audio,bytes)','!wav(audio,bytes)');
source=source.replace('(cfg->whisper->retain_cleanup?engine_process_start_bound_workspace_uncapped:engine_process_start_bound_workspace)', 'engine_process_start_bound_workspace');
source=source.replace('    if(!error&&cfg->owned_status)error=cfg->owned_status(cfg->owned_context);\n','');
source=source.replace('if(!error)error=cfg->whisper->run?cfg->whisper->run(cfg->whisper->context,cfg,audio,bytes,&items,&text):whisper_run(cfg,audio,bytes,&items,&text);','if(!error)error=run(cfg,audio,bytes,&items,&text);');
const begin=source.indexOf('    if(process.guardian>0){\n        const char *stopped;'),end=source.indexOf('    if(!error)error=inference_interrupted(cfg);',begin);assert(begin>=0&&end>begin);
source=source.slice(0,begin)+'    if(process.guardian>0){const char *stopped=engine_process_stop(&process);if(stopped&&!strcmp(stopped,"engine_shutdown_pending"))error=stopped;}\n'+source.slice(end);
assert.equal(source,old('src/whisper.c'),'Whisper algorithms/wire/CLI/results changed beyond renamed reuse, scoped owner check/dispatch and documented managed cleanup');
source=fs.readFileSync('src/activation.c','utf8');
for(const name of ['int activation_input_unchanged','void activation_cleanup_failed']){const a=source.indexOf(name+'('),b=source.indexOf('\n}\n',a)+3;assert(a>=0&&b>a);source=source.slice(0,a)+source.slice(b);}
source=source.replace('Input *input,int on_demand)','Input *input)');
source=source.replace(`    struct stat policy;
    if(on_demand){if(fstat(input->fd,&policy)||!S_ISREG(policy.st_mode)||policy.st_uid!=geteuid()||
        (policy.st_mode&022)||policy.st_nlink!=1)return "unsafe_activation_input";}
    else if(!private_file(input->fd,0))return "unsafe_activation_input";`,'    if(!private_file(input->fd,0))return "unsafe_activation_input";');
source=source.replaceAll('    if(atomic_load(&a->cleanup_failed))return "engine_guardian_failed";\n','').replace('a->on_demand=0;return e;','return e;');
source=source.replace(`    if(!a)return "provider_unavailable";
    if(!a->on_demand&&a->process.response<0)return "provider_unavailable";
    struct pollfd terminal={a->process.response,POLLIN|POLLHUP,0};
    if(!a->on_demand&&poll(&terminal,1,0)!=0)return "owned_engine_exited";`,`    if(!a||a->process.response<0)return "provider_unavailable";
    struct pollfd terminal={a->process.response,POLLIN|POLLHUP,0};if(poll(&terminal,1,0)!=0)return "owned_engine_exited";`);
source=source.replace('(c->on_demand!=0&&c->on_demand!=1)||(c->on_demand&&c->launch)||','').replace('((!c->launch&&!c->on_demand)||','(!c->launch||').replace('    a->on_demand=c->on_demand;\n','');
source=source.replace(',&engine,c->on_demand)',' ,&engine)'.trimStart()).replace(',&model,c->on_demand)',',&model)');
source=source.replace('if(!e&&!c->on_demand)e=c->launch?','if(!e)e=c->launch?').replace('if(!e&&!c->on_demand&&poll(&terminal,1,0)!=0)','if(!e&&poll(&terminal,1,0)!=0)');
source=source.replace('    if(!e&&c->on_demand&&(a->process.guardian>0||a->process.response>=0))e="invalid_on_demand_qualification";\n','').replace('            if(c->on_demand)fputs(",\\\"execution_mode\\\":\\\"on_demand\\\"",f);\n','').replace('    if(e&&c->on_demand&&!strcmp(e,"engine_guardian_failed"))activation_cleanup_failed(a);\n','');
assert.equal(source,old('src/activation.c'),'Resident activation changed beyond explicit on-demand input/qualification/status/cleanup adaptation');
const before=old('src/registry_host.c');source=fs.readFileSync('src/registry_host.c','utf8');
const a=source.indexOf('    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->on_demand_whispers[i])'),b=source.indexOf('    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->resident_visuals[i]);',a);assert(a>=0&&b>a);source=source.slice(0,a)+source.slice(b);
source=source.replace('&&!temporal&&!whisper)','&&!temporal)');source=source.split('\n').filter(line=>!line.startsWith('        whisper?(resident?')).join('\n');
const wb=source.indexOf('        if(whisper){'),we=source.indexOf('        if(vision){',wb),ob=before.indexOf('        if(whisper){'),oe=before.indexOf('        if(vision){',ob);assert(wb>=0&&we>wb&&ob>=0&&oe>ob);source=source.slice(0,wb)+before.slice(ob,oe)+source.slice(we);
source=source.replaceAll('||h->on_demand_whispers[i]','').replace('&&!h->on_demand_whispers[index]','').replace('WhisperHost *whisper=h->on_demand_whispers[index];','');
source=source.replace('            if(whisper)error=whisper->attempted?whisper_host_status(whisper):whisper_host_start(whisper,&h->providers[index]);\n            else if(visual)','            if(visual)');
source=source.replace('error=whisper?whisper_host_stop(whisper):visual?','error=visual?').replace('health=whisper?whisper_host_status(whisper):visual?','health=visual?').replace('!(whisper?whisper->attempted:visual?','!(visual?');
source=source.replace('            if(whisper)fputs(",\\\"execution_mode\\\":\\\"on_demand\\\",\\\"resident\\\":false",out);\n','');
assert.equal(source,before,'Other registry lifecycle/configuration/ownership behavior changed');
let fixture=fs.readFileSync('tests/whisper_fixture.h','utf8').replace('#ifdef SAMOSA_WHISPER_FIXTURE_ATOMIC\n#include <stdatomic.h>\nstatic atomic_uint checks;\n#else\nstatic unsigned checks;\n#endif','static unsigned checks;');assert.equal(fixture,old('tests/whisper_fixture.h'),'Original fixture helpers/assertions changed');
source=fs.readFileSync('tests/test_activation.c','utf8');const ib=source.indexOf('static const char *idle_qualify('),ie=source.indexOf('\n}\n',ib)+3;source=source.slice(0,ib)+source.slice(ie);const nb=source.indexOf('    /* Nonresident adapters qualify'),ne=source.indexOf('    printf("PASS activation:',nb);source=source.slice(0,nb)+source.slice(ne);assert.equal(source,old('tests/test_activation.c'),'Original activation assertions changed');
source=fs.readFileSync('src/engine_process.c','utf8');
source=source.replace('    int uncapped=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-workspace-uncapped");\n','').replace('int workspace=uncapped||specialist','int workspace=specialist').replace('Captured output is reserved for our existing bounded bridge.','Only our bounded bridge may opt out of the process-wide file cap.').replace('&&!duplex&&!uncapped){','&&!duplex){').replace('args[1]=transport==6?"--inference-owned-engine-workspace-uncapped":specialist?','args[1]=specialist?');
const ep=source.indexOf('const char *engine_process_start_bound_workspace_uncapped('),ee=source.indexOf('\n}\n',ep)+3;assert(ep>=0&&ee>ep);source=source.slice(0,ep)+source.slice(ee);
assert.equal(source,old('src/engine_process.c'),'Guardian changed outside explicit native file-output cap opt-in');
assert.equal(execFileSync('git',['diff','dbb680a','--','engines','src/whisper_process.c','src/backend_process.c','src/turn_control.c','src/summary_host.c','src/speech_host.c','src/visual_host.c','tests/test_whisper.c'],{encoding:'utf8'}),'','Original engines/guardians/default/other-modality implementations changed');
console.log('PASS original Whisper input/normalization/CLI/SRT/limits/cancellation/replay implementation reused unchanged except explicit owner routing/check and managed pending-cleanup retention');
console.log('PASS resident Activation/state transactions unchanged after removing explicit on-demand policy/status/cleanup additions; old registry behavior byte-identical outside the optional Whisper lifecycle');
console.log('PASS all original activation/Whisper fixture assertions retained; default long-window CLI, model engines and other modality owners unchanged; guardian differs only by explicit native file-output cap opt-in');
