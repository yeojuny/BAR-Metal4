#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-owned-render-task');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'dispatchReceipt->MarkBegun(submission);dispatchReceipt->PublishBeginReady();' in s
assert 'receipt->MarkWorkerFinished();' in s
b=s.split('id<MTL4CommandBuffer> command = lease.Begin("Actual engine',1)[1]
assert b.index('options.beginReceipt->MarkBegun')<b.index('impl->activeSourceSubmission=sourceSubmission')<b.index('targetSubmissionBegan(sourceSubmission)')
worker=(root/'rts/Rendering/Backend/NativeSerialRenderWorker.h').read_text()
assert 'auto ticket=Enqueue(std::forward<F>(function));return ticket->Join();' in worker
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeOwnedRenderTask.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Production Invoke still joined; separate renderer pilot must explicitly release after Begin; no native execution')
