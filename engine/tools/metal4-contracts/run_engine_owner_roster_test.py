#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-engine-owner-roster');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4EngineModelForward.mm').read_text()
a=s[s.index('void PreparedEngineModelForward::EncodeDependencies('):s.index('void PreparedEngineModelForward::EncodeOrderedDraw(')]
a+=s[s.index('void PreparedEngineModelForward::DidComplete()'):s.index('struct EngineModelForwardGPU::Impl')]
a+=s[s.index('void PreparedEngineModelForward::PreparePassInspection160()'):s.index('ModelPassDependencyReport160 PreparedEngineModelForward::InspectDependencies160')]
(out/'engine-owner-phases.inc').write_text(a.replace('id<MTL4CommandBuffer>','void*').replace('__bridge ',''))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestEngineOwnerRoster.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
assert s.rindex('p.draws.push_back')<s.index('p.firstOwners=BuildTerrainOwnerRoster')<s.rindex('return out;')
assert s.count('p.firstOwners=')==1
assert a.count('impl->firstOwners')==4
assert 'for(const auto& image:d.images)if(image.requireReadable)image.requireReadable();' in s
assert 'InspectSourceDependency' not in a
print('Engine non-null actual three-phase roster: 2000 randomized reference/inspection, null/alias/live callbacks, retry/retirement/lifetime/allocation faults PASS')
