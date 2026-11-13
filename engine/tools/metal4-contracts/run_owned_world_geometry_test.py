#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='bar-owned-world-') as path:
 exe=Path(path)/'test'
 subprocess.run(['g++','-std=c++23','-O1','-g','-pthread','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestOwnedWorldGeometry.cpp')),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Actual NativeOwnedWorldGeometry header with mock raster builder: alias lifetime, allocation-free constructor, exact source/bitwise target, once-only state, narrow owner exclusions/seal failures, error identity, joined external execution PASS. Not real geometry/native concurrency proof.')
