#!/usr/bin/env python3
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
for field in ('worker_begin_ns','worker_end_ns','worker_thread_cpu_ns','worker_thread_cpu_valid'):
 assert field in s, f'Missing CSV timing column: {field}'
worker=(root/'rts/Rendering/Backend/NativeCUSCPUReadWorker220.h').read_text()
assert 'ReadNativeThreadCPUTime' in worker
sources=['NativeCUSRenderInputs.cpp','NativeCUSSourceData.cpp','NativeCUSModelInputs.cpp','NativeCUSPublishedData.cpp','NativeCUSUniformData.cpp','NativeCUSTextureHistory.cpp']
with tempfile.TemporaryDirectory(prefix='bar-worker-timing220-') as directory:
 target=Path(directory)/'test'
 trace=s[s.index('void TraceCPUReadPacket220('):s.index('struct ScenePendingAdmission;')]
 fixture=Path(directory)/'trace.cpp'
 fixture.write_text('#include "Rendering/Backend/NativeCUSCPUReadWorker220.h"\n#include <cstdio>\n#include <cstdlib>\nusing namespace rendering;\nbool CPUReadPacketMeasure220(); int CPUReadPacketMode220();\nstruct CPUReadReceipt220 { NativeCUSCPUReadWorker220::Job job; uint64_t ordinal=0,startupNs=0,mainBeginNs=0,mainEndNs=0; };\n'+trace)
 subprocess.run(['g++','-std=c++23','-Werror','-Wformat=2','-fsyntax-only','-I'+str(root/'rts'),str(fixture)],check=True)
 subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-Werror','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),str(Path(__file__).with_name('TestCPUReadWorkerTiming220.cpp')),*[str(root/'rts/Rendering/Backend'/f) for f in sources],'-o',str(target)],check=True)
 subprocess.run([str(target)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Actual worker/header + real CPU-read dependencies; source-only, no app build/Metal execution')
