// Actual native Gateway/Whisper execution under a killed owner; private fixtures.
import fs from 'node:fs';import path from 'node:path';import net from 'node:net';
import assert from 'node:assert/strict';import {spawn,execFileSync} from 'node:child_process';import {once} from 'node:events';
const [gateway,engine,record]=process.argv.slice(2);assert([gateway,engine,record].every(p=>p?.startsWith('/')));
const root=fs.mkdtempSync('/private/tmp/samosa-whisper-gateway.'),home=path.join(root,'home');fs.mkdirSync(home,{mode:0o700});
const repository=process.cwd();execFileSync(process.execPath,[path.join(repository,'tests/voice_gateway_checks.mjs'),'fixtures',root]);
fs.writeFileSync(path.join(root,'app.html'),'<!doctype html><title>Whisper guardian fixture</title>\n');fs.writeFileSync(path.join(root,'logo.png'),'fixture\n');
async function port(){const s=net.createServer();await new Promise((resolve,reject)=>{s.once('error',reject);s.listen(0,'127.0.0.1',resolve);});const result=s.address().port;await new Promise(resolve=>s.close(resolve));return result;}
const publicPort=await port();let backendPort=await port();while(backendPort===publicPort)backendPort=await port();
const marker=path.join(root,'worker'),model=path.join(root,'ggml-base.en.bin');
const env={...process.env,SAMOSA_HOME:home,SAMOSA_PORT:String(publicPort),SAMOSA_BACKEND_PORT:String(backendPort),SAMOSA_BIND:'127.0.0.1',SAMOSA_LAN:'0',SAMOSA_APP_HTML:path.join(root,'app.html'),SAMOSA_APP_LOGO:path.join(root,'logo.png'),SAMOSA_MODELS_CATALOG:path.join(repository,'assets/models.json'),SAMOSA_WHISPER_CLI:engine,SAMOSA_WHISPER_MODEL:model,SAMOSA_WHISPER_TINY_MODEL:path.join(root,'ggml-tiny.en.bin'),SAMOSA_QWEN_ENGINE:'/missing-fixture-qwen',SAMOSA_BONSAI_SERVER:'/missing-fixture-llama',SAMOSA_WHISPER_GATEWAY_ROOT:home+'/',SAMOSA_WHISPER_EXPECTED_MODEL:model,SAMOSA_WHISPER_FIXTURE_MODE:'stubborn-helper',SAMOSA_WHISPER_FIXTURE_MARKER:marker};delete env.SAMOSA_LAN_PASSWORD;
const log=fs.openSync(path.join(root,'gateway.log'),'w',0o600),child=spawn(gateway,[],{env,stdio:['ignore',log,log]});fs.closeSync(log);const exited=once(child,'exit');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function absent(pid){for(let i=0;i<300;i++){try{process.kill(pid,0);}catch(e){if(e.code==='ESRCH')return true;throw e;}await delay(10);}return false;}
let worker=0,helper=0;const receipt={root,gateway,engine};
try{
  let ready=false;for(let i=0;i<200;i++){assert(child.exitCode===null,'Gateway exited');try{const response=await fetch(`http://127.0.0.1:${publicPort}/healthz`,{signal:AbortSignal.timeout(500)});if(response.ok&&fs.existsSync(path.join(home,'run/ui-token'))){ready=true;break;}}catch{}await delay(10);}assert(ready);
  const token=fs.readFileSync(path.join(home,'run/ui-token'),'utf8').trim();
  const request=fetch(`http://127.0.0.1:${publicPort}/v1/voice/transcriptions`,{method:'POST',headers:{'X-Samosa-Token':token,'Content-Type':'audio/wav'},body:fs.readFileSync(path.join(root,'voice.wav')),signal:AbortSignal.timeout(15000)}).then(async response=>({status:response.status,body:await response.text()})).catch(error=>({error:error.name}));
  let lines;for(let i=0;i<300;i++){if(fs.existsSync(marker)){const rows=fs.readFileSync(marker,'utf8').trim().split('\n');if(rows.length>=3){lines=rows;break;}}await delay(10);}assert(lines,'CLI never became active');
  const ids=lines[0].split(' ').map(Number);[worker,helper]=ids;const guardian=ids[2];assert(ids.every(n=>Number.isSafeInteger(n)&&n>1));
  const prefix=lines[1],prepared=lines[2],workspace=path.dirname(prefix);assert(prefix.startsWith(home+'/voice/tmp/')&&prepared.startsWith(home+'/voice/tmp/'));
  assert(fs.statSync(prepared).size===3244);assert(fs.existsSync(prefix+'.txt'));
  receipt.worker=worker;receipt.helper=helper;receipt.guardian=guardian;receipt.prepared=prepared;receipt.workspace=workspace;
  assert(child.kill('SIGKILL'));const result=await exited;assert(result[1]==='SIGKILL');
  receipt.workerGone=await absent(worker);receipt.helperGone=await absent(helper);receipt.guardianGone=await absent(guardian);worker=receipt.workerGone?0:worker;helper=receipt.helperGone?0:helper;
  receipt.outputGone=!fs.existsSync(prefix+'.txt');receipt.workspaceGone=!fs.existsSync(workspace);receipt.preparedGone=!fs.existsSync(prepared);
  receipt.modelRetained=fs.statSync(model).size===147964211;receipt.uploadRetained=fs.statSync(path.join(root,'voice.wav')).size===3244;receipt.request=await request;
  fs.writeFileSync(record,JSON.stringify(receipt,null,2)+'\n');
  assert(receipt.workerGone&&receipt.helperGone&&receipt.guardianGone&&receipt.outputGone&&receipt.workspaceGone&&receipt.preparedGone,'Gateway death left an owned process or prepared/result file');
  assert.deepEqual(fs.readdirSync(path.join(home,'voice/tmp')),[]);
  console.log('PASS actual Gateway SIGKILL during Whisper: resistant engine/helper/guardian and owned prepared/result workspace gone; model/upload retained; '+root);
}finally{
  if(child.exitCode===null&&child.signalCode===null){child.kill('SIGKILL');await exited;}
  // Only live native fixture identities observed from this private worker file.
  for(const pid of [worker,helper])if(pid>1){try{process.kill(pid,'SIGKILL');}catch(e){if(e.code!=='ESRCH')throw e;}}
  fs.writeFileSync(record,JSON.stringify(receipt,null,2)+'\n');
}
