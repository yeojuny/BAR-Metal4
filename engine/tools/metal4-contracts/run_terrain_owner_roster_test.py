#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-terrain-owner-roster');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm').read_text()
a=s[s.index('void PreparedTerrainSource::EncodeDependencies('):s.index('void PreparedTerrainSource::EncodeDraw(')]
a+=s[s.index('void PreparedTerrainSource::DidComplete()'):s.index('struct TerrainSourceGPU::Impl')]
(out/'terrain-owner-phases.inc').write_text(a.replace('id<MTL4CommandBuffer>','void*'))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestTerrainOwnerRoster.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
prepare=s.split('std::shared_ptr<PreparedTerrainSource> TerrainSourceGPU::Prepare(',1)[1]
assert prepare.rindex('out.draws.push_back')<prepare.index('out.firstOwners=BuildTerrainOwnerRoster(out.draws)')<prepare.rindex('return prepared;')
assert prepare.count('out.firstOwners=')==1
encode=s.split('void PreparedTerrainSource::EncodeDraw(',1)[1].split('void PreparedTerrainSource::FinishOrderedDraws()',1)[0]
assert 'for(const auto& image:draw.images)if(image.requireReadable)image.requireReadable();' in encode
assert 'InspectSourceDependency' not in a and 'std::set' not in a
assert a.count('impl->firstOwners')==2
print('Terrain exact first-owner roster: 2000 randomized reference-equivalence cases, null/first-empty semantics, live callbacks, failure quarantine, lifetime and all allocation boundaries PASS; actual phase code extracted')
