#!/usr/bin/env python3
"""Actual Metal 4 common-line PSO/cache and hardware per-sample coverage test."""
from pathlib import Path
import json, os, subprocess, tempfile
here=Path(__file__).resolve().parent
rts=here.parents[1]/'rts'
with tempfile.TemporaryDirectory(prefix='bar-common-line-133-') as d:
    tmp=Path(d); overlay=tmp/'overlay.json'; binary=tmp/'test'
    overlay.write_text(json.dumps({'version':0,'use-external-names':False,'roots':[{
        'type':'file','name':str(rts/'Rendering/Metal4/Metal4RuntimeNative.h'),
        'external-contents':str(here/'NativeCommonLineRuntimeFixture.h')}]}))
    subprocess.run(['/usr/bin/clang++','-std=c++23','-O2','-fobjc-arc','-framework','Foundation','-framework','Metal',
                    '-ivfsoverlay',str(overlay),'-I'+str(rts),str(here/'TestCommonLineSamples133.mm'),'-o',str(binary)],check=True)
    env=os.environ|{'MTL_DEBUG_LAYER':'1','BAR_METAL4_COMMON_LINE_WITNESS':'1'}
    subprocess.run([str(binary)],check=True,env=env)
    subprocess.run([str(binary),'shade'],check=True,env=env)
