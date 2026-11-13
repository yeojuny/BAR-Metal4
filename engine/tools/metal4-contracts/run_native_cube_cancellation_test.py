#!/usr/bin/env python3
"""Actual cube source + registry/history CPU test; not GPU/runtime validation."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-native-cube-cancellation');out.mkdir(exist_ok=True)
backend=root/'rts/Rendering/Backend'
subprocess.run(['g++','-std=c++23','-pthread','-I'+str(root/'rts'),'-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativeCubeCancellation.cpp')),*[str(backend/(name+'.cpp')) for name in ['NativeCubeTextureSource','NativeTextureRegistry','NativeCUSTextureHistory']],'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
