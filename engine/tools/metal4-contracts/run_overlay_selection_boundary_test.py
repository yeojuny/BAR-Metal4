#!/usr/bin/env python3
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldOverlayRenderer.mm').read_text()
start=s.index('  const auto cachedLines=source.sourceLines?nullptr:impl->lineGeometry.Find(')
end=s.index('  const auto& meshVertices=',start)
block=s[start:end]
assert block.count('lineGeometry.Find(')==1 and block.count('exactWorldLine::TryBuild(')==1
assert block.count('validatedBorrow168.Borrow(')==1 and block.count('NativeOwnedWorldGeometry geometry(')==1
assert block.index('geometry.SealIndependentClippedFillOnCaller();')<block.index('clippedFill->TryTake(')<block.index('geometry.Execute();mesh=geometry.Take();')
assert 'sealed&&clippedFill&&clippedFill->TryTake(frame.Owner(),&source,mesh)' in block
assert 'std::shared_ptr<const NativeWorldDrawCommand>(frame.Owner(),&source)' in block
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
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
launch=renderer.index('clippedFill->Start();')
consume=renderer.index('clippedFill?&*clippedFill:nullptr')
assert renderer.count('clippedFill?&*clippedFill:nullptr')==1
assert 'return value&&std::strcmp(value,"1")==0;' in renderer[renderer.index('static const bool clippedFillPilot='):launch]
assert 'worldPlan167::MainStage(draw.stage)' in renderer[:launch]
assert launch<renderer.index('if(const auto& original=mainGround.source->ground;',launch)<renderer.index('PrepareOpaqueModelSource(',launch)<renderer.index('screen = impl->screenGPU.Prepare(',launch)<consume
main=renderer[renderer.rfind('const auto mainDraws=preparedWorld167.Main();',launch,consume):consume]
assert 'overlays = impl->overlayGPU.Prepare(lease, mainDraws' in main
header=(root/'rts/Rendering/Metal4/Metal4WorldOverlayRenderer.h').read_text()
assert 'NativeClippedFillPrecompute* clippedFill=nullptr' in header
with tempfile.TemporaryDirectory(prefix='bar-overlay-selection-') as path:
 out=Path(path);(out/'overlay-selection.inc').write_text(block)
 helper=(root/'rts/Rendering/Backend/NativeOwnedWorldGeometry.h').read_text()
 geometry=(root/'rts/Rendering/Backend/NativeWorldDrawGeometry.h').read_text()
 predicate=geometry[geometry.index('inline bool IsIndependentNativeWorldClippedFill('):geometry.index('// Original geometry validation prefix')]
 (out/'owned-world-geometry.inc').write_text('namespace rendering {\n'+predicate+'}\n'+helper.replace('#include "NativeWorldDrawGeometry.h"',''))
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestOverlaySelectionBoundary.cpp')),'-o',str(out/'test')],check=True)
 subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Actual overlay selection block: branch ordering/shortcircuits/failure stops/exact target forwarding/cache owner retention/demand-seal-before-take/hit-miss-validation failure PASS. Mutable selection metrics and lease-reset source gates PASS. Mocks only; no native or asynchronous execution.')
