#!/usr/bin/env python3
"""Structural native routing contract, not Objective-C++/Metal execution."""
from pathlib import Path
s=(Path(__file__).resolve().parents[2]/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
body=s.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
fields={'waterReflection':'reflection','waterReflectionDepth':'reflectionDepth','waterRefraction':'refraction','waterRefractionDepth':'refractionDepth','preWaterColor':'preColor','preWaterDepth':'preDepth'}
start=body.index('auto waterAttachments=std::make_shared<M4WaterAttachments>()')
pin=body.index('originalLifetime->waterAttachments=waterAttachments')
retain=body.index('lease.Retain(originalLifetime)')
publish=body.index('impl->waterReflection=waterAttachments->reflection')
begin=body.index('lease.Begin(',publish)
assert start<pin<retain<publish<begin
for cached,selected in fields.items():
 # Only initial selection and complete publication may touch mutable latest fields.
 assert body.count('impl->'+cached+';')==1
 import re
 assert len(re.findall(r'impl->'+cached+r'\b',body))==2
 assert 'impl->'+cached+'=waterAttachments->'+selected+';' in body
assert 'std::shared_ptr<const M4WaterAttachments> waterAttachments;' in s
assert 'target.device != lease.Device()' in body and '(target.usage & usage) != usage' in body
assert 'DynamicWaterSceneImages images{waterAttachments->reflection,waterAttachments->refraction' in body
assert 'toTexture:waterAttachments->preDepth' in body
assert 'setTexture:waterAttachments->preColor.gpuResourceID' in body
assert body.index('lease.RetirePreviousSubmission();')<start
print('Water attachment selection/retention/publication and exact downstream routing structural PASS; no native execution')
