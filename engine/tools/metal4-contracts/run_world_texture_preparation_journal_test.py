#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,hashlib,re
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
out=Path('/tmp/bar-world-texture-journal');out.mkdir(exist_ok=True)
a=s.split('// BEGIN WORLD_TEXTURE_PREPARATION_JOURNAL',1)[1].split('// END WORLD_TEXTURE_PREPARATION_JOURNAL',1)[0]
(out/'world-texture-journal.inc').write_text(a.replace('__strong ','').replace('id<MTLTexture>','Texture'))
a=s.split(' void StartTexturePreparation(',1)[1].split(' std::function<void()> TakeTextureMipCompletion',1)[0]
(out/'world-texture-journal-start.inc').write_text('void StartTexturePreparation('+a)
lookup=s.split('id<MTLTexture> Texture(RuntimeLease&',1)[1].split('auto journal=lease.WorldTexturePreparation();',1)[1].split('return it->second.sampled;',1)[0]
(out/'world-texture-journal-lookup.inc').write_text('auto journal=lease.WorldTexturePreparation();'+lookup.replace('it->second','entry'))
subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestWorldTexturePreparationJournal.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
assert 'pendingMipTextures' not in s
assert s.count('impl->EncodePendingTextureMipmaps(lease,command,')==4
assert s.count('impl->CompletePendingTextureMipmaps(lease,')==3
assert s.count('impl->TakeTextureMipCompletion(lease,sourceSubmission)')==1
assert s.count('impl->StartTexturePreparation(lease);')==3
maintenance=s.split('Metal4MaintenanceResult Metal4WorldRenderer::SubmitMaintenance(',1)[1].split('ResolvedSSAOImage ResolvePostprocessDepth',1)[0]
assert maintenance.index('return result;')<maintenance.index('RuntimeLease lease;')<maintenance.index('StartTexturePreparation(lease,true)')
assert 'for (const auto& cached: lease.WorldTexturePreparation()->entries) impl->RetainTextureAllocations(lease, cached);' in maintenance
main=s.split('RuntimeLease lease(std::move(previousRetired));',1)[1]
assert main.index('postprocessGPU.PrepareFrameImages')<main.index('StartTexturePreparation(lease)')<main.index('impl->Map(lease')
texture=s.split('id<MTLTexture> Texture(RuntimeLease&',1)[1].split('void EncodePendingTextureMipmaps',1)[0]
assert texture.index('RetirePreviousSubmission()')<texture.index('RetainTextureAllocations(')<texture.index('if(!journal->initialized)StartTexturePreparation(lease,true)')<texture.index('journal->Register')
encode=s.split('void EncodePendingTextureMipmaps(',1)[1].split(' void StartTexturePreparation(',1)[0]
# Freeze actual Metal producer body, including strict different-submission rejection,
# copy/generate/optimize commands and every original barrier, apart from loop owner.
body=encode.split('   if(cached->status->ready)',1)[1]
EXPECTED_HASH='ed3e92a2eb0d133016a1a0d423eef0f2acac08c9b4abe05a2c748ac84806d85d'
assert hashlib.sha256(body.encode()).hexdigest()==EXPECTED_HASH
h=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.h').read_text()
assert 'std::shared_ptr<WorldTexturePreparationJournal> worldTexturePreparation;' in h
print('all four encode/three sync/one async call sites use explicit lease; reset order, maintenance recovery/residency and unchanged GPU producer body PASS')
