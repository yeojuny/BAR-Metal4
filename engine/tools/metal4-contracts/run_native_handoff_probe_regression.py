#!/usr/bin/env python3
"""Production-header stress. No game/Metal execution; bounded host process."""
import os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='bar-handoff-probe-') as tmp:
    exe=Path(tmp)/'probe'
    subprocess.run([os.environ.get('CXX','c++'),'-std=c++23','-O1','-g','-pthread',
        '-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeHandoffProbeRegression.cpp')),
        '-o',str(exe)],check=True,timeout=60)
    subprocess.run([str(exe)],check=True,timeout=60,
        env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
