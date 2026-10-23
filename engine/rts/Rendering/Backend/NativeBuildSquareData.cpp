/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBuildSquareData.h"
#include "NativeBuildSquarePrograms.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

namespace rendering {
namespace {
void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
constexpr std::array<std::string_view,18> Names{
	"heightOffset","waterLevel","cellInset","cellSize","simplifiedReferenceCells","simplifiedSizeFalloff",
	"minimumScreenDiameter","cornerRadius","simplifiedOutlineScale","simplifiedCornerRadiusScale",
	"footprintBoundaryEnabled","footprintBoundaryWidth","showInvalidFootprintBoundary","alphaMultiplier",
	"invalidFootprintBoundaryColor","heightmapTex","isMiniMap","rotationMiniMap"};
std::string Substitute(std::string_view pattern,std::string_view engine)
{
	constexpr std::string_view marker="//__ENGINEUNIFORMBUFFERDEFS__";
	std::string text(pattern);const auto at=text.find(marker);
	Check(at!=text.npos && text.find(marker,at+marker.size())==text.npos,"Original BuildSquare engine marker differs");
	text.replace(at,marker.size(),engine);return text;
}
void ValidateBuffer(const NativeBufferSnapshotRef& b,bool instance)
{
	Check(b && b->object && b->revision,"Original BuildSquare buffer owner missing");const auto& l=b->layout;
	Check(l.kind==NativeBufferKind::Vertex && l.stride==(instance?72:8) && l.attributes.size()==(instance?5:1) && b->bytes.size()==CheckedNativeBufferByteSize(l),"Original BuildSquare VBO layout differs");
	for(unsigned i=0;i<l.attributes.size();++i){const auto& a=l.attributes[i];const auto count=instance&&i<4?4u:2u;
		Check(a.id==i+unsigned(instance) && a.offset==(instance?i*16:0) && a.byteSize==count*4 && a.declaredCount==count && a.scalarType==NativeScalarType::Float32 && a.valueShape==NativeValueShape::Scalar && !a.normalized,"Original BuildSquare attribute ABI differs");}
}
template<size_t N>void Copy(std::array<float,N>& out,const auto& bytes,size_t at){Check(at+N*4<=bytes.size(),"Original BuildSquare UBO range differs");std::memcpy(out.data(),bytes.data()+at,N*4);}
}
std::uint64_t NativeBuildSquareSourceHash(std::string_view text)noexcept{std::uint64_t h=14695981039346656037ULL;for(unsigned char c:text)h=(h^c)*1099511628211ULL;return h;}
NativeBuildSquareProgramState::NativeBuildSquareProgramState(std::string_view v,std::string_view f,std::string_view engine)
{
	Check(!engine.empty() && v==Substitute(buildSquareProgram::Vertex,engine) && f==Substitute(buildSquareProgram::Fragment,engine),"Original31657 BuildSquare shader identity differs");
}
int NativeBuildSquareProgramState::Location(std::string_view name)const{auto p=std::find(Names.begin(),Names.end(),name);return p==Names.end()?-1:int(p-Names.begin());}
bool NativeBuildSquareProgramState::Floats(int at,std::span<const float> v)
{
	if(at==-1)return true;
	if(at==14){if(v.size()!=4)return false;std::copy(v.begin(),v.end(),state.invalidFootprintBoundaryColor.begin());return true;}
	if(at<0||at>=14||v.size()!=1)return false;
	std::array<float*,14> slots{&state.heightOffset,&state.waterLevel,&state.cellInset,&state.cellSize,&state.simplifiedReferenceCells,&state.simplifiedSizeFalloff,&state.minimumScreenDiameter,&state.cornerRadius,&state.simplifiedOutlineScale,&state.simplifiedCornerRadiusScale,&state.footprintBoundaryEnabled,&state.footprintBoundaryWidth,&state.showInvalidFootprintBoundary,&state.alphaMultiplier};
	*slots[at]=v[0];return true;
}
bool NativeBuildSquareProgramState::Integers(int at,std::span<const std::int32_t> v)
{
	if(at==-1)return true;if(at<15||at>17||v.size()!=1)return false;
	(at==15?state.heightUnit:at==16?state.isMiniMap:state.rotationMiniMap)=v[0];return true;
}
NativeBuildSquareUniformBlock ReadNativeBuildSquareUniforms(const NativeBuildSquareDraw& d)
{
	Check(bool(d.original),"Original BuildSquare UBO owner missing");ValidateNativeCUSUniformPublication(*d.original);
	Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame && d.original->params->sourceDrawFrame<=d.sourceDrawFrame,"Original BuildSquare uses future uniforms");
	NativeBuildSquareUniformBlock u;Copy(u.cameraViewProjection,d.original->matrices->bytes,5*64);Copy(u.mapSize,d.original->params->bytes,48);Copy(u.viewGeometry,d.original->params->bytes,32);
	const auto& s=d.program;u.values0={s.heightOffset,s.waterLevel,s.cellInset,s.cellSize};u.values1={s.simplifiedReferenceCells,s.simplifiedSizeFalloff,s.minimumScreenDiameter,s.cornerRadius};
	u.values2={s.simplifiedOutlineScale,s.simplifiedCornerRadiusScale,s.footprintBoundaryEnabled,s.footprintBoundaryWidth};u.boundaryColor=s.invalidFootprintBoundaryColor;u.values3={s.showInvalidFootprintBoundary,s.alphaMultiplier,d.surface.alphaTest.reference,0};
	std::uint32_t caps=0;std::memcpy(&caps,d.original->params->bytes.data()+12,4);u.options={std::uint32_t(s.isMiniMap),std::uint32_t(s.rotationMiniMap),std::uint32_t(!(caps&1)),0};u.alpha={std::uint32_t(d.surface.alphaTest.enabled),std::uint32_t(d.surface.alphaTest.compare),0,0};return u;
}
void ValidateNativeBuildSquareDraw(const NativeBuildSquareDraw& d)
{
	ValidateBuffer(d.vertices,false);ValidateBuffer(d.instances,true);
	Check(d.firstVertex==0 && d.vertexCount==4 && d.vertices->layout.elements==4,"Original BuildSquare quad range differs");
	Check(d.firstInstance<=d.instances->layout.elements && d.instanceCount<=d.instances->layout.elements-d.firstInstance,"Original BuildSquare instance range exceeds producer");
	constexpr std::array<float,8> corners{0,0,8,0,0,8,8,8};Check(std::memcmp(d.vertices->bytes.data(),corners.data(),sizeof(corners))==0,"Original BuildSquare triangle-strip corners differ");
	Check(d.program.cellSize==8 && d.program.heightUnit==0 && (d.program.isMiniMap==0||d.program.isMiniMap==1) && d.program.rotationMiniMap>=0 && d.program.rotationMiniMap<4,"Original BuildSquare units/rotation differ");
	Check(d.stage==NativeWorldDrawStage::PreUnit || d.stage==NativeWorldDrawStage::World,"Original BuildSquare callback stage differs");
	if(!d.program.isMiniMap)Check(d.height.knowledge==NativeCUSBindingKnowledge::BoundObject && d.height.object && d.height.object->identity && d.height.object->version && d.height.object->target==NativeCUSTextureTarget::Image2D && d.height.object->storage!=NativeTextureStorageKnowledge::Unobserved,"Original BuildSquare real height binding missing");
	Check(!d.raster.inheritedClipPlane && d.raster.frontPolygonMode==NativePolygonMode::Fill && d.raster.backPolygonMode==NativePolygonMode::Fill,"Original BuildSquare raster outside source scope");
	(void)ReadNativeBuildSquareUniforms(d);
}
}
