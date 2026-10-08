#!/usr/bin/env python3
"""Render original Ghost Protocol music with stdlib synthesis and installed FFmpeg.
Run from the repository root: python tools/generate_music.py
No samples or third-party compositions are used. Outputs are project-owned.
"""
from array import array
from pathlib import Path
import math
import random
import shutil
import subprocess
import wave
SR=44100
ROOT=Path(__file__).resolve().parents[1]
TAU=math.tau

def note(midi): return 440*2**((midi-69)/12)
def render(name,bpm,bars,kind):
    seconds=3 if kind=='sting' else 60/bpm*bars*4
    n=round(seconds*SR)
    left=array('f',[0])*n;right=array('f',[0])*n
    rng=random.Random(2508+len(name))
    def tone(start,duration,midi,amp,pan=0,brass=False):
        first=round(start*SR); count=min(round(duration*SR),n-first)
        if count<=0:return
        freq=note(midi)
        for j in range(count):
            t=j/SR; env=min(1,t/.015)*max(0,1-t/duration)**(1.5 if brass else 2)
            phase=TAU*freq*t
            value=(math.sin(phase)+(.3 if brass else .12)*math.sin(phase*2)+(.18 if brass else .06)*math.sin(phase*3))*amp*env
            left[first+j]+=value*(1-pan*.4);right[first+j]+=value*(1+pan*.4)
    def drum(start,kick=False,amp=.12):
        first=round(start*SR);count=min(round(.17*SR),n-first)
        for j in range(max(0,count)):
            t=j/SR
            value=(math.sin(TAU*(72*t-110*t*t)) if kick else rng.uniform(-1,1))*math.exp(-t*(28 if kick else 60))*amp
            left[first+j]+=value;right[first+j]+=value
    if kind=='sting':
        for start,midi,duration in [(0,64,.26),(.3,67,.26),(.6,71,.26),(.95,76,.9),(1.95,59,.8)]:
            for interval in (0,4,7):tone(start,duration,midi+interval,.09,(interval-3)/7,True)
            drum(start,True,.14)
    else:
        beat=60/bpm
        roots=[40,36,45,47]
        for bar in range(bars):
            root=roots[(bar//2)%4]
            for interval in (12,19,22):tone(bar*beat*4,beat*3.8,root+interval,.035,(interval-17)/8)
            for b in range(4):
                start=(bar*4+b)*beat
                tone(start,beat*.65,root,.14 if kind=='loud' else .10)
                drum(start,True,.17 if kind=='loud' else .075)
                if kind=='loud':
                    if b in (1,3):drum(start,False,.15)
                    for off in (0,.5):drum(start+off*beat,False,.045)
                    tone(start+beat*.5,beat*.32,root+12+(7 if b%2 else 0),.055,.5 if b%2 else -.5)
                elif b==2:
                    tone(start,beat*.9,root+36+(7 if bar%2 else 3),.06,.55)
                elif kind=='menu' and b==3:
                    tone(start,beat*.7,root+31,.055,-.5)
    peak=max(max(abs(v) for v in left),max(abs(v) for v in right),.001)
    pcm=array('h')
    for i in range(n):
        edge=min(1,i/(SR*.008),(n-1-i)/(SR*.008))
        for channel in (left,right):pcm.append(round(channel[i]/peak*.42*32767*edge))
    temp=ROOT/'build/audio-generator'/f'{name}.wav';temp.parent.mkdir(parents=True,exist_ok=True)
    with wave.open(str(temp),'wb') as f:f.setparams((2,2,SR,n,'NONE','not compressed'));f.writeframes(pcm.tobytes())
    output=ROOT/'assets/audio/music'/f'{name}.ogg';output.parent.mkdir(parents=True,exist_ok=True)
    subprocess.run([shutil.which('ffmpeg') or 'ffmpeg','-v','error','-y','-i',str(temp),'-c:a','libvorbis','-q:a','4',str(output)],check=True)
    print(f'{name}: {seconds:.3f}s stereo',flush=True)
if __name__=='__main__':
    render('stealth_loop',80,8,'stealth');render('loud_loop',140,8,'loud');render('menu_loop',90,8,'menu');render('payout_sting',100,0,'sting')
