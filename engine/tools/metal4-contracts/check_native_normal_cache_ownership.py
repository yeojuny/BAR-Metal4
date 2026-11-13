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
assert 'terrainPair.GetOrCreatePrepared(terrain.heights' in s
assert 'heights=pair->cornerBuffer;terrainNormals=pair->texture;' in s
assert 'std::memcpy(heights.contents' not in s
pair=s.split('auto pair=terrainPair.GetOrCreatePrepared',1)[1].split('ModelCache& Model',1)[0]
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

map_body=s.split('void Map(RuntimeLease& lease',1)[1].split('ModelCache& Model',1)[0]
assert 'map = std::move(updated)' not in map_body
assert map_body.index('stagedMap.emplace')<map_body.index('GetOrCreatePrepared')<map_body.index('lease.AddAllocation(candidate->cornerBuffer)')<map_body.index('lease.AddPersistentAllocation(batch.instances)')<map_body.index('if(stagedMap)map=std::move(*stagedMap)')<map_body.index('heights=pair->cornerBuffer')
assert 'std::is_nothrow_move_assignable_v<MapCache>' in map_body
