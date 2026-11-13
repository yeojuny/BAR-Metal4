#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2];m=root/'rts/Rendering/Metal4';out=Path('/tmp/bar-shadow-preparation-owner');out.mkdir(exist_ok=True)
# Actual Prepare/Matches logic; Metal device and buffer allocations replaced by
# CPU sentinels only. This is ownership/order evidence, not a Metal build.
s=(m/'Metal4ShadowTerrainPreparation.inc').read_text().replace('__strong id<MTLDevice>','const void*').replace('id<MTLDevice>','const void*').replace('=nil','=nullptr')
(out/'preparation.inc').write_text(s)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestShadowPreparationOwnership.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
engine=(m/'Metal4EngineShadow.mm').read_text();renderer=(m/'Metal4WorldRenderer.mm').read_text();bridge=(m/'Metal4SceneBridge.cpp').read_text();header=(m/'Metal4WorldRenderer.h').read_text();source=(m/'Metal4OriginalShadowSource.mm').read_text()
assert 'terrainPreparation' not in engine
assert 'const bool useEarly=early&&early->Matches(' in engine
assert 'useEarly?early->geometry[ordinal]:impl->terrainGeometry.Acquire(' in engine
assert 'options.preparedShadowTerrain=independentResources;' in bridge
assert 'result.shaderCompileMs,options.preparedShadowTerrain)' in renderer
assert 'images,std::move(preparedGeometry))' in source
assert 'std::shared_ptr<const metal4::ShadowTerrainPreparation> preparedShadowTerrain;' in header
assert 'options(source)' in header
body=renderer.split('Metal4WorldRenderer::PrepareIndependentResources(',1)[1].split('Metal4WorldPresentationResult',1)[0]
assert body.index('FinishCPUWork();')<body.index('NativeRendererAccessGate::Scope')<body.index('PrepareTerrainGeometry(')
print('1000 exact A/B source/context/device/owner/count and release cases PASS; actual typed request wiring + conservative fallback/gates PASS')
