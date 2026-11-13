#!/usr/bin/env python3
"""Actual source DTOs and worker transport; native compiler execution is not covered."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='bar-compile-sources-') as d:
 binary=Path(d)/'test'
 subprocess.run(['g++','-std=c++23','-pthread','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeCompileControlSources.cpp')),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True,timeout=40)
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
for family in ['Terrain','DeferredLight','UnitStencil','Outline','Distortion','SensorPreview']:
 b=s.split('Metal4WorldRenderer::CompileOriginal'+family+'Program(',1)[1].split('\n}',1)[0]
 assert b.index('Require([NSThread isMainThread]')<b.index('Scope cacheAccess')<b.index('runtime.CanSafelyDetach()')<b.index('device=runtime.Device()' if family=='SensorPreview' else 'device=layer.device')<b.index('InvokeNativeRendererControl')<b.index('RuntimeLease lease(RuntimeLease::Retirement::RequireAlreadyRetired,runtime)')<b.index('Require(device==lease.Device()'),family
 typename='NativeTerrainProgramRequest' if family=='Terrain' else 'Native'+family+'ProgramSource'
 assert 'make_shared<const '+typename+'>(source)' in b and 'lease,*captured,' in b,family
 assert '.Begin(' not in b and 'Submit' not in b and 'previousRetired' not in b,family
 if family=='SensorPreview':assert 'Metal4WindowSurfaceNativeAccess' not in b and 'device=runtime.Device()' in b
 else:assert 'layer&&layer.device==runtime.Device()' in b
terrain=s.split('Metal4WorldRenderer::CompileOriginalTerrainProgram(',1)[1].split('\n}',1)[0]
for contract in ['target.colors[0]=MTLPixelFormatRGBA8Unorm','target.colors.fill(MTLPixelFormatRGBA8Unorm)','target.combinedColor=MTLPixelFormatRGBA8Unorm','target.depth=MTLPixelFormatDepth32Float;target.stencil=MTLPixelFormatInvalid;target.samples=1','target,NativeDiagnosticInstrumentation,milliseconds']:
 assert contract in terrain,contract
print('Six native routes owned-source/main-drain/device/retired-only lease/no-submit structural PASS')
