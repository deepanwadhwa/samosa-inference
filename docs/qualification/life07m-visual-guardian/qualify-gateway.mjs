// Actual compiled Gateway, original PDF routing, native helper and killed owner.
import fs from 'node:fs';import path from 'node:path';import net from 'node:net';
import assert from 'node:assert/strict';import {spawn,execFileSync} from 'node:child_process';import {once} from 'node:events';
const [gateway,engine,backend,kind,record]=process.argv.slice(2);assert([gateway,engine,backend,record].every(p=>p?.startsWith('/')));assert(['vision','molmo'].includes(kind));
const repository=process.cwd(),root=fs.mkdtempSync('/private/tmp/samosa-visual-gateway.'),home=path.join(root,'home'),model=path.join(root,'model');fs.mkdirSync(home,{mode:0o700});fs.mkdirSync(model,{mode:0o700});
fs.mkdirSync(path.join(home,'qwen-model'));fs.writeFileSync(path.join(home,'qwen-model/experts.bin'),'fixture\n');fs.writeFileSync(path.join(root,'tokenizer.json'),'fixture\n');fs.writeFileSync(path.join(root,'app.html'),'<!doctype html><title>Visual guardian fixture</title>\n');fs.writeFileSync(path.join(root,'logo.png'),'fixture\n');
if(kind==='vision')execFileSync(process.execPath,[path.join(repository,'tests/visionpsy_e2e_checks.mjs'),'model-fixture',model]);
else{
  for(const [name,body] of [['weights.safetensors','w\n'],['tokenizer.json','t\n'],['config.json','{}\n'],['processor.json','{}\n']])fs.writeFileSync(path.join(model,name),body);
  fs.writeFileSync(path.join(model,'manifest.json'),JSON.stringify({format:'samosa.molmo2.mlx.v1',package_id:'molmo2-4b-mlx-q4-v1',model_id:'allenai/Molmo2-4B',upstream_revision:'042abfa7a38879a376cec03d949eff0aefaa0600',processor_fingerprint:'808de9add76144a557348c5f5180a8408b12ca83592c7a6a257ae69c968e51df',estimated_resident_bytes:300000000,quantization:{mode:'affine',bits:4,group_size:64},files:[{name:'weights.safetensors',role:'weights',bytes:2,sha256:'0'.repeat(64)},{name:'tokenizer.json',role:'tokenizer',bytes:2,sha256:'95e80901c901584f416b8fd4349fd60022774b89ba4377626511f0562cc599f7'},...['config.json','processor.json'].map(name=>({name,role:'metadata',bytes:3,sha256:name==='config.json'?'17e072e3c3b29d9be7a348c74b88658e67ccce094c31b21f87646f6cecd2a76f':'808de9add76144a557348c5f5180a8408b12ca83592c7a6a257ae69c968e51df'}))]})+'\n');
}
const extractor=path.join(root,'extract'),ocr=path.join(root,'ocr');
fs.writeFileSync(extractor,String.raw`#!/bin/sh
case "$1" in
 --version) echo samosa-extract-test-v1 ;;
 --json) echo '{"ok":true,"text_layer":true,"page_count":3,"text":"PAGE ONE\nPAGE TWO\nPAGE THREE","tokens":6,"tokens_estimate":6}' ;;
 --json-pages) echo "{\"ok\":true,\"text_layer\":true,\"page_count\":3,\"page_start\":$3,\"page_end\":$3,\"pages\":[{\"index\":$3,\"text_chars\":8,\"tokens\":2,\"has_raster_figure\":true,\"text\":\"PAGE $3\"}],\"text\":\"PAGE $3\",\"tokens_estimate\":2}" ;;
 --render-ppm) printf 'P6\n1 1\n255\n\000\000\000' > "$4"; echo "{\"ok\":true,\"page\":$3,\"format\":\"image/x-portable-pixmap\"}" ;;
 *) exit 64 ;;
esac
`,{mode:0o700});
fs.writeFileSync(ocr,'#!/bin/sh\nif [ "$1" = --version ]; then echo samosa-ocr-test-v1; else echo \'{"ok":true,"text":"LINE ONE","lines":[{"text":"LINE ONE","conf":0.99}]}\'; fi\n',{mode:0o700});
async function port(){const server=net.createServer();await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(0,'127.0.0.1',resolve);});const n=server.address().port;await new Promise(resolve=>server.close(resolve));return n;}
const publicPort=await port();let backendPort=await port();while(backendPort===publicPort)backendPort=await port();
const identity=path.join(root,'identity'),helperFile=path.join(root,'helper'),ready=path.join(root,'ready'),requests=path.join(root,'requests');
const env={...process.env,SAMOSA_HOME:home,SAMOSA_PORT:String(publicPort),SAMOSA_BACKEND_PORT:String(backendPort),SAMOSA_BIND:'127.0.0.1',SAMOSA_LAN:'0',SAMOSA_APP_HTML:path.join(root,'app.html'),SAMOSA_APP_LOGO:path.join(root,'logo.png'),SAMOSA_MODELS_CATALOG:path.join(repository,'assets/models.json'),SAMOSA_QWEN_ENGINE:backend,SAMOSA_QWEN_MODEL:path.join(home,'qwen-model'),SAMOSA_TOKENIZER:path.join(root,'tokenizer.json'),SAMOSA_EXTRACT:extractor,SAMOSA_OCR:ocr,SAMOSA_VISIONPSY_ENGINE:kind==='vision'?engine:'/missing-private-vision',SAMOSA_VISIONPSY_MODEL:model,SAMOSA_MOLMO2_ENGINE:kind==='molmo'?engine:'/missing-private-molmo',SAMOSA_MOLMO2_MODEL:model,SAMOSA_FAKE_MM_EXPECTED_MODEL:model,SAMOSA_FAKE_MM_FRAMED:kind==='molmo'?'1':'0',SAMOSA_FAKE_MM_IDENTITY:identity,SAMOSA_FAKE_MM_HELPER_PID_FILE:helperFile,SAMOSA_FAKE_MM_LOG:requests,SAMOSA_FAKE_MM_HANG_READY:ready,SAMOSA_FAKE_MM_LIFECYCLE:'stubborn',SAMOSA_FAKE_MM_BLOCK_REQUEST:'1'};delete env.SAMOSA_LAN_PASSWORD;
const peer=spawn('/bin/sleep',['300']),peerExit=once(peer,'exit');const out=fs.openSync(path.join(root,'gateway.log'),'w',0o600),child=spawn(gateway,[],{env,stdio:['ignore',out,out]});fs.closeSync(out);const ended=once(child,'exit'),delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
async function gone(pid){for(let i=0;i<300;i++){try{process.kill(pid,0);}catch(e){if(e.code==='ESRCH')return true;throw e;}await delay(10);}return false;}
const receipt={root,gateway,engine,backend,kind};let worker=0,helper=0;
try{
  let health;for(let i=0;i<300;i++){assert(child.exitCode===null,'Gateway exited');try{const response=await fetch(`http://127.0.0.1:${publicPort}/healthz`,{signal:AbortSignal.timeout(500)});health=await response.json();if(health.ready)break;}catch{}await delay(20);}assert(health?.ready);const primary=health.pid;assert(primary>1);
  const token=fs.readFileSync(path.join(home,'run/ui-token'),'utf8').trim(),pdf=Buffer.from('%PDF-1.4\nfixture\n');
  const upload=await fetch(`http://127.0.0.1:${publicPort}/v1/attachments`,{method:'POST',headers:{'X-Samosa-Token':token,'Content-Type':'application/pdf','X-Filename':'visual.pdf'},body:pdf,signal:AbortSignal.timeout(5000)});const attachment=await upload.json();assert(upload.ok&&attachment.id,JSON.stringify(attachment));
  const request=fetch(`http://127.0.0.1:${publicPort}/v1/chat/completions`,{method:'POST',headers:{'X-Samosa-Token':token,'Content-Type':'application/json'},body:JSON.stringify({attachment_ids:[attachment.id],stream:false,messages:[{role:'user',content:'Describe every chart across the entire document.'}]}),signal:AbortSignal.timeout(20000)}).then(async response=>({status:response.status,body:await response.text()})).catch(error=>({error:error.name}));
  let active=false;for(let i=0;i<600;i++){if(fs.existsSync(ready)){active=true;break;}await delay(20);}if(!active){receipt.request=await request;throw Error('Visual request never became active: '+JSON.stringify(receipt.request));}
  const ids=fs.readFileSync(identity,'utf8').trim().split(' ').map(Number);[worker,,]=ids;helper=Number(fs.readFileSync(helperFile,'utf8').trim());const guardian=ids[1];assert(ids.length===3&&ids.every(n=>Number.isSafeInteger(n)&&n>1)&&ids[0]===ids[2]&&helper>1);
  const calls=fs.readFileSync(requests,'utf8').trim().split('\n').map(line=>JSON.parse(line)),call=calls.find(item=>item.command==='inspect'||item.command==='analyze');assert(call);
  const prepared=call.image_path||call.media_path;
  const workspace=path.dirname(prepared);
  const privateRoot=path.join(fs.realpathSync('/tmp'),'samosa-inference-'+process.geteuid());
  assert(prepared.startsWith('/tmp/samosa-inference-specialist-')||
    (path.dirname(workspace)===privateRoot&&/^specialist-[a-z0-9]{6}$/i.test(path.basename(workspace))));
  assert(fs.statSync(prepared).size>0&&!(fs.statSync(prepared).mode&0o077));
  Object.assign(receipt,{primary,worker,helper,guardian,prepared,workspace:path.dirname(prepared)});
  assert(child.kill('SIGKILL'));const result=await ended;assert(result[1]==='SIGKILL');receipt.workerGone=await gone(worker);receipt.helperGone=await gone(helper);receipt.guardianGone=await gone(guardian);receipt.primaryGone=await gone(primary);worker=receipt.workerGone?0:worker;helper=receipt.helperGone?0:helper;
  receipt.preparedGone=!fs.existsSync(prepared);receipt.workspaceGone=!fs.existsSync(path.dirname(prepared));receipt.callerPagesGone=!fs.readdirSync(home).some(name=>name.startsWith('vision-page-'));receipt.peerSurvives=peer.exitCode===null&&peer.signalCode===null;process.kill(peer.pid,0);
  function files(directory){return fs.readdirSync(directory,{withFileTypes:true}).flatMap(entry=>{const file=path.join(directory,entry.name);return entry.isDirectory()?files(file):entry.isFile()?[file]:[];});}
  receipt.durablePdfRetained=files(path.join(home,'attachments')).some(file=>{const stat=fs.statSync(file);return stat.size===pdf.length&&fs.readFileSync(file).equals(pdf);});receipt.modelRetained=fs.existsSync(path.join(model,kind==='vision'?'model.safetensors':'weights.safetensors'));receipt.request=await request;
  fs.writeFileSync(record,JSON.stringify(receipt,null,2)+'\n');assert(receipt.workerGone&&receipt.helperGone&&receipt.guardianGone&&receipt.primaryGone&&receipt.preparedGone&&receipt.workspaceGone&&receipt.callerPagesGone&&receipt.peerSurvives&&receipt.durablePdfRetained&&receipt.modelRetained);
  console.log(`PASS actual Gateway ${kind} PDF SIGKILL: primary/visual engine/helper/guardian and prepared-page workspace gone; durable PDF/model and unrelated peer retained; ${root}`);
}finally{
  if(child.exitCode===null&&child.signalCode===null){child.kill('SIGKILL');await ended;}
  for(const pid of [worker,helper])if(pid>1){try{process.kill(pid,'SIGKILL');}catch(e){if(e.code!=='ESRCH')throw e;}}
  if(peer.exitCode===null&&peer.signalCode===null)peer.kill('SIGKILL');await peerExit;fs.writeFileSync(record,JSON.stringify(receipt,null,2)+'\n');
}
