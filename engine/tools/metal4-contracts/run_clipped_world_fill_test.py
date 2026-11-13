#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile,hashlib
root=Path(__file__).resolve().parents[2]
source=root/'rts/Rendering/Backend/NativeWorldDrawGeometry.cpp'
s=source.read_text()
helpers=s[s.index('NativeWorldDrawVertex Mix('):s.index('bool ClipUserPlane(')]
helpers+=s[s.index('std::vector<NativeWorldDrawVertex> ClipUserPolygon('):s.index('bool Clip(',s.index('std::vector<NativeWorldDrawVertex> ClipUserPolygon('))]
assert hashlib.sha256(helpers.encode()).hexdigest()=='320e05c4ee267583aaa37957e5ee2c3ba511c5e7040e9c4bd6661acb01f79ad8'
# Frozen reference is the original pre-extraction fill loop, not a call to the new kernel.
reference='''NativeWorldRasterMesh Reference(const NativeWorldDrawCommand& d){
 NativeWorldRasterMesh result;
 for(size_t i=0;i<d.indices.size();i+=3) {
  const auto polygon=ClipUserPolygon({d.vertices[d.indices[i]],d.vertices[d.indices[i+1]],d.vertices[d.indices[i+2]]},*d.world.inheritedClipPlane);
  if(polygon.size()<3)continue;
  const auto first=uint32_t(result.vertices.size());
  result.vertices.insert(result.vertices.end(),polygon.begin(),polygon.end());
  for(uint32_t j=1;j+1<polygon.size();++j)result.indices.insert(result.indices.end(),{first,first+j,first+j+1});
 }
 return result;
}
'''
# Full original caller prefix is retained before the real kernel invocation.
prefix=s[s.index('static NativeWorldRasterMesh BuildGeometry('):s.index('return BuildValidatedNativeWorldClippedFill(d.vertices')]
assert hashlib.sha256(prefix.encode()).hexdigest()=='9f24ee4ae9c992957a185d762ef6f6b86d3315ab64e5db7dbf4399bbbe7de8d0'
assert prefix.index('ValidateNativeWorldDrawCommand(d)')<prefix.index('Require(samples')<prefix.index('for(double value:viewport)')<prefix.index('if(d.world.inheritedClipPlane)for(float value:')<prefix.index('if(d.primitive==NativeDrawPrimitive::Triangles)')<prefix.index('if(!d.world.inheritedClipPlane)return {d.vertices,d.indices};')
with tempfile.TemporaryDirectory(prefix='bar-clipped-fill-') as path:
 out=Path(path);(out/'clipped-world-fill.inc').write_text('namespace rendering { namespace oldReference {\n'+helpers+reference+'}}\nusing rendering::oldReference::Reference;\n')
 subprocess.run(['g++','-std=c++23','-O1','-g','-ffunction-sections','-fdata-sections','-fsanitize=address,undefined','-I'+str(root/'rts'),'-I'+str(out),str(Path(__file__).with_name('TestClippedWorldFill.cpp')),str(source),'-Wl,--gc-sections','-o',str(out/'test')],check=True)
 subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Actual production CPP clipped-fill kernel linked: 4000 byte-identical reference cases, degenerate/boundary/perspective/empty inputs and allocation-order fault parity PASS. No app or Metal execution.')
