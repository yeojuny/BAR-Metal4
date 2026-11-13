#!/usr/bin/env python3
"""Structural ownership checks only: deliberately not a Metal/runtime test."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'normalsReady' not in s
assert 'std::shared_ptr<MapImage> normalImage;' in s
assert 'if(!normalBacking->Resource())normalBacking->Fulfill(impl->normalImage);' in s
assert 'actual==impl->normalImage && actual->texture==impl->terrainNormals' in s
assert s.count('normalImage.reset();')==1
assert 'impl->terrainPair.Clear();' in s
assert 'terrainPair.GetOrCreate(terrain.heights' in s
assert 'heights=pair->cornerBuffer;terrainNormals=pair->texture;' in s
assert 'std::memcpy(heights.contents' not in s
pair=s.split('auto pair=terrainPair.GetOrCreate',1)[1].split('lease.AddAllocation(heights)',1)[0]
assert pair.index('cornerBuffer=Buffer')<pair.index('image->texture=[lease.Device()newTexture')<pair.index('return image;')<pair.index('heights=pair->cornerBuffer')
assert 'retire([&]{std::dynamic_pointer_cast<const Impl::MapImage>(normalBacking->Resource())->completed=true;});' in s
world=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert world.index('lease.RetirePreviousSubmission();')<world.index('impl->Map(lease, frame.terrain);')<world.index('normalBacking->Fulfill(impl->normalImage)')
assert 'normalBacking=std::move(normalBacking)]' in world
print('Normal cache structural ownership checks passed; no Objective-C++ or Metal execution')

import subprocess,os
out=Path('/tmp/bar-normal-owner');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeNormalResourceOwner.cpp')),str(root/'rts/Rendering/Backend/NativeMapTexturePublication.cpp'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)

subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeSourceResourceCache.cpp')),'-o',str(out/'cache-test')],check=True)
subprocess.run([str(out/'cache-test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
