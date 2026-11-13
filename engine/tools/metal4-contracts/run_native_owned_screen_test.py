#!/usr/bin/env python3
"""Owned screen execution packet host test; no Metal execution or async claim."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-native-owned-screen-test');out.mkdir(exist_ok=True)
base=['g++','-std=c++23','-I'+str(root/'rts')]
subprocess.run(base+['-Wno-deprecated-declarations','-fsyntax-only',str(root/'rts/Rendering/Backend/NativeUIRecorder.cpp')],check=True)
subprocess.run(base+['-O1','-g','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativeOwnedScreenFrame.cpp')),str(root/'rts/Rendering/Backend/ScreenDrawList.cpp'),'-Wl,--gc-sections','-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
