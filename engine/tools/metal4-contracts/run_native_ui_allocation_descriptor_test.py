#!/usr/bin/env python3
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-ui-allocation');out.mkdir(exist_ok=True)
b=root/'rts/Rendering/Backend'
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DRECOIL_USE_SDL3','-I'+str(root/'rts'),'-I'+str(root/'rts/lib'),'-I'+str(root/'rts/lib/tracy/public'),'-I'+str(root/'rts/lib/nowide/include'),'-I'+str(root/'rts/lib/lua/include'),'-I'+str(root/'rts/System/SDL3Compat'),'-I'+str(root.parent.parent/'bar-validation-sdl3/include'),str(Path(__file__).with_name('TestNativeUIAllocationDescriptor.cpp')),str(b/'NativeTextureRegistry.cpp'),str(b/'NativeCUSTextureHistory.cpp'),str(b/'NativeUITextureHistory.cpp'),str(b/'NativeMapTexturePublication.cpp'),str(b/'NativeUIRecorder.cpp'),str(b/'NativePostprocessData.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(b/'NativeUITextureHistory.cpp').read_text()
create=s.split('void NativeUITextureHistory::Create(',1)[1].split('void NativeUITextureHistory::',1)[0]
assert create.index('registry.Allocate')<create.index('PublishNativeUIAllocationDescriptor')<create.index('e.declared=registry.Find')
assert 'e.image=' not in create
resolver=s.split('NativeUITextureHistory::DescribeBinding(',1)[1].split('bool NativeUITextureHistory::Enabled',1)[0]
assert 'bindings' not in resolver and 'e.binding->current' not in resolver
print('Initial descriptor publication and no-latest-name resolver structural PASS; live context/provider still remains')
retained=s.split('NativeUITextureHistory::DescribeRetainedBinding(',1)[1].split('bool NativeUITextureHistory::Enabled',1)[0]
assert 'CheckContext' not in retained and 'registry' not in retained and 'bindings' not in retained
capture=s.split('NativeUITextureHistory::CaptureResolver()',1)[1].split('NativeUITextureHistory::DescribeBinding',1)[0]
assert '[provider=externalImages]' in capture and 'DescribeRetainedBinding(binding,provider)' in capture
world=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
dispatch=world.split('Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldRenderer::DrawPrepared(',1)[0]
assert dispatch.index('uiContext.UI().CaptureResolver()')<dispatch.index('cacheAccess.reset()')
assert 'uiResolver=std::move(uiResolver)' in dispatch
assert 'const auto source=DescribeUIBinding(lease,binding)' in world and 'const auto source=owner->DescribeUIBinding(lease,binding)' in world
assert world.count('OriginalNativeTextureContext().UI().DescribeBinding(binding)')==1
helper=world.split('ObservedImage DescribeUIBinding(',1)[1].split('}',1)[0]
assert helper.index('lease.UIResolver().Describe')<helper.index('isMainThread')<helper.index('OriginalNativeTextureContext')
assert 'ResolverScope' not in world and 'uiBindingResolver' not in world
assert world.count('lease.BindUIResolver(std::move(uiResolver))')==2
assert 'return ContinueScreen(frame,options,presentation,std::move(uiResolver));' in world
owner=(b/'NativeUIBindingResolverOwner.h').read_text()
assert owner.index('if(resolver)return resolver(binding)')<owner.index('if(!mainThread)')<owner.index('std::forward<Fallback>(fallback)()')
print('Captured resolver exact lease ownership, main-only fallback and continuation handoff structural PASS; no native execution')
