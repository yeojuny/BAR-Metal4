#!/usr/bin/env python3
import subprocess, os
from pathlib import Path
root=Path(__file__).resolve().parents[2]
repo=root.parent
out=Path('/tmp/bar-surface-event-test');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-g','-O1','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(repo.parent/'bar-validation-sdl3/include'),str(Path(__file__).with_name('TestMetal4SurfaceEventPolicy.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
e=s.split('void HandleEvent(',1)[1].split('ui::NativeUIFrameRasterMapping',1)[0]
assert 'ApplyDrainedMetal4SurfaceEvent' in e and 'RequireRetiredGPUWork();' in e
for name in ['void PrepareWorld(', 'DrawResult DrawWorld(']:
 block=s.split(name,1)[1]
 pos=block.index('session->surface.Refresh();')
 assert 'if(BeginReadyPilotRequested())RequireRetiredGPUWork();' in block[max(0,pos-360):pos]
w=(root/'rts/Rendering/Metal4/Metal4WindowSurface.mm').read_text()
assert 'ClassifyMetal4SurfaceEvent(event,impl->state.Snapshot().windowID)' in w
r=(root/'rts/Rendering/GlobalRendering.cpp').read_text().split('void CGlobalRendering::UpdateWindow()',1)[1].split('SetWindowAttributes(sdlWindow);',1)[0]
assert r.index('if (winChgFrame != drawFrame)')<r.rindex('rendering::scene::FinishPendingFrame();')
assert r.index('if (sdlWindow == nullptr)')<r.rindex('rendering::scene::FinishPendingFrame();')
app=(root/'rts/System/SpringApp.cpp').read_text()
assert 'if (!closeRequested && rendering::scene::IsInitialized())' in app
assert 'if (gu->globalQuit && rendering::scene::IsInitialized()) rendering::scene::HandleEvent(event);' in app
print('Surface events: 15000 relevant cases, drain failure/noop/topology + actual production wiring PASS')

window=app.split('case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {',1)[1]
assert window.index('event.window.windowID!=SDL_GetWindowID(globalRendering->GetWindow())')<window.index('switch (event.type)')
print('Foreign-window engine geometry and quit filtering PASS')

g=(root/'rts/Rendering/GlobalRendering.cpp').read_text()
notify=g.split('void CGlobalRendering::ConfigNotify(',1)[1].split('void CGlobalRendering::UpdateWindow()',1)[0]
dual=notify.split('if (key == "DualScreenMode"',1)[1].split('#endif',1)[0]
assert 'nativeDualScreenPending.store(true' in dual and 'return;' in dual
assert 'FinishPendingFrame' not in dual and 'SetDualScreenParams' not in dual
pending=r.split('if(sdlWindow && nativeDualScreenPending.load',1)[1].split('#endif',1)[0]
assert pending.index('FinishPendingFrame();')<pending.index('nativeDualScreenPending.exchange')<pending.index('SetDualScreenParams();')<pending.index('UpdateGLGeometry();')
assert 'if(game!=nullptr)gmeChgFrame=drawFrame;' in pending
print('Dual-screen config deferred out of mutex, drain precedes geometry PASS')
