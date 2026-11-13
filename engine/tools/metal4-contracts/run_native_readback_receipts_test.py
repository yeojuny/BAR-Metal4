#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-readback-receipts');out.mkdir(exist_ok=True)
capture=(root/'rts/Rendering/Backend/NativeUIRecorder.cpp').read_text().split('NativeUIReadbackPrefix NativeUIRecorder::CaptureReadbackPrefix()',1)[1].split('void NativeUIRecorder::AbortFrame',1)[0]
assert capture.index('prefix.targetReceipts=impl->targetJournal.Receipts()')<capture.index('if(!impl->targets.empty())')
assert capture.index('prefix.targetReceipts.push_back')<capture.index('impl->targetJournal.Append')<capture.index('CommitTargetPair')
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert '!prefix.TargetActualSubmission(*found)' in renderer
assert 'found->actualSubmission' not in renderer
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeReadbackReceipts.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
