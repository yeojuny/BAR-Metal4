#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile,hashlib
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4EngineModelDeferred.mm').read_text()
def between(a,b):return s[s.index(a):s.index(b)]
a=between('void PreparedEngineModelDeferred::EncodeDependencies(', 'void PreparedEngineModelDeferred::EncodeBeforeLua(')
a+=between('void PreparedEngineModelDeferred::DidComplete()', 'struct EngineModelDeferredGPU::Impl')
a+=between('size_t PreparedEngineModelDeferred::DrawCount()', 'uint64_t PreparedEngineModelDeferred::SequenceAt(')
a+=between('void PreparedEngineModelDeferred::FinishOrderedDraws()', 'void PreparedEngineModelDeferred::DidComplete()')
# Grouped/ordered encode and finish behavior is byte-identical to audited source.
assert hashlib.sha256(between('void PreparedEngineModelDeferred::EncodeBeforeLua(','void PreparedEngineModelDeferred::DidComplete()').encode()).hexdigest()=='70da41097cfabefde24f5acd5fbb02d00d42862cff66a064ec1416f966cc8c2f'
assert 'std::set' not in a and a.count('impl->firstOwners')==2
assert s.index('prepareList(p.source->afterLua,p.after)')<s.index('uniformArena.Report("EngineDeferred"')<s.index('p.firstOwners=BuildGroupedOwnerRoster')<s.rindex('return out;')
assert s.count('firstOwners=')==1
assert 'for(const auto& image:draw.images)if(image.requireReadable)image.requireReadable();' in s
with tempfile.TemporaryDirectory(prefix='bar-deferred-roster-') as path:
 out=Path(path);(out/'deferred-owner-phases.inc').write_text(a.replace('id<MTL4CommandBuffer>','void*'))
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestDeferredOwnerRoster.cpp')),'-o',str(out/'test')],check=True)
 subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Deferred actual lifecycle/ordered finish: 2000 grouped reference cycles, null/cross-list/alias/live callbacks, retry/report flags, lifetime and allocation faults PASS; no app build or Metal execution')
