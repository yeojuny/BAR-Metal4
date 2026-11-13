#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-source-frame-credits');out.mkdir(exist_ok=True)
game=(root/'rts/Game/Game.cpp').read_text()
assert game.index('AcquireWorldFrameCredit(globalRendering->drawFrame)')<game.index('nativeUI.BeginFrame')
assert 'std::move(luaSnowFrame),nativeFrameCredit)' in game
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
retire=scene.split('void ApplyDeferredFrameReceipts()',1)[1].split('void RequireRetiredGPUWork()',1)[0]
assert retire.index('sourceCredits.ValidateHeld')<retire.index('ValidateHeldFrame')<retire.index('RetireCompletedFrame')<retire.index('sourceCredits.ReleaseHeld')
record=scene.split('struct ScenePendingAdmission {',1)[1].split('void CompleteDynamicCapture',1)[0]
assert record.index('sourceCredits.Hold')<record.index('source->Take')<record.index('TransferValidated')
accept=record.split(' void Accept(',1)[1]
assert 'sourceCredits.ReleaseHeld' not in accept
subprocess.run(['g++','-std=c++23','-O1','-g','-pthread','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeSourceFrameCredits.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Game acquires before capture; exact admission retains through GPU+main ledger retirement; Accept does not release credit')
