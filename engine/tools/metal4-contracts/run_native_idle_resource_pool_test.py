#!/usr/bin/env python3
from pathlib import Path
import subprocess,os
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-idle-resource-pool');out.mkdir(exist_ok=True)
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(root/'rts'),str(Path(__file__).with_name('TestNativeIdleResourcePool.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
s=(root/'rts/Rendering/Metal4/Metal4MainAttachments.h').read_text()
assert 'NativeIdleResourcePool<MainFrameAttachments,2> storage;' in s
assert 'std::shared_ptr<const MainFrameAttachments> storageOwner;' in s
assert 'prepared->storageOwner=candidate;' in s
prepare=s.split('class MainAttachmentStorage {',1)[1]
assert prepare.index('prepared->storageOwner=candidate')<prepare.index('if(image)lease.AddAllocation(image)')<prepare.index('frame=std::move(prepared)')<prepare.index('generation=std::max')
assert 'auto prepared=std::make_shared<MainFrameAttachments>(*candidate)' in s
assert 'void Clear() { storage.Clear(); }' in s
world=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'main->RetainForContinuation(lease)' in world
assert 'originalLifetime->mainAttachments=mainAttachments' in world
print('Native backing/descriptor pin and continuation structural checks PASS; no Objective-C++ execution')
terrain=s.split('class MainTerrainDepthSnapshot {',1)[1]
assert 'NativeIdleResourcePool<MainTerrainDepthStorage,2>& storage' in terrain
assert 'bool(candidate.samples)==needsSamples&&bool(candidate.single)==needsSingle' in terrain
assert 'result->storageOwner=candidate' in terrain
assert 'reusableSamples' not in terrain and 'reusableSingle' not in terrain
assert 'crop,needsSampleDepth,true,impl->terrainDepthStorage)' in world
assert 'impl->terrainDepthStorage.Clear();' in world
print('Cropped terrain depth exact pair ownership/native selection structural PASS')
