#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4CubeTextureSource.mm').read_text()
out=Path('/tmp/bar-cube-lease-samplers');out.mkdir(exist_ok=True)
a=s.split('// BEGIN CUBE_LEASE_SAMPLERS',1)[1].split('// END CUBE_LEASE_SAMPLERS',1)[0]
(out/'cube-lease-samplers.inc').write_text(a.replace('__strong ','').replace('id<MTLSamplerState>','Sampler'))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),'-I'+str(root/'rts'),str(Path(__file__).with_name('TestCubeLeaseSamplers.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
f=s.split('id<MTLSamplerState> Sampler(',1)[1].split('\n}\n}',1)[0]
import hashlib
assert hashlib.sha256(f.split(' auto* cache=',1)[0].encode()).hexdigest() == 'cc4d8e66ef7092944f46810f2d1daa250000ee1688ea4776462965f2677a4e8a', 'original descriptor validation prefix must remain unchanged'
assert 'if(cache)if(auto sampler=cache->Find(p)){lease.Retain(sampler);return sampler;}' in f
assert f.index('Require(sampler,')<f.index('cache->Remember(p,sampler)')<f.rindex('lease.Retain(sampler)')
h=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.h').read_text()
assert 'std::shared_ptr<CubeLeaseSamplers> cubeSamplers;' in h
prepare=s.split('std::shared_ptr<PreparedCubeTexture> PrepareNativeCubeTexture(',1)[1].split('SourceDependencyResult160',1)[0]
assert 'p->sampler=actualSampler?actualSampler:Sampler(lease,sampling)' in prepare
assert 'actualSampler.device==lease.Device()' in prepare
print('unchanged validation/descriptor prefix, actualSampler bypass/device guard, per-consumer retain and successful-only insertion PASS')
