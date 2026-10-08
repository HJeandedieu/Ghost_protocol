// Offline generator only. Download sfxr.mjs, sfxr.js and riffwave.js from
// https://github.com/chr15m/jsfxr at b7b6aa27d62f8f356db267276bf54b451555c49d
// into build/audio-generator/. The generator is Unlicense; it is not a runtime dependency.
// Run: node tools/generate_sfx.mjs (FFmpeg must be available).
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import {sfxr} from '../build/audio-generator/sfxr.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const text=fs.readFileSync(path.join(root,'docs/04_Data_Formats.md'),'latin1');
const list=text.slice(text.indexOf('## 7. SFX manifest'),text.indexOf('## 8. The map'));
const names=[...list.matchAll(/`([a-z_]+)`/g)].map(x=>x[1]);
let seed=2508;
Math.random=()=>{seed=(1664525*seed+1013904223)>>>0;return seed/4294967296;};
fs.mkdirSync(path.join(root,'assets/audio/sfx'),{recursive:true});
for(const name of names){
 let preset=name.startsWith('shot_')?'laserShoot':/hit_|down|dye_burst/.test(name)?'hitHurt':/pick|ping_ready/.test(name)?'pickupCoin':/ping_|gate|vault|van/.test(name)?'powerUp':'blipSelect';
 const p=sfxr.generate(preset);p.sample_rate=44100;p.sample_size=16;p.sound_vol=.18;
 if(name.startsWith('step_')){p.wave_type=3;p.p_base_freq=.14;p.p_lpf_freq=.24;p.p_env_attack=0;p.p_env_sustain=name==='step_sprint'?.08:name==='step_crouch'?.28:.18;p.p_env_decay=.14;}
 if(name==='shot_pistol_supp'){p.wave_type=3;p.p_env_sustain=.03;p.p_env_decay=.09;p.p_lpf_freq=.3;}
 if(name==='shot_smg'){p.wave_type=3;p.p_env_sustain=.04;p.p_env_decay=.10;p.p_lpf_freq=.55;}
 if(name==='shot_shotgun'){p.wave_type=3;p.p_env_sustain=.08;p.p_env_decay=.24;p.p_lpf_freq=.45;}
 if(/loop|drill/.test(name)){p.wave_type=3;p.p_env_sustain=.42;p.p_env_decay=.18;p.p_lpf_freq=.18;p.p_repeat_speed=.7;}
 if(name==='alarm_siren'){p.wave_type=2;p.p_base_freq=.32;p.p_env_attack=.06;p.p_env_sustain=1;p.p_env_decay=.25;p.p_vib_strength=.65;p.p_vib_speed=.32;}
 if(name==='payout_stamp'){p.wave_type=3;p.p_env_sustain=.06;p.p_env_decay=.18;p.p_lpf_freq=.28;}
 const wav=path.join(root,'build/audio-generator',name+'.wav');
 fs.writeFileSync(wav,Buffer.from(sfxr.toWave(p).dataURI.split(',')[1],'base64'));
 const result=spawnSync('ffmpeg',['-v','error','-y','-i',wav,'-ac','1','-ar','44100','-c:a','libvorbis','-q:a','3',path.join(root,'assets/audio/sfx',name+'.ogg')],{encoding:'utf8'});
 if(result.status!==0)throw Error(result.stderr||'FFmpeg failed');
}
console.log(`Generated ${names.length} mono 44.1kHz SFX`);
