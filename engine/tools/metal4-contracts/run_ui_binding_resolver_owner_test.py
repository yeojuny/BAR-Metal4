#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-ui-resolver-owner');out.mkdir(exist_ok=True)
h=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.h').read_text()
assert h.count('void BindUIResolver(')==1 and h.count('const ui::NativeUIBindingResolverOwner& UIResolver()')==1
runtime=h.split('class Runtime {',1)[1].split('class RuntimeLease {',1)[0]
assert 'BindUIResolver(' not in runtime
lease=h.split('class RuntimeLease {',1)[1]
accessors='void BindUIResolver('+lease.split(' void BindUIResolver(',1)[1].split('\tid<MTLDevice> Device()',1)[0]
(out/'lease-ui-resolver.inc').write_text(accessors)
assert 'ui::NativeUIBindingResolverOwner uiResolver;' in lease
subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-I'+str(out),'-I'+str(root/'rts'),str(Path(__file__).with_name('TestUIBindingResolverOwner.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'uiBindingResolver' not in s and 'ResolverScope' not in s
assert s.count('lease.BindUIResolver(std::move(uiResolver))')==2
assert 'DescribeUIBinding(lease,binding)' in s and 'owner->DescribeUIBinding(lease,binding)' in s
assert 'return ContinueScreen(frame,options,presentation,std::move(uiResolver));' in s
main=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert main.index('ContinueScreen(frame,options,presentation,std::move(uiResolver))')<main.index('RuntimeLease lease')
assert main.index('RuntimeLease lease')<main.index('lease.BindUIResolver(')<main.index('postprocessGPU.PrepareFrameImages')
continuation=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::ContinueScreen(',1)[1].split('Metal4WorldRenderer::PrepareIndependentResources(',1)[0]
assert 'ui::NativeUICapturedBindingResolver uiResolver)' in continuation
assert continuation.index('RuntimeLease lease')<continuation.index('lease.BindUIResolver(')<continuation.index('postprocessGPU.PrepareFrameImages')<continuation.index('screenGPU.Prepare(')
print('actual lease accessors, only two lease binds, both provider routes and continuation early-return ownership PASS')
