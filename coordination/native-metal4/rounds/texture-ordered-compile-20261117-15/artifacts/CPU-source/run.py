from pathlib import Path
import subprocess, re, hashlib, json
root=Path(__file__).resolve().parents[1]
src=(root/'candidate/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
journal=src.split('// BEGIN WORLD_TEXTURE_PREPARATION_JOURNAL\n')[1].split('// END WORLD_TEXTURE_PREPARATION_JOURNAL')[0]
journal=journal.replace('__strong id<MTLTexture>','std::shared_ptr<int>').replace('nil','nullptr')
(root/'tests/extracted_journal.h').write_text('#include <algorithm>\n#include <functional>\n#include <vector>\n#include "NativeTextureProducerReceipt.h"\nusing namespace rendering;\nstruct WorldTexture {};\ninline void Require(bool b,const char* s){if(!b)throw std::logic_error(s);}\n'+journal)
# All original texture consumer tests migrate; completed-only proof body remains exact.
base=(root/'base/engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm').read_text()
start='class CompletedS3OImageProof final';end='struct UploadedTexture'
assert base.split(start)[1].split(end)[0]==src.split(start)[1].split(end)[0]
assert src.count('status->Readable(submission)')==7 # six consumer sites plus borrow encoder
assert src.count('status->Readable(sourceSubmission)')==1
assert not re.search(r'status->ready\s*\|\|',src)
rb=(root/'base/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
rc=(root/'candidate/engine/rts/Rendering/Metal4/Metal4RuntimeNative.mm').read_text()
# Entire submission/drain/sync block unchanged, not merely a matched line.
assert rb.split('void RuntimeLease::SubmitAndDefer')[1]==rc.split('void RuntimeLease::SubmitAndDefer')[1]
cmd=['c++','-std=c++20','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'candidate/engine/rts/Rendering/Backend'),'-I'+str(root/'tests'),str(root/'tests/test.cpp'),'-o',str(root/'tests/test')]
subprocess.run(cmd,check=True);subprocess.run([str(root/'tests/test')],check=True)
print('Production helper + extracted journal + consumer/proof/final-drain wiring checks PASS')
