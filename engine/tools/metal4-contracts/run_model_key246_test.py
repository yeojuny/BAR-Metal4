#!/usr/bin/env python3
"""Standalone current ModelKey246 contract equivalence test."""
from pathlib import Path
import subprocess,tempfile
here=Path(__file__).resolve().parent
rts=here.parents[1]/'rts'
with tempfile.TemporaryDirectory(prefix='bar-modelkey246-') as d:
 binary=Path(d)/'proof'
 subprocess.run(['/usr/bin/clang++','-std=c++23','-O2','-UNDEBUG','-fobjc-arc','-I'+str(rts),str(here/'TestModelKey246.mm'),'-framework','Foundation','-framework','Metal','-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
