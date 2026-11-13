#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldOverlayRenderer.mm').read_text()
start=s.index('  const auto cachedLines=source.sourceLines?nullptr:impl->lineGeometry.Find(')
end=s.index('  const auto& meshVertices=',start)
block=s[start:end]
assert block.count('lineGeometry.Find(')==1 and block.count('exactWorldLine::TryBuild(')==1
assert block.count('validatedBorrow168.Borrow(')==1 and block.count('BuildNativeWorldDrawGeometry(')==1
assert s.index('const ScopedWorldBorrow168 validatedBorrow168(source,borrowWork168)')<s.index('auto rectangle=camera.viewport')<s.index('auto mapped=MapWorldViewport(rectangle,logicalWidth,logicalHeight,width,height)')<start
assert s.index('if(rectangle[2]==0 || rectangle[3]==0)continue;')<start
assert 'if(!mapped)continue;' in s[:start]
cache=(root/'rts/Rendering/Metal4/Metal4WorldLineGeometryCache.inc').read_text()
find=cache[cache.index(' std::shared_ptr<const WorldLineGeometryBuffers> Find('):cache.index(' void Store(')]
assert find.index('ResetContext(lease);')<find.index('Eligible(d,samples)')<find.index('entries.find(')<find.index('completed->load(std::memory_order_acquire)')<find.index('return e.buffers;')
assert 'entries.clear();bytes=0;context=lease.ContextIdentity();device=lease.Device();' in cache
assert 'std::shared_ptr<WorldLineGeometryBuffers> buffers;' in cache
plan=(root/'rts/Rendering/Backend/NativeWorldDrawPlan167.h').read_text()
at=plan[plan.index(' const NativeWorldDrawCommand& At(std::size_t index) const'):plan.index('template<class T> inline void Push')]
assert '++metrics.entryChecks;return c;' in at and 'mutable NativeWorldPlanMetrics167 metrics;' in plan
assert 'owner->draws.at(index)' in at
with tempfile.TemporaryDirectory(prefix='bar-overlay-selection-') as path:
 out=Path(path);(out/'overlay-selection.inc').write_text(block)
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestOverlaySelectionBoundary.cpp')),'-o',str(out/'test')],check=True)
 subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Actual overlay selection block: branch ordering/shortcircuits/failure stops/exact target forwarding/cache owner retention PASS. Mutable selection metrics and lease-reset source gates PASS. Mocks only; no native or asynchronous execution.')
