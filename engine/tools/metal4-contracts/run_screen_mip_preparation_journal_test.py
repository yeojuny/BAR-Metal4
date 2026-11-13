#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,hashlib
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4ScreenNative.mm').read_text()
out=Path('/tmp/bar-screen-mip-journal');out.mkdir(exist_ok=True)
state='struct ScreenMipState {'+s.split('struct ScreenMipState {',1)[1].split('// BEGIN SCREEN_MIP_REQUEST_JOURNALS',1)[0]
state=state.rsplit('}',1)[0] # remove closing anonymous namespace only
(out/'screen-mip-state.inc').write_text(state.replace('__strong ','').replace('id<MTLTexture>','Texture'))
journals=s.split('// BEGIN SCREEN_MIP_REQUEST_JOURNALS',1)[1].split('// END SCREEN_MIP_REQUEST_JOURNALS',1)[0]
(out/'screen-mip-journals.inc').write_text(journals.replace('id<MTLTexture>','Texture'))
subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestScreenMipPreparationJournal.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
assert 'pendingMipmaps' not in s
assert s.count('QueueMipmaps(lease,source,')==2
assert 'mipRecovery.Acquire(lease.ScreenMipPreparation(mipOwner))' in s
assert 'MipPreparation(lease)->Queue(source,texture.image,texture.mipState)' in s
queue=s.split('void QueueMipmaps(',1)[1].split('NSMutableDictionary',1)[0]
assert queue.index('if(!texture.mipState||texture.mipState->initialized||texture.mipState->queued)return;')<queue.index('MipPreparation(lease)')
assert 'impl->MipPreparation(lease)->Take([&](id<MTLTexture> image){lease.AddAllocation(image);})' in s
producer=s.split('void PreparedScreenMipmaps::EncodeDependencies(',1)[1].split('struct ExactScreenLineProbe',1)[0]
assert hashlib.sha256(producer.encode()).hexdigest()=='04b30fa70834f36615f57d1c7a3487d0b7200e3099444afba6237e66e2e46790'
h=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.h').read_text().split('class RuntimeLease {',1)[1].split('class CaptureScope',1)[0]
assert h.index('std::unique_lock<std::mutex> lock;')<h.index('std::shared_ptr<ScreenMipLeaseJournals> screenMipJournals;')
runtime=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
destructor=runtime.split('RuntimeLease::~RuntimeLease()',1)[1].split('PublishedRequestState& RuntimeLease::Publications()',1)[0]
assert 'lock.unlock' not in destructor and 'screenMipJournals' not in destructor
calls=[]
for p in (root/'rts/Rendering/Metal4').glob('*.mm'):
 if p.name!='Metal4ScreenNative.mm':calls.extend(line for line in p.read_text().splitlines() if '.PrepareMipmaps(' in line)
assert len(calls)==6 and all('.PrepareMipmaps(lease)' in line for line in calls)
assert 'SourceSubmission' not in journals
print('all six standalone/world plan routes preserve lease; both texture queue sites; unchanged GPU producer/retirement body and capsule-before-lock destruction order PASS')
