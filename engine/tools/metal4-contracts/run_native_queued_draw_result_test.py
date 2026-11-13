#!/usr/bin/env python3
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-queued-draw-result');out.mkdir(exist_ok=True)
s=(root/'rts/Game/Game.cpp').read_text().split('nativeResult = rendering::scene::DrawWorld(',1)[1]
assert s.index('nativeResult.Validate();')<s.index('nativeUI.RetireCompletedFrame')
assert 'return nativeResult.AdvanceCPUFrame();' in s
assert 'return nativeResult.presented;' not in s
# No actual producer enables a queued return in this boundary-only checkpoint.
scene=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert '.queued=true' not in scene and '.queued = true' not in scene
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeQueuedDrawResult.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('Game validates before retirement, advances CPU buffers without claiming present; no async producer enabled')
