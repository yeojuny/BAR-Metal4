#!/usr/bin/env python3
"""Actual portable policy and joined worker; native callback ordering is structural only."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='bar-lease-policy-') as d:
 binary=Path(d)/'test'
 subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeLeaseRetirementPolicy.cpp')),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
c=s.split('Metal4WorldRenderer::CompileOriginalCUSProgram(',1)[1].split('Metal4WorldRenderer::CompileOriginalTerrainProgram(',1)[0]
assert c.index('Require([NSThread isMainThread]')<c.index('Scope cacheAccess')<c.index('runtime.CanSafelyDetach()')<c.index('Metal4WindowSurfaceNativeAccess::Layer')<c.index('InvokeNativeRendererControl')<c.index('RuntimeLease lease(RuntimeLease::Retirement::RequireAlreadyRetired,runtime)')<c.index('device==lease.Device()')<c.index('LinkOriginalStages')
assert 'make_shared<const NativeCUSProgramSource>(source)' in c and 'LinkOriginalStages(lease,*captured,' in c
assert '.Begin(' not in c and 'Submit' not in c and 'previousRetired' not in c
r=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
assert 'RuntimeLease({},selectedRuntime,Retirement::DrainOnCaller)' in r
assert 'RuntimeLease(std::move(previousRetired),selectedRuntime,Retirement::DrainOnCaller)' in r
ctor=r.split('RuntimeLease::RuntimeLease(std::function<void()> previousRetired, Runtime& selectedRuntime,Retirement retirement)',1)[1].split('RuntimeLease::~RuntimeLease',1)[0]
assert ctor.index('lock(runtime.requestMutex)')<ctor.index('!runtime.poisoned')<ctor.index('ApplyNativeLeaseRetirementPolicy')<ctor.index('SelectRecordingSlot158')<ctor.index('make_shared<RuntimeFrameResources>')
assert 'bool(runtime.pendingSubmission)||!runtime.pendingSubmissions.empty()' in ctor
assert '[&]{runtime.DrainPendingLocked();}' in ctor
print('Native CUS main drain/device capture/owned request/retired-only worker and old constructor semantics structural PASS; no native execution')
