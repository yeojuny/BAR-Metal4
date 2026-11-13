#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-cus-owner-roster');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4CUSSource.mm').read_text()
a=s[s.index('void PreparedCUSSource::EncodeDependencies('):s.index('void PreparedCUSSource::EncodeDraw(')]
a+=s[s.index('void PreparedCUSSource::DidComplete()'):s.index('struct CUSSourceGPU::Impl')]
a+=s[s.index('void PreparedCUSSource::PreparePassInspection160()'):s.index('ModelPassDependencyReport160 PreparedCUSSource::InspectDependencies160')]
(out/'cus-owner-phases.inc').write_text(a.replace('id<MTL4CommandBuffer>','void*').replace('__bridge ',''))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestCUSOwnerRoster.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
assert s.rindex('out->impl->draws.push_back')<s.index('out->impl->firstOwners=BuildTerrainOwnerRoster')<s.rindex('return out;')
assert s.count('out->impl->firstOwners=')==1
assert a.count('impl->firstOwners')==4
assert 'for(const auto& image:d.images)if(image.requireReadable)image.requireReadable();' in s
assert 'InspectSourceDependency' not in a
print('CUS actual three-phase roster: 2000 randomized reference/inspection, null/alias/live callbacks, retry/retirement/lifetime/allocation faults PASS')

# R5: mandatory census belongs to prepare time, before diagnostic reporting.
assert s.index("out->impl->firstOwners=BuildTerrainOwnerRoster") < s.index('if(sampled)TraceRecord("perf f=')
