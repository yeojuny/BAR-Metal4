from pathlib import Path
import datetime,hashlib,json,shutil,subprocess

S=Path(__file__).resolve().parent
Q=S.parent/'shared-program-v2-20261115-01'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(*args):return subprocess.run(args,check=True,capture_output=True,text=True).stdout
def deps(p):return run('/usr/bin/otool','-L',str(p)).splitlines()[1:]
parent=json.loads((Q/'CANDIDATE.json').read_text())
raw=S/'build/spring-metal4-scene';binary=S/'runtime/native/bin/spring-metal4-scene'
assert raw.is_file() and b'BAR_NATIVE_MODEL_TYPE_FACTS' in raw.read_bytes()
assert sha(Q/'runtime/native/bin/spring-metal4-scene')==parent['signedSHA256']
rawSHA=sha(raw);shutil.copy2(raw,binary)
before=deps(binary)
for old,new in parent['relocations']:
 assert any(line.strip().startswith(old+' ') for line in before),old
 run('/usr/bin/install_name_tool','-change',old,new,str(binary))
run('/usr/bin/codesign','--force','--sign','-',str(binary))
run('/usr/bin/codesign','--verify','--strict',str(binary))
assert deps(binary)==deps(Q/'runtime/native/bin/spring-metal4-scene')
signedSHA=sha(binary)
for name in ('gfx_sepia_tone.lua','sepia_pilot_program.lua'):
 assert sha(S/'runtime/native/data/LuaUI/NativeWidgets'/name)==sha(Q/'runtime/native/data/LuaUI/NativeWidgets'/name)
assert sha(Q/'runtime/native/bin/spring-metal4-scene')==parent['signedSHA256']
product=dict(variant='private-model-source-type-facts',package=parent['product']['package'],
 binary=str(binary),native=str(S/'runtime/native'),sha256=signedSHA,publishedPackageReadOnly=True)
(S/'measurement').mkdir(exist_ok=True)
(S/'measurement/PRODUCT.json').write_text(json.dumps(product,indent=2)+'\n')
contract=json.loads((Q/'measurement/RELEASE-CONTRACT.json').read_text())
# Preserve every release contract field, replacing only the private binary pin.
def replace(value):
 if isinstance(value,str):return value.replace(parent['signedSHA256'],signedSHA)
 if isinstance(value,list):return [replace(x) for x in value]
 if isinstance(value,dict):return {k:replace(v) for k,v in value.items()}
 return value
(S/'measurement/RELEASE-CONTRACT.json').write_text(json.dumps(replace(contract),indent=2)+'\n')
receipt=dict(status='SIGNED_PRIVATE_CANDIDATE',utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
 rawSHA256=rawSHA,signedSHA256=signedSHA,parentSHA256=parent['signedSHA256'],
 relocations=parent['relocations'],product=product,adopted=False,runtime='PENDING',
 cause='Positive-only immutable CPU backing type facts; no GPU binding/readiness reuse',
 environmentFlag='BAR_NATIVE_MODEL_TYPE_FACTS',compiledDefault=0,sourceClosure='SOURCE-CLOSURE.json')
(S/'CANDIDATE.json').write_text(json.dumps(receipt,indent=2)+'\n')
manifestPath=S/'runtime/native/manifest.json';manifest=json.loads(manifestPath.read_text())
manifest['engine']['sha256']=signedSHA
manifest['privateModelSourceTypeFacts']=receipt
manifestPath.write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(dict(status=receipt['status'],sha256=signedSHA)))
