#!/usr/bin/env python3
from pathlib import Path
import os,subprocess,tempfile,hashlib,re
root=Path(__file__).resolve().parents[2]
s=(root/'rts/Rendering/Metal4/Metal4EngineShadow.mm').read_text()
a=s[s.index('void Rigid('):s.index('Uniform Snapshot(')]
fast=' // Capture classified these exact immutable vertices before publication. Keep\n // per-draw pose/format guards and the original successful-set insertion.\n if(draw.object->model->gl4BoneLayout==WorldModelGL4BoneLayout::RigidPerPiece){validated.insert(draw.object->model.get());return;}\n'
assert a.count(fast)==1
reference=a.replace(fast,'').replace('void Rigid(','void ReferenceRigid(')
assert hashlib.sha256(reference.encode()).hexdigest()=='0d13befb7596dd30eddff6dbf89bf2d9009226c1ae9ebedd5024de29acd5665c'
assert a.index('Require(')<a.index('validated.contains')<a.index('gl4BoneLayout')<a.index('for (size_t p')
capture=(root/'rts/Rendering/WorldFrameCapture.cpp').read_text()
start=capture.index('if (piece->HasGeometryData() && (value.boneIds[0]')
end=capture.index('rigidPerPiece = false;',start)+len('rigidPerPiece = false;')
predicate=capture[start:end]
# Source provenance gates: copied indices use the same HasGeometryData backing.
piece=(root/'rts/Rendering/Models/3DModelPiece.hpp').read_text()
assert 'bool HasGeometryData() const { return indices.size() >= 3; }' in piece
assert 'const std::vector<uint32_t>& GetIndicesVec() const { return indices; }' in piece
assert 'std::unordered_map<const S3DModel*, std::shared_ptr<const WorldModel>> models;' in capture
model=capture[capture.index('std::shared_ptr<const WorldModel> Model('):capture.index('std::shared_ptr<const WorldTerrainMaterial> TerrainMaterial(')]
assert model.index('CModelsLock::GetScopedLock()')<model.index('for (const SVertexData& vertex')<model.index('output.indices = piece->GetIndicesVec()')<model.index('model->gl4BoneLayout =')<model.index('models.emplace(source, model)')
assert model.index('output.indices = piece->GetIndicesVec()')<model.index('if (index >= output.vertices.size())')<model.index('model->gl4BoneLayout =')
# New production creators/classification writers require another provenance audit.
creators=[];writers=[]
for p in (root/'rts').rglob('*'):
 if p.suffix not in ('.h','.hpp','.cpp','.mm'):continue
 text=p.read_text(errors='replace')
 if re.search(r'make_shared\s*<\s*WorldModel\s*>',text):creators.append(p.relative_to(root).as_posix())
 if re.search(r'gl4BoneLayout\s*=(?!=)',text):writers.append(p.relative_to(root).as_posix())
assert creators==['rts/Rendering/WorldFrameCapture.cpp'],creators
assert set(writers)=={'rts/Rendering/WorldFrameCapture.cpp','rts/Rendering/Backend/WorldFrameData.h'},writers
classifier='WorldModelGL4BoneLayout CaptureClassification(const WorldModel& input){auto model=std::make_shared<WorldModel>();bool rigidPerPiece=true;for(const auto& entry:input.pieces){const auto* piece=&entry;for(const auto& value:entry.vertices){'+predicate+'}model->pieces.push_back(entry);}return rigidPerPiece?WorldModelGL4BoneLayout::RigidPerPiece:WorldModelGL4BoneLayout::Unsupported;}\n'
with tempfile.TemporaryDirectory(prefix='bar-shadow-rigid-') as path:
 out=Path(path);loop='const auto& v = piece.vertices[vertex];'
 assert a.count(loop)==1 and reference.count(loop)==1
 (out/'shadow-rigid.inc').write_text(a.replace(loop,'++fastVisited;'+loop)+reference.replace(loop,'++referenceVisited;'+loop));(out/'capture-rigid.inc').write_text(classifier)
 subprocess.run(['g++','-std=c++23','-O1','-g','-fsanitize=address,undefined','-I'+str(out),str(Path(__file__).with_name('TestShadowRigidProof.cpp')),'-o',str(out/'test')],check=True)
 subprocess.run([str(out/'test')],env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},check=True)
print('Shadow Rigid production predicate + capture classification: 4000 reference cycles, emitter/unreferenced vertices, guards, fallback diagnostics, insertion fault and fabricated-proof caveat PASS; no Metal/app execution')
