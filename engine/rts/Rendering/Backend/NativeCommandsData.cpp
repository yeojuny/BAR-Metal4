/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeCommandsData.h"
#include "NativeCommandsPrograms.h"
#include <cmath>
#include <stdexcept>
namespace rendering {
namespace {
bool ready=false;
void Check(bool value,const char* why){if(!value)throw std::invalid_argument(why);}
constexpr std::array<std::string_view,6> Names{"u_viewMat","u_projMat","u_lineTexLen","u_lineWidth","u_tex","u_useTex"};
void Buffer(const NativeBufferSnapshotRef& source,unsigned stride,unsigned first,unsigned attributes,unsigned count)
{
	Check(source && source->object && source->revision && source->bytes.size()==CheckedNativeBufferByteSize(source->layout),"Original Commands FX buffer owner differs");
	const auto& layout=source->layout;Check(layout.kind==NativeBufferKind::Vertex && layout.stride==stride && layout.attributes.size()==attributes,"Original Commands FX buffer layout differs");
	for(unsigned i=0;i<attributes;++i){const auto& a=layout.attributes[i];Check(a.id==first+i && a.offset==i*count*4 && a.scalarType==NativeScalarType::Float32 && a.valueShape==NativeValueShape::Scalar && a.declaredCount==count && a.byteSize==count*4 && !a.normalized,"Original Commands FX vertex/instance attribute differs");}
}
}
std::uint64_t NativeCommandsSourceHash(std::string_view source)noexcept{std::uint64_t h=14695981039346656037ULL;for(unsigned char c:source)h=(h^c)*1099511628211ULL;return h;}
NativeCommandsProgramState::NativeCommandsProgramState(std::string_view vertex,std::string_view fragment)
{
	Check(NativeCommandsSourceHash(vertex)==commandsProgram::VertexHash && NativeCommandsSourceHash(fragment)==commandsProgram::FragmentHash,"Commands FX shader differs from exact30770");
}
int NativeCommandsProgramState::Location(std::string_view name)const{auto i=std::find(Names.begin(),Names.end(),name);return i==Names.end()?-1:int(i-Names.begin());}
bool NativeCommandsProgramState::Floats(int location,std::span<const float> values)
{
	if(location==-1)return true;if((location!=2 && location!=3)||values.size()!=1)return false;
	(location==2?state.lineTextureLength:state.lineWidth)=values[0];return true;
}
bool NativeCommandsProgramState::Integers(int location,std::span<const std::int32_t> values)
{
	if(location==-1)return true;if((location!=4 && location!=5)||values.size()!=1)return false;
	(location==4?state.textureUnit:state.useTexture)=values[0];return true;
}
bool NativeCommandsProgramState::Matrix(int location,std::span<const float> values)
{
	if(location==-1)return true;if((location!=0 && location!=1)||values.size()!=16)return false;
	auto& out=location==0?state.view:state.projection;std::copy(values.begin(),values.end(),out.begin());return true;
}
NativeCommandsUniformBlock ReadNativeCommandsUniforms(const NativeCommandsDraw& draw)
{
	Check(bool(draw.original),"Original Commands FX clip capability publication missing");ValidateNativeCUSUniformPublication(*draw.original);
	Check(draw.original->params->sourceDrawFrame<=draw.sourceDrawFrame,"Original Commands FX clip capability is from the future");
	NativeCommandsUniformBlock u;u.view=draw.program.view;u.projection=draw.program.projection;
	u.values={draw.program.lineTextureLength,draw.program.lineWidth,draw.surface.alphaTest.reference,0};
	std::uint32_t capabilities=0;std::memcpy(&capabilities,draw.original->params->bytes.data()+12,4);
	u.options={std::uint32_t(draw.program.useTexture!=0),0,std::uint32_t(!(capabilities&1u)),std::uint32_t(draw.surface.alphaTest.compare)|(std::uint32_t(draw.surface.alphaTest.enabled)<<8)};
	return u;
}
void ValidateNativeCommandsDraw(const NativeCommandsDraw& draw)
{
	Buffer(draw.vertices,8,0,1,2);Buffer(draw.instances,48,1,3,4);
	Check(draw.firstVertex<=draw.vertices->layout.elements && draw.vertexCount<=draw.vertices->layout.elements-draw.firstVertex && draw.firstInstance<=draw.instances->layout.elements && draw.instanceCount<=draw.instances->layout.elements-draw.firstInstance,"Original Commands FX draw exceeds its uploaded owner");
	Check(draw.vertexCount==4 && draw.firstVertex==0,"Original Commands FX selected quad draw differs");
	for(const auto& m:{draw.program.view,draw.program.projection})for(float f:m)Check(std::isfinite(f),"Original Commands FX matrix is nonfinite");
	Check(draw.program.textureUnit>=0 && draw.program.textureUnit<int(NativeCUSTextureHistory::Units),"Original Commands FX texture unit differs");
	(void)ReadNativeCommandsUniforms(draw);
}
void SetNativeCommandsConsumerAvailable(bool value)noexcept{ready=value;}
bool IsNativeCommandsConsumerAvailable()noexcept{return ready;}
}
