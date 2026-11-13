#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-terrain-info-receipts');out.mkdir(exist_ok=True)
s=(root/'rts/Rendering/Metal4/Metal4TerrainInfoSource.mm').read_text()
a=s.index('// BEGIN TERRAIN_INFO_LEASE_RECEIPTS');b=s.index('// END TERRAIN_INFO_LEASE_RECEIPTS',a)
(out/'terrain-info-receipts.inc').write_text(s[a:b])
r=s[s.index('struct InfoResource final:'):s.index('struct ImageOwner {')]
(out/'terrain-info-resource.inc').write_text(r.replace('id<MTL4CommandBuffer>','void*').replace('nil','nullptr'))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestTerrainInfoLeaseReceipts.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
f=s.split('CUSSourceImage ResolveNativeTerrainInfoImage(',1)[1]
lookup=f.index('receipts->Find(key)')
assert f.index('CheckTexture(')<f.index('resource->Register(')<f.index('lease.Retain(sampler)')<lookup
assert f.count('receipts->Remember(key,out)')==2
assert f.index('lease.OrderAfterPreviousSubmission(')<f.index('receipts->Remember(key,out)')
assert 'out.requireReadable=readable;' in f and 'owner->resource->Readable(submission)' in f
print('Actual bounded typed receipt cache: 1000 cycles, all10 key changes, lease isolation, dynamic readability and capacity PASS; production validation/order preserved')
