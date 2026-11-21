// Explicit retained native inputs; private state only. No model fetch/conversion.
import fs from 'node:fs';
import path from 'node:path';
import net from 'node:net';
import {spawn} from 'node:child_process';
import {once} from 'node:events';
import assert from 'node:assert/strict';
const [gateway, engine, model, guard, record] = process.argv.slice(2);
assert([gateway,engine,model,guard,record].every(p=>p?.startsWith('/')));
const home=fs.mkdtempSync('/private/tmp/samosa-primary-real.');
fs.mkdirSync(path.join(home,'run'),{mode:0o700});
fs.mkdirSync(path.join(home,'chats'),{mode:0o700});
fs.writeFileSync(path.join(home,'model-backend'),'qwen\n',{mode:0o600});
const wrapper=path.join(home,'guarded-qwen');
fs.writeFileSync(wrapper,'#!/bin/sh\nexec /bin/bash "$SAMOSA_PRIMARY_MEMORY_GUARD" "$SAMOSA_PRIMARY_REAL_ENGINE" "$@"\n',{mode:0o500});
async function freePort(){const s=net.createServer();await new Promise((resolve,reject)=>{s.once('error',reject);s.listen(0,'127.0.0.1',resolve);});const port=s.address().port;await new Promise(resolve=>s.close(resolve));return port;}
const port=await freePort();let backendPort=await freePort();while(backendPort===port)backendPort=await freePort();
const env={...process.env,SAMOSA_HOME:home,SAMOSA_PORT:String(port),SAMOSA_BACKEND_PORT:String(backendPort),SAMOSA_BIND:'127.0.0.1',SAMOSA_LAN:'0',SAMOSA_QWEN_ENGINE:wrapper,SAMOSA_QWEN_MODEL:model,SAMOSA_TOKENIZER:path.join(model,'tokenizer_qwen36.json'),SAMOSA_BONSAI_MODEL:'/missing-owned-bonsai',SAMOSA_ORNITH_MODEL:'/missing-owned-ornith',SAMOSA_PRIMARY_MEMORY_GUARD:guard,SAMOSA_PRIMARY_REAL_ENGINE:engine,OMP_NUM_THREADS:'2',SAMOSA_CONTEXT_TOKENS:'512',MAX_FOOTPRINT_MB:'6144',MAX_SWAP_DELTA_MB:'0'};
delete env.SAMOSA_LAN_PASSWORD;
const output=fs.openSync(path.join(home,'gateway.log'),'w',0o600);
const child=spawn(gateway,[],{env,stdio:['ignore',output,output]});fs.closeSync(output);
const ended=once(child,'exit');let ownedPid=0;
try {
  let health;
  for(let i=0;i<300;i++){
    assert(child.exitCode===null,'gateway exited before ready');
    try{const response=await fetch(`http://127.0.0.1:${port}/healthz`,{signal:AbortSignal.timeout(1000)});health=await response.json();if(health.ready)break;}catch{}
    await new Promise(resolve=>setTimeout(resolve,100));
  }
  assert(health?.ready&&health.backend==='qwen');
  ownedPid=Number(fs.readFileSync(path.join(home,'run/backend.pid'),'utf8').trim());assert(ownedPid>1&&health.pid===ownedPid);
  const response=await fetch(`http://127.0.0.1:${backendPort}/v1/chat/completions`,{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({messages:[{role:'user',content:'Say hello.'}],max_tokens:4,thinking:false,stream:false}),signal:AbortSignal.timeout(120000)});
  assert(response.ok);const reply=await response.json();assert.match(reply.choices?.[0]?.message?.content??'',/hello/i);assert(reply.usage?.completion_tokens>0&&reply.usage.completion_tokens<=4);
  fs.writeFileSync(path.join(home,'response.json'),JSON.stringify(reply,null,2)+'\n',{mode:0o600});
  const shutdown=await fetch(`http://127.0.0.1:${backendPort}/v1/shutdown`,{method:'POST',signal:AbortSignal.timeout(5000)});assert(shutdown.ok);
  let completed=false;for(let i=0;i<100;i++){if(fs.readFileSync(path.join(home,'backend.log'),'utf8').includes('[Memory Guard] Process exited with code 0')){completed=true;break;}await new Promise(resolve=>setTimeout(resolve,100));}assert(completed,'guard did not report native HTTP shutdown');
  child.kill('SIGTERM');const result=await Promise.race([ended,new Promise((_,reject)=>setTimeout(()=>reject(Error('gateway stop deadline')),15000).unref())]);assert(result[0]===0);
  for(let i=0;i<100;i++){try{process.kill(ownedPid,0);}catch(e){if(e.code==='ESRCH'){ownedPid=0;break;}throw e;}await new Promise(resolve=>setTimeout(resolve,20));}
  assert(ownedPid===0,'owned backend survived Gateway shutdown');
  const log=fs.readFileSync(path.join(home,'backend.log'),'utf8');assert(!log.includes('[Memory Guard] ABORT'));assert(log.includes('Peak process-tree RSS:'));assert(log.includes('Final swap delta: 0 MB'));
  fs.writeFileSync(record,home+'\n');
  console.log(`PASS actual retained Qwen through primary owner: ${reply.usage.prompt_tokens} prompt/${reply.usage.completion_tokens} generated, ${JSON.stringify(reply.choices[0].message.content)}, zero swap growth, native HTTP shutdown and Gateway/owned backend stopped; ${home}`);
} finally {
  if(child.exitCode===null&&child.signalCode===null){child.kill('SIGTERM');await Promise.race([ended,new Promise(resolve=>setTimeout(resolve,12000).unref())]);}
  if(ownedPid>1){try{process.kill(-ownedPid,'SIGTERM');}catch{}try{process.kill(-ownedPid,'SIGKILL');}catch{}}
}
