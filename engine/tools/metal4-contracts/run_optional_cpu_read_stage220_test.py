#!/usr/bin/env python3
"""Exercise actual production source-stage helper with injected failures, not GPU work."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Backend/NativeCUSCPUReadWorker220.h').read_text()
build=s.split(' static void Build(Job& job)noexcept{',1)[1].split(' NativeCUSCPUReadWorker220():',1)[0]
assert 'if(VisitSources(job,[&](const auto& source){scope.Prime(source);}))\n    job.packet=scope.Seal();' in build
assert 'catch(...){job.error=std::current_exception();}' in build
stages=s.split('template<class Prime> static bool VisitSources',1)[1].split(' static void Build',1)[0]
assert stages.index('job.sources')<stages.index('job.AwaitTail()')<stages.index('job.tailSources')<stages.index('try{for(const auto& source:job.optionalSources)')<stages.index('job.optionalFailed=true;return false;')
assert 'job.error' not in stages
assert 'optionalSources=std::move(optional);tailState=TailState::Published;' in s
with tempfile.TemporaryDirectory() as tmp:
 exe=str(Path(tmp)/'optional-stage')
 subprocess.run(['g++','-std=c++23','-O1','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-pthread','-I'+str(root/'rts'),str(Path(__file__).with_name('TestOptionalCPUReadStage220.cpp')),'-o',exe],check=True)
 subprocess.run([exe],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Production Build guard/error boundary checks passed; no native Metal execution')
