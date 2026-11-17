from pathlib import Path
import subprocess, os
r=Path(__file__).resolve().parents[1];s=(r/'candidate/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text();b=(r/'base/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
helper=s.split('// Opt-in correctness evidence only.')[1].split('// BEGIN WORLD_TEXTURE_PREPARATION_JOURNAL')[0]
helper=helper[helper.index('static void TraceTextureOrdered'):]
(r/'tests/wrapper.h').write_text(helper)
assert 'RequireCompleted(producer);TraceTextureOrdered' in s
assert 'Require(admitted,"Texture borrow lost independent pending-producer ordering");\n    TraceTextureOrdered' in s
assert 'Require(cached->status->Readable(submission),"Borrowed texture lost its exact encoding permit");TraceTextureOrdered' in s
# Only listed diagnostic replacements differ. All other source must be exact.
t=s.replace('#include "Rendering/Backend/NativeTextureOrderedTrace.h"\n','')
a=t.index('// Opt-in correctness evidence only.');z=t.index('// BEGIN WORLD_TEXTURE_PREPARATION_JOURNAL');t=t[:a]+t[z:]
t=t.replace('\n  TraceTextureOrdered(NativeTextureOrderedTraceEvent::Armed);','').replace('[entry,producer,consumer]{entry.status->RequireCompleted(producer);TraceTextureOrdered(NativeTextureOrderedTraceEvent::ProducerRetiredCheck,producer,consumer);}','[entry,producer]{entry.status->RequireCompleted(producer);}').replace('\n    TraceTextureOrdered(NativeTextureOrderedTraceEvent::Admitted,producer,consumer);','').replace('TraceTextureOrdered(NativeTextureOrderedTraceEvent::EncodePermitCheck,cached->borrow->producer,submission);','')
assert t==b
for live in [False,True]:
 exe=r/'tests'/('live' if live else 'nonlive')
 cmd=['c++','-std=c++20','-Wall','-Wextra','-Werror','-pthread','-I'+str(r/'candidate/engine/rts/Rendering/Backend'),'-I'+str(r/'tests')]
 if live:cmd+=['-DRECOIL_METAL4_LIVE_SCENE']
 subprocess.run(cmd+[str(r/'tests/test.cpp'),'-o',str(exe)],check=True)
 for flag in ['','0','1','true']:
  env=os.environ.copy();env['BAR_NATIVE_TEXTURE_ORDERED_TRACE']=flag
  subprocess.run([str(exe),'1' if live and flag=='1' else '0'],env=env,check=True)
print('production budget/wrapper concurrency, exception, flag-off/nonlive and exact source wiring PASS')
