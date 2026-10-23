/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDistortionData.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value, const char* message)
{
	if (!value) throw std::runtime_error(message);
}
void Snapshot(const NativeBufferSnapshotRef& value, NativeBufferKind kind)
{
	Check(value && value->object && value->revision && value->layout.kind == kind,
		"Distortion draw lacks an actual immutable VBO generation");
	Check(value->bytes.size() == CheckedNativeBufferByteSize(value->layout),
		"Distortion VBO backing does not match its original definition");
}
uint32_t UInt(const NativeBufferSnapshot& buffer, size_t record, size_t offset)
{
	uint32_t value;
	std::memcpy(&value, buffer.bytes.data() + record * buffer.layout.stride + offset, sizeof(value));
	return value;
}
// Called after ValidateNativeDistortionDraw has proved the packed layout/range.
// Type11 reads model speed even when the effect is not attached to a unit.
void ValidateModelSpeedReads(const NativeDistortionDraw& draw, uint64_t sourceDrawFrame)
{
	for (size_t i = draw.firstInstance; i < size_t(draw.firstInstance) + draw.instanceCount; ++i) {
		float effectType;
		std::memcpy(&effectType, draw.instances->bytes.data() + i * draw.instances->layout.stride + 92, sizeof(effectType));
		if (std::round(effectType) != 11.f) continue;
		const auto& model = draw.modelUniforms;
		Check(model && model->kind == NativeCUSPublishedKind::ModelUniforms && model->stride == 128 &&
			model->contextGeneration && model->allocationSerial && model->publicationSerial &&
			model->publishedDrawFrame <= sourceDrawFrame,
			"Distortion type11 lacks its actual captured model uniform publication");
		const uint32_t uniform = UInt(*draw.instances, i, 104);
		Check(NativeCUSPublishedElementObserved(*model, uniform),
			"Distortion type11 references an unobserved original model speed element");
	}
}
float Uniform(const NativeDistortionDraw& draw, const char* name)
{
	const auto item = draw.floatUniforms.find(name);
	Check(item != draw.floatUniforms.end() && item->second.size() == 1,
		"Distortion draw lacks an actual scalar uniform value");
	return item->second[0];
}
}
void ValidateNativeDistortionProgramSource(const NativeDistortionProgramSource& source)
{
 Check((source.name=="Deferred Distortions GL4"||source.name=="ScreenDistortionShader GL4")&&!source.vertex.empty()&&!source.fragment.empty(),"Original Distortion source/program identity missing");
 Check(!source.assembledVertex.empty()&&!source.assembledFragment.empty()&&!source.engineDefinitions.empty()&&!source.quaternionDefinitions.empty(),"Original Distortion helper assembly missing");
 const std::set<std::string,std::less<>> names{"VOIDWATER","CHROMATIC_ABERRATION","DEBUGCOMBINER","HEAT_LEGACY_DOUBLE_FALLOFF","USEQUATERNIONS","DEPTH_CLIP01","UNIFORMSBUFFERCOPY"};
 for(const auto&[name,value]:source.defines){
  Check(names.contains(name),"Unselected original Distortion compile define");
  if(const auto number=std::get_if<float>(&value))Check(std::isfinite(*number),"Nonfinite Distortion selector");
  else Check((name=="USEQUATERNIONS"||name=="DEPTH_CLIP01")&&(std::get<std::string>(value)=="0"||std::get<std::string>(value)=="1"),"Invalid Distortion textual selector");
 }
 for(const auto& name:names)Check(name=="DEPTH_CLIP01"||name=="UNIFORMSBUFFERCOPY"||source.defines.contains(name),"Missing original Distortion selector");
}
void ValidateNativeDistortionDraw(const NativeDistortionDraw& draw)
{
	Check(draw.sequence && draw.program && draw.program->kind==NativeDistortionProgram::Volume && draw.program->pipelineLifetime,
		"Distortion draw has no compiled original source owner or ordinal");
	Check(draw.mode == 0x0004 && draw.count && draw.instanceCount,
		"Original distortion instanced triangle draw parameters differ");
	Snapshot(draw.vertices, NativeBufferKind::Vertex);
	Snapshot(draw.instances, NativeBufferKind::Vertex);
	const auto& layout = draw.instances->layout;
	Check(layout.stride == 116 && layout.attributes.size() == 8,
		"Original distortion instance record is not the actual 116-byte layout");
	for (unsigned i = 0; i < 8; ++i) {
		const auto field = std::find_if(layout.attributes.begin(), layout.attributes.end(), [i](const auto& a) { return a.id == i + 3; });
		const size_t offset = i < 6 ? 16 * i : (i == 6 ? 96 : 100);
		const size_t bytes = i == 6 ? 4 : 16;
		Check(field != layout.attributes.end() && field->offset == offset && field->byteSize == bytes &&
			field->declaredCount == (i == 6 ? 1 : 4) && !field->normalized &&
			field->scalarType == (i < 6 ? NativeScalarType::Float32 : NativeScalarType::UInt32),
			"Original distortion instance attribute type or offset differs");
	}
	Check(draw.firstInstance <= layout.elements && draw.instanceCount <= layout.elements - draw.firstInstance,
		"Distortion instance range exceeds original captured backing");
	if (draw.indexed) {
		Snapshot(draw.indices, NativeBufferKind::Index);
		const auto scalar = draw.indices->layout.attributes.front().scalarType;
		const auto type = scalar == NativeScalarType::UInt8 ? 0x1401u : scalar == NativeScalarType::UInt16 ? 0x1403u : 0x1405u;
		Check(draw.indexType == type && draw.firstIndex <= draw.indices->layout.elements &&
			draw.count <= draw.indices->layout.elements - draw.firstIndex,
			"Distortion index type or range differs from original VBO");
		for (size_t i = draw.firstIndex; i < size_t(draw.firstIndex) + draw.count; ++i) {
			uint32_t index = 0;
			std::memcpy(&index, draw.indices->bytes.data() + i * draw.indices->layout.stride, draw.indices->layout.stride);
			const uint32_t restart = type == 0x1401u ? 0xffu : type == 0x1403u ? 0xffffu : 0xffffffffu;
			if (index == restart) continue; // Original DrawElements enables fixed maximum restart index.
			Check(draw.baseVertex >= 0 && uint64_t(index) + uint32_t(draw.baseVertex) < draw.vertices->layout.elements,
				"Distortion index references outside original captured vertices");
		}
	} else {
		Check(!draw.indices && draw.firstVertex <= draw.vertices->layout.elements &&
			draw.count <= draw.vertices->layout.elements - draw.firstVertex,
			"Distortion DrawArrays range exceeds original captured vertices");
	}
	Check(bool(draw.engineUniforms), "Distortion draw lacks original engine UBO publications");
	ValidateNativeCUSUniformPublication(*draw.engineUniforms);
	if (draw.program->uniformBufferCopy) {
  const auto& copy=draw.uniformBufferCopy;
  Snapshot(copy.snapshot,NativeBufferKind::Storage);
  Check(copy.binding==4&&copy.snapshot->layout.stride==128&&copy.offset%128==0&&
   copy.offset<=copy.snapshot->bytes.size()&&copy.size<=copy.snapshot->bytes.size()-copy.offset,
   "Distortion original copy binding range differs");
  if(Uniform(draw,"attachedtounitID")>0)for(size_t i=draw.firstInstance;i<size_t(draw.firstInstance)+draw.instanceCount;++i)
   Check(uint64_t(UInt(*draw.instances,i,104))*128+128<=copy.size,"Distortion copy uniform index exceeds actual bound range");
 }
	if (Uniform(draw, "attachedtounitID") > 0) {
		Check(draw.transforms && draw.modelUniforms && draw.transforms->stride && draw.modelUniforms->stride == 128,
			"Unit-attached distortion lacks original transform or model uniform publications");
		for (size_t i = draw.firstInstance; i < size_t(draw.firstInstance) + draw.instanceCount; ++i) {
			const uint32_t transform = UInt(*draw.instances, i, 100), uniform = UInt(*draw.instances, i, 104);
			const uint32_t piece = UInt(*draw.instances, i, 96);
			Check(NativeCUSPublishedElementObserved(*draw.modelUniforms, uniform) &&
				NativeCUSPublishedElementObserved(*draw.transforms, transform),
				"Unit-attached distortion references an unobserved original engine element");
			if (draw.transforms->stride == 32) {
				Check(NativeCUSPublishedElementObserved(*draw.transforms, uint64_t(transform) + 1),
					"Unit distortion interpolation lacks its second original world transform");
				if (piece) {
					const uint64_t index = uint64_t(transform) + 2 * uint64_t(piece);
					Check(NativeCUSPublishedElementObserved(*draw.transforms, index) && NativeCUSPublishedElementObserved(*draw.transforms, index + 1),
						"Unit distortion interpolation lacks its original piece transform pair");
				}
			} else {
				Check(draw.transforms->stride == 64 && (!piece || NativeCUSPublishedElementObserved(*draw.transforms, uint64_t(transform) + piece)),
					"Unit distortion matrix path lacks its original piece matrix");
			}

		}
	}
}
void ValidateNativeDistortionCommand(const NativeDistortionCommand& command)
{
 Check(command.sequence!=0,"Distortion command lost its common source ordinal");
 const auto version=[&](const std::shared_ptr<const NativeDistortionVersion>& value){
  Check(value&&value->image&&value->image->identity&&value->image->width&&value->image->height&&value->generation&&value->producerSequence&&value->sourceDrawFrame==command.sourceDrawFrame,"Distortion image is not this source draw's actual immutable version");
 };
 if(command.output){version(command.output);Check(command.output->producerSequence==command.sequence,"Distortion write ordinal differs");}
 if(command.previous){version(command.previous);Check(command.output&&command.previous->image==command.output->image&&command.previous->generation<command.output->generation,"Distortion load lost its previous target image");}
 for(const auto& input:command.inputs)if(input)version(input);
 if(command.kind==NativeDistortionOperation::Volume||command.kind==NativeDistortionOperation::Combine){
  Check(command.draw&&command.draw->program&&command.draw->program->pipelineLifetime&&command.draw->sequence==command.sequence,"Distortion draw lost actual program or buffer capture");
  if(command.kind==NativeDistortionOperation::Volume){Check(command.output&&command.previous&&command.output->image->format==NativeDistortionFormat::RGBA16Float,"Distortion volume has no cleared actual RGBA16F target");ValidateNativeDistortionDraw(*command.draw);ValidateModelSpeedReads(*command.draw,command.sourceDrawFrame);}
  else {
   Check(!command.output&&command.inputs[0]&&command.inputs[1]&&command.inputs[0]->image->format==NativeDistortionFormat::RGBA8&&command.inputs[1]->image->format==NativeDistortionFormat::RGBA16Float,"Distortion combine lacks original screen-copy/offset inputs");
   Check(command.draw->program->kind==NativeDistortionProgram::Combine&&command.draw->vertices&&!command.draw->instances&&!command.draw->indexed&&command.draw->mode==0x0004&&command.draw->count==6,"Distortion combine is not the actual original TexRect VAO");
   Snapshot(command.draw->vertices,NativeBufferKind::Vertex);
   Check(command.draw->firstVertex<=command.draw->vertices->layout.elements&&command.draw->count<=command.draw->vertices->layout.elements-command.draw->firstVertex,"Distortion combine exceeds actual original vertices");
  }
 }else if(command.kind==NativeDistortionOperation::Clear){
  Check(command.output&&!command.draw&&command.output->image->format==NativeDistortionFormat::RGBA16Float,"Distortion clear target differs");
  for(float x:command.clearColor)Check(std::isfinite(x),"Nonfinite original Distortion clear value");
 }else {
  Check(command.kind==NativeDistortionOperation::CopyScreen&&command.output&&!command.draw&&command.output->image->format==NativeDistortionFormat::RGBA8,"Distortion screen copy target differs");
  Check(command.copySource[2]>0&&command.copySource[3]>0&&command.copyDestination[0]>=0&&command.copyDestination[1]>=0&&command.copyDestination[2]==command.copySource[2]&&command.copyDestination[3]==command.copySource[3],"Distortion copy interval differs");
 }
}
} // namespace rendering
