#!/usr/bin/env python3
"""Actual map publication and registry with allocator faults, no Metal runtime."""
from pathlib import Path
import os,subprocess
root=Path(__file__).resolve().parents[2];out=Path('/tmp/bar-map-publication-transaction');out.mkdir(exist_ok=True)
b=root/'rts/Rendering/Backend'
source=Path(__file__).with_name('TestNativeMapPublicationTransaction.cpp')
base=['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-I'+str(root/'rts'),'-I'+str(b)]
files=[b/(f+'.cpp') for f in ['NativeTextureRegistry','NativeCUSTextureHistory']]
env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:alloc_dealloc_mismatch=0'}
def build(pub,target):
 subprocess.run(base+[str(source),str(pub)]+[str(f) for f in files]+['-o',str(target)],check=True)
build(b/'NativeMapTexturePublication.cpp',out/'test')
subprocess.run([str(out/'test')],env=env,check=True)
# Negative control: original nontransactional pair loop, keeping current registry.
old=subprocess.check_output(['git','show','0779432d2404009686d45bfaa5abc8b2337bb306:engine/rts/Rendering/Backend/NativeMapTexturePublication.cpp'],cwd=root.parent,text=True)
assert 'catch' not in old.split('NativeMapTexturePublication::Publish(',1)[1].split('NativeMapTexturePublication& OriginalNativeMapTextures',1)[0]
(out/'old.cpp').write_text(old)
build(out/'old.cpp',out/'negative')
r=subprocess.run([str(out/'negative')],env=env,capture_output=True,text=True)
assert r.returncode!=0 and 'Assertion' in r.stderr,(r.returncode,r.stdout,r.stderr)
print('Negative control original pair publication fails exact prior-backing restoration')
# Test-only accessor exposes the real history overflow branch, with identical
# overlaid headers in every translation unit. No production API or layout change.
import shutil
overlay=out/'overflow';headers=overlay/'Rendering/Backend';headers.mkdir(parents=True,exist_ok=True)
for header in b.glob('*.h'):shutil.copyfile(header,headers/header.name)
history=headers/'NativeCUSTextureHistory.h'
text=history.read_text().replace('class NativeCUSTextureHistory {\npublic:', 'class NativeCUSTextureHistory {\npublic:\n void TestSetSequence(uint64_t value) noexcept {sequence=value;}')
assert 'TestSetSequence' in text
history.write_text(text)
for f in ['NativeTextureRegistry.cpp','NativeCUSTextureHistory.cpp']:shutil.copyfile(b/f,headers/f)
overflow=out/'overflow.cpp'
overflow.write_text('''#include "Rendering/Backend/NativeTextureRegistry.h"
#include <cassert>
#include <limits>
#include <iostream>
using namespace rendering;
int main(){NativeCUSTextureHistory h;NativeTextureRegistry r(h);r.NewContext();h.ActiveTexture(0);
auto before=r.Create(NativeCUSTextureTarget::Image2D,"overflow");
auto sampling=before->sampling;sampling.minFilter=0x2600;
h.TestSetSequence(std::numeric_limits<uint64_t>::max());bool threw=false;
try{r.SetSampling(before->name,sampling);}catch(const std::overflow_error&){threw=true;}
assert(threw);assert(r.Find(before->name)==before);
std::cout<<"Actual registry history overflow preserves exact prior object PASS\\n";}
''')
# Put the overlay before ordinary root paths, and compile the copied actual TUs.
command=base[:1]+['-I'+str(overlay)]+base[1:]+[str(overflow),str(headers/'NativeTextureRegistry.cpp'),str(headers/'NativeCUSTextureHistory.cpp')]
subprocess.run(command+['-o',str(out/'overflow-test')],check=True)
subprocess.run([str(out/'overflow-test')],env=env,check=True)
registry=headers/'NativeTextureRegistry.cpp'
text=registry.read_text()
start=text.index('auto& entry=objects.at(name);',text.index('void NativeTextureRegistry::Replace('))
end=text.index('entry = ObjectEntry{std::move(object),{},0};',start)+len('entry = ObjectEntry{std::move(object),{},0};')
registry.write_text(text[:start]+'objects[name] = ObjectEntry{std::move(object),{},0};\n\thistory.ObjectStateChanged();'+text[end:])
subprocess.run(command+['-o',str(out/'overflow-negative')],check=True)
r=subprocess.run([str(out/'overflow-negative')],env=env,capture_output=True,text=True)
assert r.returncode!=0 and 'Assertion' in r.stderr,(r.returncode,r.stdout,r.stderr)
print('Negative control old Replace order fails exact prior-object assertion')
