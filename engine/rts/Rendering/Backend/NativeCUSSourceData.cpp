/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCUSSourceData.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace rendering {
void SetNativeCUSBlend(NativeCUSSourceDraw& d,unsigned source,unsigned destination)
{
	const auto factor=[](unsigned x){switch(x){
		case 0:return ScreenBlendFactor::Zero;case 1:return ScreenBlendFactor::One;
		case 0x300:return ScreenBlendFactor::SourceColor;case 0x301:return ScreenBlendFactor::OneMinusSourceColor;
		case 0x302:return ScreenBlendFactor::SourceAlpha;case 0x303:return ScreenBlendFactor::OneMinusSourceAlpha;
		case 0x304:return ScreenBlendFactor::DestinationAlpha;case 0x305:return ScreenBlendFactor::OneMinusDestinationAlpha;
		case 0x306:return ScreenBlendFactor::DestinationColor;case 0x307:return ScreenBlendFactor::OneMinusDestinationColor;
		case 0x308:return ScreenBlendFactor::SourceAlphaSaturate;
	}throw std::runtime_error("CUS original blend factor differs");};
	auto b=d.surface.blend;b.sourceColor=b.sourceAlpha=factor(source);b.destinationColor=b.destinationAlpha=factor(destination);b.enabled=true;
	d.sourceBlend=source;d.destinationBlend=destination;d.surface.blend=b;
}
bool HasNoNativeCUSAttachmentEffects(const NativeCUSSourceDraw& d) noexcept
{
	return d.rasterObserved&&d.depthFunction>=0x200&&d.depthFunction<=0x207&&
		(d.cullFace==0x404||d.cullFace==0x405||d.cullFace==0x408)&&
		!d.world.stencilTest&&!(d.depthTest&&d.depthWrite)&&
		std::none_of(d.colorMask.begin(),d.colorMask.end(),[](bool value){return value;});
}
void ValidateNativeCUSSourceDraw(const NativeCUSSourceDraw& d)
{
	const auto fail=[](const char* message){throw std::runtime_error(message);};
	if(!d.program||!d.program->pipelineLifetime||!d.textures||d.objects.empty()||!d.instances||!d.modelInputs||!d.engineUniforms)
		fail("CUS source draw lacks its compiled program, original submission, or instance data");
	ValidateNativeCUSUniformPublication(*d.engineUniforms);
	if(d.program->vertexSHA512.empty()||d.program->fragmentSHA512.empty())
		fail("CUS source draw has no paired original program identity");
	if(d.textures->originalSubmissionOrder.size()!=d.objects.size()||d.textures->originalShader!=d.program->family||
	   d.textures->originalPass!=d.program->originalPass)
		fail("CUS original program/pass/submission identities differ");
	for(size_t i=0;i<d.objects.size();++i)if(d.objects[i].kind!=d.textures->originalSubmissionOrder[i].kind||d.objects[i].id!=d.textures->originalSubmissionOrder[i].id)
		fail("CUS original VAO submission order differs from captured history");
	if(d.submissions.size()!=d.objects.size()||d.modelInputs->submissions!=d.submissions||
	   d.modelInputs->originalInstances!=d.instances||d.modelInputs->sourceDrawFrame!=d.textures->sourceDrawFrame||
	   d.modelInputs->sourceSequence!=d.sequence)
		fail("CUS original model/pose publication is not paired with this actual submission");
	for(size_t i=0;i<d.objects.size();++i)if(!d.submissions[i]||d.submissions[i]->object.kind!=d.objects[i].kind||d.submissions[i]->object.id!=d.objects[i].id)
		fail("CUS original indirect model owner differs from the submitted object");
	const auto& layout=d.instances->layout;
	if(layout.kind!=NativeBufferKind::Vertex||layout.stride!=16||layout.attributes.size()!=1||layout.elements<d.objects.size())
		fail("CUS original instData layout/capacity differs");
	const auto& a=layout.attributes[0];
	if(a.id!=6||a.scalarType!=NativeScalarType::UInt32||a.offset!=0||a.byteSize!=16||a.normalized||a.declaredCount!=4||a.valueShape!=NativeValueShape::Scalar)
		fail("CUS source requires its actual uvec4 attribute 6");
	if(d.instances->bytes.size()!=CheckedNativeBufferByteSize(layout))fail("CUS immutable instance payload is incomplete");
	const auto pass=d.intUniforms.find("drawPass");
	// Original shadow variants compile this uniform out. LuaShader correctly
	// skips their missing location; the source program identity still fixes16.
	const bool activePass=std::any_of(d.program->uniforms.begin(),d.program->uniforms.end(),[](const auto& uniform){return uniform.name=="drawPass";});
	if(activePass&&(pass==d.intUniforms.end()||pass->second.size()!=1||pass->second[0]!=d.program->originalPass))
		fail("CUS original drawPass uniform disagrees with its compiled program");
	for(const auto& table:{&d.floatUniforms,&d.intUniforms})for(const auto& [name,values]:*table){
		if(name.empty()||values.empty())fail("CUS source uniform has no name/value");
		for(float v:values)if(!std::isfinite(v))fail("CUS source uniform is non-finite");
	}
}
} // namespace rendering
