#!/usr/bin/env python3
"""Verify actual viewport mapping and hardware depth/coverage across resolutions."""
from pathlib import Path
import os, subprocess, tempfile
here=Path(__file__).resolve().parent
metal=here.parents[1]/'rts/Rendering/Metal4'
env=os.environ|{'MTL_DEBUG_LAYER':'1','BAR_NATIVE_RANGE_DIVISION233':'1','ASAN_OPTIONS':'detect_leaks=0'}
with tempfile.TemporaryDirectory(prefix='bar-native-resolution-') as d:
    for file in ('TestNativeResolutionMapping.cpp','TestGroundDecalPhysicalDepth.mm','TestWeaponRangeRebasedViewport.mm'):
        path=here/file; binary=Path(d)/path.stem
        cmd=['/usr/bin/clang++','-std=c++23','-I'+str(metal),str(path),'-o',str(binary)]
        if path.suffix=='.mm':
            cmd+=['-O2','-fobjc-arc','-framework','Foundation','-framework','Metal']
        else:
            cmd+=['-O1','-g','-fsanitize=address,undefined']
        subprocess.run(cmd,check=True)
        subprocess.run([str(binary)],check=True,env=env)
