#!/usr/bin/env python3
"""Actual source/registry CPU test. No GPU execution."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-shadow-cancellation');out.mkdir(exist_ok=True)
backend=root/'rts/Rendering/Backend'
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeShadowCancellation.cpp')),*[str(backend/(name+'.cpp')) for name in ['NativeShadowTextureSource','NativeTextureRegistry','NativeCUSTextureHistory']],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
