#!/usr/bin/env python3
"""Actual registry/receipt CPU allocation faults; GPU allocation wiring structural only."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2];b=root/'rts/Rendering/Backend';source=Path(__file__).with_name('TestNativeGeometryAllocationReceipt.cpp')
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:alloc_dealloc_mismatch=0'}
with tempfile.TemporaryDirectory(prefix='bar-geometry-receipt-') as d:
 out=Path(d);base=['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),'-I'+str(b)]
 files=[str(source),str(b/'NativeTextureRegistry.cpp'),str(b/'NativeCUSTextureHistory.cpp')]
 subprocess.run(base+files+['-o',str(out/'test')],check=True);subprocess.run([str(out/'test')],env=env,check=True)
 overlay=out/'Rendering/Backend';overlay.mkdir(parents=True)
 old=(b/'NativeGeometryAllocationReceipt.h').read_text();line='for(unsigned i=count;i-->0;)if(published[i])registry.CancelContentPublication(published[i],prior[i]);';assert line in old
 (overlay/'NativeGeometryAllocationReceipt.h').write_text(old.replace(line,'// Negative control: omit rollback.'))
 subprocess.run(base[:1]+['-I'+str(out)]+base[1:]+files+['-o',str(out/'negative')],check=True)
 r=subprocess.run([str(out/'negative')],env=env,capture_output=True,text=True);assert r.returncode!=0 and 'Assertion' in r.stderr,(r.returncode,r.stderr)
 print('Negative control missing exact rollback fails prior backing preservation')
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text();body=s.split('Metal4WorldRenderer::CheckShieldGeometryAllocation(',1)[1].split('Metal4WorldRenderer::CapturePBRSource(',1)[0]
assert body.index('Require([NSThread isMainThread]')<body.index('runtime.CanSafelyDetach()')<body.index('ValidateSources')<body.index('InvokeNativeRendererControl')<body.index('RuntimeLease lease(RuntimeLease::Retirement::RequireAlreadyRetired,runtime)')<body.index('receipt.Publish')
worker=body.split('[this,&runtime]',1)[1].split('}});',1)[0];assert 'registry' not in worker and 'PublishContent' not in worker and '.Begin(' not in worker
assert 'Request{request,sourceFrame}' in body and 'result.complete=true;return result;' in worker
print('Source registry never enters worker; exact main receipt publication after joined allocation structural PASS')
