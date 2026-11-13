#!/usr/bin/env python3
from pathlib import Path
import subprocess,os,hashlib
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4WorldParticles.mm').read_text()
out=Path('/tmp/bar-particle-atlas-journal');out.mkdir(exist_ok=True)
state=s.split('struct Atlas {',1)[1].split('void EncodeAtlasMipmaps(',1)[0]
(out/'particle-atlas-state.inc').write_text(('struct Atlas {'+state).replace('__strong ','').replace('id<MTLTexture>','Texture'))
complete='void CompleteAtlasMipmaps('+s.split('void CompleteAtlasMipmaps(',1)[1].split('struct RetainedAtlasImage',1)[0]
(out/'particle-atlas-complete.inc').write_text(complete)
journal=s.split('// BEGIN PARTICLE_ATLAS_PREPARATION_JOURNAL',1)[1].split('// END PARTICLE_ATLAS_PREPARATION_JOURNAL',1)[0]
(out/'particle-atlas-journal.inc').write_text(journal)
prepare='void PrepareAtlasImages('+s.split('void WorldParticlesGPU::PrepareAtlasImages(',1)[1].split('void WorldParticlesGPU::EncodeAtlasImages(',1)[0]
(out/'particle-atlas-prepare.inc').write_text(prepare)
subprocess.run(['g++','-std=c++23','-Wall','-Wextra','-Werror','-O1','-g','-fsanitize=address,undefined','-I'+str(out),'-I'+str(root/'rts'),str(Path(__file__).with_name('TestParticleAtlasPreparationJournal.cpp')),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
assert 'sourceAtlases' not in s and 'impl->sourceSubmission' not in s
assert 'impl->lastAtlasPreparation.reset()' in s
assert 'lease.SourceSubmission()==submission' in s
assert prepare.index('impl->GetAtlas(lease,atlas->owner)==atlas')<prepare.index('journal->Prepare(')<prepare.index('impl->lastAtlasPreparation=journal')
assert 'lease.ParticleAtlasPreparation()->Register(atlas,submission)' in s
# Freeze complete production GPU producer/retirement bodies and cache lookup,
# including pending producer drain/device/residency, and all image callbacks.
for begin,end,digest in [
 ('void EncodeAtlasMipmaps(', 'struct RetainedAtlasImage', 'd5a175b8210afc0660a27153e4b81fd9257bdb5afe16501cfe509cb58da504f7'),
 ('std::shared_ptr<Atlas> GetAtlas(', 'std::shared_ptr<ShadowColorCache> shadowColor', 'a937912e49c8cb3f7fbd5a76cfe25631289c29d0376b98a2f2498cb40a528aba'),
 ('CUSSourceImage image;image.texture=target==', 'void WorldParticlesGPU::PrepareAtlasImages(', '5ab6084d4b1e511eed778736c9f132072df711fc13782390e6d6faa81a55ba91')]:
 part=s.split(begin,1)[1].split(end,1)[0]
 assert hashlib.sha256(part.encode()).hexdigest()==digest
r=(root/'rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
for method,count in [('PrepareAtlasImages',3),('EncodeAtlasImages',3),('CompleteAtlasImages',2),('TakeAtlasCompletion',1)]:
 assert r.count('particlesGPU.'+method+'(lease,')==count
 assert r.count('particlesGPU.'+method+'(')==count
h=(root/'rts/Rendering/Metal4/Metal4RuntimeNative.h').read_text()
assert 'std::shared_ptr<ParticleAtlasPreparationJournal> particleAtlasPreparation;' in h
print('all three prepare/encode, two sync and one detached completion routes use exact lease; original mip producer/retirement/cache/readability bodies unchanged PASS')
