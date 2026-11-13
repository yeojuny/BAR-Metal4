#!/usr/bin/env python3
"""Production receipt header plus source wiring guard, not a Metal runtime test."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-native-environment-stage-test');out.mkdir(exist_ok=True)
source=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
assert 'impl->environmentInputs' not in source
assert 'impl->reflectionMetalInfo' not in source
assert 'impl->reflectionWorldInfo' not in source
capture=source.split('NativeEnvironmentStageReceipt Metal4WorldRenderer::CaptureEnvironmentInputs(',1)[1].split('bool Metal4WorldRenderer::SubmitScreenTargets',1)[0]
assert 'MainThread(); Require(impl != nullptr && frame != nullptr' in capture
assert 'return {std::move(frame),shaderValidation};' in capture
assert 'impl->' not in capture
prepared=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert 'options.environmentStage.HasCubeSource()' in prepared
assert prepared.index('if(options.environmentStage.frame)')<prepared.index('frame.mainSampling.samples')
assert 'input->metalInfoFrame,options.environmentStage.shaderValidation,compileMs' in prepared
assert 'input->worldInfoFrame,options.environmentStage.shaderValidation,compileMs' in prepared
assert 'Require(options.environmentStage.Matches(frame.environmentFrame), "Original reflection image stage was not retained before Lua PreUnit")' in prepared
assert 'completedShield=frame.shieldDepthFrame,reflectionWorldInfo,' in prepared
bridge=(root/'rts/Rendering/Metal4/Metal4SceneBridge.cpp').read_text()
assert bridge.count('options.environmentStage=session->environmentStage;')==2
assert bridge.count('session->environmentStage={};')==2
assert 'session->environmentStage=session->renderer->CaptureEnvironmentInputs(session->capture.PrepareEnvironmentBeforePreUnit());' in bridge
subprocess.run(['g++','-std=c++23','-pthread','-I'+str(root/'rts'),'-O1','-g','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativeEnvironmentStageReceipt.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
