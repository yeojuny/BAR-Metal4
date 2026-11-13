#!/usr/bin/env python3
"""Actual control/gate/worker CPU protocol, structural Metal/Lua allocation wiring."""
import os,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-renderer-allocation-control');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeRendererAllocationControl.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=30)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
allocation=s.split('Metal4WorldRenderer::AllocateCoverageTarget(',1)[1].split('Metal4WorldRenderer::AllocateBlockoutTarget',1)[0]
assert allocation.index('Require([NSThread isMainThread]')<allocation.index('(void)SharedRuntime()')<allocation.index('InvokeNativeRendererControl')
assert 'RuntimeLease' not in allocation and 'RuntimeCleanupLease' not in allocation
assert allocation.count('ReserveAttachmentAllocationForControl(')==2
assert 'return std::make_shared<Owner>' in allocation
screen=(root/'rts/Rendering/Metal4/Metal4ScreenNative.mm').read_text().split('ScreenGPU::ReserveAttachmentAllocationForControl(',1)[1].split('PreparedScreenImage ScreenGPU::PrepareImage',1)[0]
assert 'owner.Acquired()&&![NSThread isMainThread]' in screen
assert 'RuntimeLease' not in screen and 'AllocateScreenInitialStorage(runtime.Device(),source)' in screen
lua=(root/'rts/Lua/LuaNativeUI.cpp').read_text().split('int CreateCoverageFBO(',1)[1].split('int ActiveCoverageFBO',1)[0]
assert lua.index('auto storage=coverageTargetAllocator(')<lua.index('if(!storage)')<lua.index('lua_newuserdata')
print('Synchronous real allocation before Lua userdata/no RuntimeLease/worker-only cache structural PASS; no GPU execution')
