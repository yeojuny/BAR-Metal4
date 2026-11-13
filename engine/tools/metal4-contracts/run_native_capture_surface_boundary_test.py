#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-capture-surface-boundary');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeCaptureSurfaceBoundary.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
b=s.split('void PrepareWorld(',1)[1].split('void AbortPreparedWorld()',1)[0]
prefix=b.split('auto& frame=*session->preparedFrame;',1)[0]
assert prefix.count('CaptureAtNativeSurfaceBoundary(')==1
assert prefix.index('capture.Capture(')<prefix.index('CompleteDeferredMainParticles(')
assert prefix.index('CaptureRefreshUnchanged()')<prefix.index('RequireRetiredGPUWork();')<prefix.index('surface.Refresh();')<prefix.index('capture.Capture(')
assert 'CaptureWater' not in prefix and 'CaptureOriginalAlphaModels' not in prefix
assert 'NativeWaterScenePass::Main' in prefix
# The other refresh/control boundary is unchanged.
draw=s.split('DrawResult DrawWorld(',1)[1]
pos=draw.index('session->surface.Refresh();')
assert 'if(BeginReadyPilotRequested())RequireRetiredGPUWork();' in draw[max(0,pos-360):pos]
w=(root/'rts/Rendering/Metal4/Metal4WindowSurface.mm').read_text()
probe=w.split('bool CaptureRefreshUnchanged() const',1)[1].split('bool Refresh()',1)[0]
assert 'layer.drawableSize' in probe and 'layer.displaySyncEnabled' in probe and 'requested==displaySyncRequest' in probe
assert 'state.Observe' not in probe and 'ApplyRenderSize' not in probe and 'ApplyDisplaySync' not in probe
observe=w.split('Metal4SurfaceObservation Observe() const',1)[1].split('bool CaptureRefreshUnchanged()',1)[0]
assert 'SDL_GetWindowSize' in observe and 'convertRectToBacking' in observe and 'SDL_GetWindowFlags' in observe
assert 'state.Observe' not in observe and 'layer.' not in observe
# Actual particle producer detaches geometry into the fresh frame; no scratch alias.
p=(root/'rts/Rendering/Backend/NativeWorldParticleCapture.cpp').read_text()
assert 'auto frame=std::make_shared<NativeWorldParticleFrame>();' in p
assert 'AppendNativeParticleGeometry(frame,vertices,geometry.GetIndcs(),range);' in p
print('Capture surface: 1000 exact observations, all changed/unknown states, drain/capture failures and reobservation PASS; production scope limited to Capture(Main)')

c=(root/'rts/Rendering/WorldFrameCapture.cpp').read_text()
assert 'if(deferMainParticles)result.deferredMainParticles.Defer();\n else result.particleFrame =' in c
completion=c.split('void WorldFrameCapture::CompleteDeferredMainParticles(',1)[1].split('std::shared_ptr<const WorldModel>',1)[0]
assert 'frame.deferredMainParticles.Complete(' in completion
assert completion.index('sourceDrawFrame')<completion.index('state->particleCapture.Capture(')
r=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text().split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1]
assert r.index('frame.deferredMainParticles.RequireComplete();')<r.index('FinishCPUWork();')
h=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.h').read_text()
assert 'this->frame->deferredMainParticles.RequireComplete();' in h
print('Deferred particle writer: missing/repeated/reentrant/failed completion rejected; particle source runs only after boundary drain; submission rejects partial frame')
