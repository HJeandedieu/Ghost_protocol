// Local development tool: npm install --prefix build/kokoro-local kokoro-js@1.2.1
// Node.js runs Kokoro on CPU. Only generated OGG files ship with the game.
import { readFile, writeFile, mkdir } from 'node:fs/promises';
import { existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { KokoroTTS } from '../build/kokoro-local/node_modules/kokoro-js/dist/kokoro.js';
import { env } from '../build/kokoro-local/node_modules/@huggingface/transformers/dist/transformers.node.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
env.cacheDir = path.join(root, 'build/kokoro-models/js');
const script = new TextDecoder('windows-1252').decode(await readFile(path.join(root, 'docs/05_Design_Docs.md')));
const rows = [...script.matchAll(/^\| (V\d\d) \| (.*?) \| ([123]) \| "(.*?)" \|/gm)]
  .map(([, id, trigger, priority, text]) => ({ id, file: `audio/voice/${id}.ogg`, text, priority: Number(priority), trigger }));
if (rows.length !== 25 || rows.some((row, index) => row.id !== `V${String(index + 1).padStart(2, '0')}`))
  throw new Error('Expected exactly V01–V25 from the approved script');
await writeFile(path.join(root, 'build/day25-voice-manifest.json'), JSON.stringify(rows, null, 2) + '\n');
const only = process.argv[2];
const model = await KokoroTTS.from_pretrained('onnx-community/Kokoro-82M-v1.0-ONNX', {
  dtype: 'q8', device: 'cpu',
  progress_callback: info => { if (info.status === 'initiate' || info.status === 'done') console.log(info.status, info.file); },
});
for (const row of rows) {
  if (only && row.id !== only) continue;
  const destination = path.join(root, 'assets', row.file);
  if (existsSync(destination)) { console.log(`${row.id}: already rendered`); continue; }
  await mkdir(path.dirname(destination), { recursive: true });
  const audio = await model.generate(row.text, { voice: 'af_bella', speed: row.priority === 3 ? 1.1 : .98 });
  if (!audio.audio.length || !audio.audio.every(Number.isFinite)) throw new Error(`Invalid speech: ${row.id}`);
  const temporary = path.join(root, 'build/audio-generator', `${row.id}.wav`);
  await mkdir(path.dirname(temporary), { recursive: true });
  await audio.save(temporary);
  const filter = (row.priority === 3 ? 'asetrate=25440,aresample=24000,' : '') + 'loudnorm=I=-18:TP=-3:LRA=7';
  execFileSync('ffmpeg', ['-v', 'error', '-y', '-i', temporary, '-af', filter, '-ac', '1', '-ar', '24000', '-c:a', 'libvorbis', '-b:a', '64k', destination]);
  console.log(`${row.id}: ${(audio.audio.length / 24000).toFixed(2)}s local Kokoro.js/af_bella`);
}
