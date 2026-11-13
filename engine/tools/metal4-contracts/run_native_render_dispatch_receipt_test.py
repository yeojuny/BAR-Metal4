#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-render-dispatch-receipt');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text().split('Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldRenderer::DrawPrepared(',1)[0]
worker=s.split('impl->cpuJob->Submit(worker,',1)[1].split('FinishCPUWork();',1)[0]
capture=worker.split(']() mutable',1)[0]
assert '&' not in capture
assert 'dispatchReceipt,deferredReceipt=std::move(deferredReceipt)' in capture
assert '[dispatchReceipt](std::uint64_t submission)' in worker
assert s.index('make_shared<NativeRenderDispatchReceipt>')<s.index('dispatchReceipt->Reserve')<s.index('cacheAccess.reset(); // Reservation')<s.index('impl->cpuJob->Submit(worker,')
settlement=(root/'rts/Rendering/Backend/NativeRenderJobSettlement.h').read_text().split(' void Finish(){',1)[1]
assert settlement.index('task->Join()')<settlement.index('task.reset();receipt->ReleaseHandoff();')<settlement.index('try{target();}')<settlement.index('receipt->DeliverRetirement();')
assert 'FinishCPUWork();\n  auto result=impl->cpuJob->Consume();' in s
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeRenderDispatchReceipt.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=40,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Owned worker wiring checked structurally; Begin-ready pilot uses same exact receipts; no native execution')
