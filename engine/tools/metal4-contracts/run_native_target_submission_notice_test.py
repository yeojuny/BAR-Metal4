#!/usr/bin/env python3
"""Production Begin journal + serial worker; no Metal/app runtime validation."""
import os, subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path('/tmp/bar-native-target-submission-notice');out.mkdir(exist_ok=True)
source=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
dispatch=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawDispatch(',1)[1].split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[0]
assert 'NativeTargetSubmissionNotice targetNotice(options.screenUI);' in dispatch
assert 'targetNotice.MarkBegun(submission);' in dispatch
assert 'deliverTargetNotice();\n  deliverReceipt();' in dispatch
assert 'catch(...){deliverTargetNotice();deliverReceipt();throw;}' in dispatch
assert 'ui::GetRecorder().RecordTargetSubmission' in dispatch
prepared=source.split('Metal4WorldPresentationResult Metal4WorldRenderer::DrawPrepared(',1)[1]
assert 'ui::GetRecorder().RecordTargetSubmission' not in prepared
assert prepared.index('originalSubmission.began=true')<prepared.index('targetSubmissionBegan(sourceSubmission)')<prepared.index('options.submissionBegan(sourceSubmission')
subprocess.run(['g++','-std=c++23','-pthread','-I'+str(root/'rts'),'-O1','-g','-fsanitize=address,undefined',str(Path(__file__).with_name('TestNativeTargetSubmissionNotice.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
