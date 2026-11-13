#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4PBRSource.mm').read_text()
out=Path('/tmp/bar-pbr-sampler-cache');out.mkdir(exist_ok=True)
a=s.split('// BEGIN PBR_SAMPLER_CACHE',1)[1].split('// END PBR_SAMPLER_CACHE',1)[0]
(out/'pbr-sampler-cache.inc').write_text(a.replace('__strong ','').replace('id<MTLDevice>','Device').replace('id<MTLSamplerState>','Sampler'))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestPBRSamplerCache.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
image=s.split('CUSSourceImage PBRSourceGPU::Image(',1)[1]
assert image.index('Storage(image)')<image.index('lease.AddAllocation(storage->texture)')<image.index('state==image->allocation.sampling')<image.index('impl->samplers.Get')<image.index('lease.Retain(sampler)')
assert 'const bool useLinear=state.minFilter==0x2601;' in image
expected='auto* sd=[MTLSamplerDescriptor new];sd.minFilter=sd.magFilter=useLinear?MTLSamplerMinMagFilterLinear:MTLSamplerMinMagFilterNearest;sd.mipFilter=MTLSamplerMipFilterNotMipmapped;sd.sAddressMode=sd.tAddressMode=sd.rAddressMode=MTLSamplerAddressModeClampToEdge;sd.supportArgumentBuffers=YES;'
factory=image.split('impl->samplers.Get(lease.Device(),useLinear,[&]{',1)[1].split('});lease.Retain(sampler);',1)[0]
assert ''.join(factory.split())==''.join((expected+'return [lease.Device() newSamplerStateWithDescriptor:sd];').split())
assert 'result.requireReadable=[storage,submission]{Readable(*storage,submission);}' in image
print('Actual bounded PBR sampler cache: 2000 exact hits, device identity/lifetime/rollover, nil/throw retry PASS; production descriptor and per-consumer guards wiring PASS')
