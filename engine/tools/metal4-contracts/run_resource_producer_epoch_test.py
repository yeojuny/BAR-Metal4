#!/usr/bin/env python3
"""Portable receipt tests + actual Objective-C++ wiring checks, not a Metal build."""
from pathlib import Path
import subprocess, tempfile, hashlib
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'normalsEncoded' not in s
assert s.count('resource->normalEpochs.Current()')==3
assert s.count('[resource,encoded]{Require(resource&&encoded->Encoded()')==3
assert s.index('normalEpochs.Begin(normalResource->Completed())') < s.index('impl->Pipelines(lease, layer.pixelFormat')
assert s.index('impl->Pipelines(lease, layer.pixelFormat') < s.index('const id<MTLComputePipelineState> normalPipeline=')
assert 'normalResource,normalEpoch](const FrameMetrics& metrics)' in s
assert 'normalResource->completed.store(true,std::memory_order_release)' in s
producer=s[s.index('if (!normalResource->Completed()) {'):s.index('if (screen != nullptr) screen->EncodeDependencies(command);')]
assert hashlib.sha256(producer.encode()).hexdigest()=='d127975f12385d91c758780d251bce4078ebc729b5215fd8bee97ecf14190c2a' # Original producer modulo exact owner substitutions.
assert 'impl->' not in producer
assert producer.index('[normals endEncoding]') < producer.index('normalEpoch->DidEncode()')
assert 'MTLSizeMake(8,8,1)' in producer
assert 'barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice' in producer
with tempfile.TemporaryDirectory() as tmp:
 out=str(Path(tmp)/'test')
 subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'rts'),str(Path(__file__).with_name('TestResourceProducerEpoch.cpp')),'-o',out],check=True)
 subprocess.run([out],check=True)
print('Actual normal provider/producer/completion wiring checks passed; no native Metal execution')
