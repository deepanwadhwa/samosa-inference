import {readFileSync} from 'node:fs';
import {spawnSync,execFileSync} from 'node:child_process';
import path from 'node:path';

const [client, fixture] = process.argv.slice(2);
if (!client || !fixture) throw Error('Pass absolute native client and fixture paths');
const pins = new Map(readFileSync('docs/qualification/life07h-qwen-sources/actual-artifacts.sha256','utf8')
    .trim().split('\n').map(line => [line.slice(66),line.slice(0,64)]));
const engine = '/private/tmp/samosa-life07h-real.2wwUok/qwen36b';
const model = path.join(path.dirname(engine),'model');
const library = '/opt/homebrew/opt/libomp/lib/libomp.dylib';
const libraryPin = execFileSync('shasum',['-a','256',library],{encoding:'utf8'}).slice(0,64);
console.log('Installed OpenMP pin:', libraryPin, library);
const env = {...process.env, MAX_FOOTPRINT_MB:'6144', MAX_SWAP_DELTA_MB:'0'};
for (const [asset, sha] of pins) if (path.dirname(asset) === model) env[path.basename(asset)] = sha;
const command = spawnSync('bash', ['../../tools/run_memory_guarded.sh',client,fixture,engine,model,pins.get(engine),library,libraryPin],
    {stdio:'inherit',env});
if (command.error) throw command.error;
process.exitCode = command.status ?? 1;
