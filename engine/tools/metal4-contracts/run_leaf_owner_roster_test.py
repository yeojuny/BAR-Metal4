#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[2]
for name in ('Grass','ResourceSpots'):
 s=(root/f'rts/Rendering/Metal4/Metal4{name}.mm').read_text()
 a=s[s.index(f'void Prepared{name}::EncodeDependencies('):s.index(f'void Prepared{name}::EncodeDraw(')]
 a+=s[s.index(f'void Prepared{name}::DidComplete()'):s.index(f'struct {name}GPU::Impl')]
 assert 'std::set' not in a and a.count('impl->firstOwners')==2
 assert s.rindex('draws.push_back')<s.index('out->impl->firstOwners=BuildTerrainOwnerRoster')<s.rindex('return out;')
 assert s.count('firstOwners=')==1
 assert 'for(const auto& image:d.images)image.requireReadable();' in s
 with tempfile.TemporaryDirectory(prefix='bar-leaf-roster-') as path:
  out=Path(path)
  (out/'leaf-owner-phases.inc').write_text(a.replace('id<MTL4CommandBuffer>','void*').replace('Prepared'+name,'PreparedLeaf'))
  subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestLeafOwnerRoster.cpp')),'-o',str(out/'test')],check=True)
  subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
 print(name+': actual lifecycle extraction; 2000 reference cycles, null/alias/empty callbacks, live callbacks, retries, lifetime and allocation faults PASS')
print('Source-only; no app build or Metal execution')
