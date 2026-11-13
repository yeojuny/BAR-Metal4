#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-terrain-shading-receipts');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4TerrainShadingSource.mm').read_text()
a=s.index('// BEGIN TERRAIN_SHADING_LEASE_RECEIPTS');b=s.index('// END TERRAIN_SHADING_LEASE_RECEIPTS',a)
(out/'shading-receipts.inc').write_text(s[a:b])
r=s[s.index('struct Resource final:'):s.index('void Register(')].replace('__strong id<MTLTexture>','void*').replace('nil','nullptr')
r+=s[s.index('void Readable('):s.index('void Encode(')]
r+=s[s.index('void Complete('):s.index('struct Owner {')]
(out/'shading-resource.inc').write_text(r)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestTerrainShadingLeaseReceipts.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
f=s.split('CUSSourceImage ResolveNativeTerrainShadingImage(',1)[1]
assert f.index('auto resource=Prepare(')<f.index('Require(resource->texture.device')<f.index('lease.Retain(sampler)')<f.index('receipts->Find(key)')
assert f.index('out.didComplete=')<f.index('receipts->Remember(key,out)')
assert 'Readable(*owner->resource,owner->submission)' in f and 'Complete(*owner->resource,*owner->source,owner->submission)' in f
print('Shading actual typed cache1000 x14 key cases + allocation/copy fallback + actual recursive Readable/Complete states PASS (modeled Metal dependencies)')
