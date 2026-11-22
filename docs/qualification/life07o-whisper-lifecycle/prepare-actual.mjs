// Same canonical PCM construction as the existing test_whisper_real.c gate.
import fs from 'node:fs';import assert from 'node:assert/strict';
const [source,destination]=process.argv.slice(2);assert(source?.startsWith('/')&&destination?.startsWith('/'));
const raw=fs.readFileSync(source);assert(raw.toString('ascii',0,4)==='RIFF'&&raw.toString('ascii',8,12)==='WAVE');
let data,format;for(let at=12;at+8<=raw.length;){const bytes=raw.readUInt32LE(at+4);assert(at+8+bytes<=raw.length);const id=raw.toString('ascii',at,at+4);if(id==='fmt ')format=raw.subarray(at+8,at+8+bytes);if(id==='data')data=raw.subarray(at+8,at+8+bytes);at+=8+bytes+(bytes%2);}
assert(format&&data&&format.readUInt16LE(0)===1&&format.readUInt16LE(2)===1&&format.readUInt32LE(4)===16000&&format.readUInt16LE(14)===16);
const ms=Math.floor(data.length/32);assert(ms>=500&&ms<=30000);const canonical=Buffer.alloc(44+ms*32);
canonical.write('RIFF');canonical.writeUInt32LE(canonical.length-8,4);canonical.write('WAVEfmt ',8);canonical.writeUInt32LE(16,16);canonical.writeUInt16LE(1,20);canonical.writeUInt16LE(1,22);canonical.writeUInt32LE(16000,24);canonical.writeUInt32LE(32000,28);canonical.writeUInt16LE(2,32);canonical.writeUInt16LE(16,34);canonical.write('data',36);canonical.writeUInt32LE(ms*32,40);data.copy(canonical,44,0,ms*32);
fs.writeFileSync(destination,canonical,{mode:0o600,flag:'wx'});console.log(`Retained synthetic native speech: ${ms}ms canonical PCM; same original framing; dropped only sub-millisecond tail`);
