#!/usr/bin/env python3
"""Generate the Handler with locally installed Kokoro (no API key/service).
User selected Kokoro on 8 October 2026. Install in an isolated environment:
  python -m pip install kokoro soundfile
Run from repository root: python tools/generate_voice.py
Models/cache stay in ignored build/. FFmpeg exports mono 64 kbps Vorbis.
"""
from pathlib import Path
import argparse
import json
import os
import re
import shutil
import subprocess
ROOT=Path(__file__).resolve().parents[1]
def script_lines():
    text=(ROOT/'docs/05_Design_Docs.md').read_text(encoding='cp1252')
    rows=[]
    for line in text.splitlines():
        match=re.match(r'\| (V\d\d) \| (.*?) \| ([123]) \| "(.*?)" \|',line)
        if match:
            identifier,trigger,priority,spoken=match.groups()
            rows.append(dict(id=identifier,file=f'audio/voice/{identifier}.ogg',text=spoken,priority=int(priority),trigger=trigger))
    if [row['id'] for row in rows]!=[f'V{i:02}' for i in range(1,26)]:raise ValueError('Expected all 25 script lines in order')
    return rows

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--voice',default='af_bella');parser.add_argument('--only');args=parser.parse_args()
    os.environ.setdefault('HF_HOME',str(ROOT/'build/kokoro-models'))
    os.environ.setdefault('TORCH_HOME',str(ROOT/'build/kokoro-models/torch'))
    os.environ.setdefault('XDG_CACHE_HOME',str(ROOT/'build/kokoro-models/cache'))
    from kokoro import KPipeline
    import numpy as np
    import soundfile as sf
    import torch
    torch.set_num_threads(2)
    pipeline=KPipeline(lang_code='a',repo_id='hexgrad/Kokoro-82M',device='cpu')
    rows=script_lines()
    (ROOT/'build/day25-voice-manifest.json').write_text(json.dumps(rows,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    for row in rows:
        if args.only and row['id']!=args.only:continue
        destination=ROOT/'assets'/row['file'];destination.parent.mkdir(parents=True,exist_ok=True)
        if destination.exists():print(row['id']+' already rendered',flush=True);continue
        samples=[]
        for _,_,audio in pipeline(row['text'],voice=args.voice,speed=1.1 if row['priority']==3 else .98):samples.append(audio.numpy())
        if not samples:raise RuntimeError('No speech returned for '+row['id'])
        pcm=np.concatenate(samples)
        if not np.isfinite(pcm).all() or len(pcm)<2400:raise RuntimeError('Invalid speech for '+row['id'])
        temporary=ROOT/'build/audio-generator'/f"{row['id']}.wav";temporary.parent.mkdir(parents=True,exist_ok=True)
        sf.write(temporary,pcm,24000)
        filters='loudnorm=I=-18:TP=-3:LRA=7'
        if row['priority']==3:filters='asetrate=25440,aresample=24000,'+filters
        subprocess.run([shutil.which('ffmpeg') or 'ffmpeg','-v','error','-y','-i',str(temporary),'-af',filters,'-ac','1','-ar','24000','-c:a','libvorbis','-b:a','64k',str(destination)],check=True)
        print(f"{row['id']}: {len(pcm)/24000:.2f}s local Kokoro/{args.voice}",flush=True)
if __name__=='__main__':main()
