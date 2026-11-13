#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-completed-cube-receipts');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4CubeTextureSource.mm').read_text()
for name,marker in [('cache','COMPLETED_CUBE_LEASE_RECEIPTS'),('eligibility','COMPLETED_CUBE_ELIGIBILITY')]:
 a=s.index('// BEGIN '+marker);b=s.index('// END '+marker,a)
 (out/('completed-cube-'+name+'.inc')).write_text(s[a:b])
(out/'completed-cube-readable.inc').write_text(s[s.index('void Readable('):s.index('void RequireCubePredecessorComplete158(')])
parts=s[s.index('void RequireCubePredecessorComplete158('):s.index('uint64_t OrderCubePredecessor158(')]
parts+=s[s.index('bool CubeDependenciesAlreadyEncoded158('):s.index('MTLSamplerAddressMode Address(')]
parts+=s[s.index('SourceDependencyResult160 PreparedCubeTexture::InspectDependencies160()'):s.index('void PreparedCubeTexture::EncodeDependencies(')]
parts+=s[s.index('void PreparedCubeTexture::DidComplete()'):s.index('CUSSourceImage ResolveNativeCubeTexture(')]
(out/'completed-cube-prepared.inc').write_text(parts)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestCompletedCubeLeaseReceipts.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
f=s.split('CUSSourceImage ResolveNativeCubeTexture(',1)[1]
assert f.index('Require(submission')<f.index('Storage(*image)')<f.index('CompletedCubeReadEligible(')<f.index('receipts->Find(key)')
hit=f.split('receipts->Find(key)',1)[1].split('return *cached;',1)[0]
for token in ['RetainCubeStorage(lease,resource->storage)','actualSampler.device==lease.Device()','lease.Retain(actualSampler)','lease.AddAllocation(resource->storage->texture)','lease.AddAllocation(resource->storage->depth)']:assert token in hit
assert f.index('PrepareNativeCubeTexture(')<f.index('out.dependencyQuery160=')<f.index('receipts->Remember(key,out)')
assert 'prepared.get()' in f and 'out.owner=prepared' in f
prepare=s.split('std::shared_ptr<PreparedCubeTexture> PrepareNativeCubeTexture(',1)[1].split('SourceDependencyResult160',1)[0]
assert 'CompletedCubeReads' not in prepare and 'CompletedCubeReadEligible' not in prepare
renderer=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
terrain=(root/'rts/Rendering/Metal4/Metal4TerrainSourceGPU.mm').read_text()
assert 'ResolveNativeCubeTexture(lease,binding,submission,actualSampler)' in renderer
assert 'SMF_SKY_REFLECTIONS' in terrain and 'provider(lease,binding,unsigned(unit),sampler)' in terrain
assert 'for(const auto& cached:samplers)' in terrain
print('Completed cube cache: actual eligibility/readability, 1000 lifetime/query cycles, key/capacity/copy faults and production route/wiring PASS; Metal dependencies modeled')
