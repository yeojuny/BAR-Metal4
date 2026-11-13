#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='bar-geometry-prefix-') as path:
 exe=Path(path)/'test'
 subprocess.run(['g++','-std=c++23','-O1','-g','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestWorldGeometryPrefix.cpp')),str(root/'rts/Rendering/Backend/NativeWorldDrawGeometry.cpp'),'-Wl,--gc-sections','-o',str(exe)],check=True)
 subprocess.run([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Actual geometry prefix CPP: source-first/extent/viewport/plane error ordering and failed seal rejection PASS; command validator is an explicit mock.')
