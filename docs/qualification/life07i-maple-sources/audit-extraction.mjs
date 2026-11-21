import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const here = path.dirname(fileURLToPath(import.meta.url));
const owner = path.resolve(here, '../../..');
const records = fs.readFileSync(path.join(here, 'source-audit.txt'), 'utf8').split('\n');
const pairs = JSON.parse(fs.readFileSync(path.join(here, 'copied-files.json'), 'utf8'));
for (const [original, extracted, kind] of pairs) {
  const record = records.find(line => line.endsWith(` ${original} = ${extracted}`));
  if (!record || !/^[a-f0-9]{64} /.test(record)) throw Error(`missing source pin: ${original}`);
  const relative = extracted.replace(/^components\/samosa-inference\//, '');
  let bytes = fs.readFileSync(path.join(owner, relative));
  if (kind === 'OWNER_INCLUDE_ONLY') {
    bytes = Buffer.from(bytes.toString().replaceAll('../engines/maple/', '../src/maple/'));
  }
  const digest = createHash('sha256').update(bytes).digest('hex');
  if (digest !== record.slice(0, 64)) throw Error(`unexpected source/fixture change: ${original}`);
}
console.log(`PASS: ${pairs.length} source/test/fixture pins; only recorded owner includes differ, all original assertions preserved.`);
