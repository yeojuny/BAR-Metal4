/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativePlacementData.h"
#include "NativeS3OTextureSource.h"
#include "NativePlacementIdentity.h"
#include "Rendering/Backend/NativeWorldDrawRecorder.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>

namespace rendering {
namespace {
std::atomic<bool> gridAvailable{false},samplesAvailable{false};
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class R> void Finite(const R& values){for(auto value:values)Check(std::isfinite(value),"Nonfinite original building placement input");}
uint64_t Hash(std::string_view source){uint64_t h=14695981039346656037ULL;for(unsigned char c:source)h=(h^c)*1099511628211ULL;return h;}
void Replace(std::string& source,std::string_view expanded,std::string_view marker){Check(!expanded.empty(),"Original Building Grid expansion missing");auto p=source.find(expanded);Check(p!=std::string::npos&&source.find(expanded,p+expanded.size())==std::string::npos,"Original Building Grid expansion changed");source.replace(p,expanded.size(),marker);}
float Smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
float Distance(std::array<float,3> a,std::array<float,3> b){float d=0;for(unsigned i=0;i<3;++i)d+=(a[i]-b[i])*(a[i]-b[i]);return std::sqrt(d);}
}
void SetNativePlacementGridAvailable(bool value)noexcept{gridAvailable=value;}
void SetNativePlacementSamplesAvailable(bool value)noexcept{samplesAvailable=value;}
bool IsNativePlacementAvailable()noexcept{return gridAvailable&&samplesAvailable;}
void ValidateNativeBuildingGrid(const NativeBuildingGridDraw& d){
	Check(d.stage==NativeWorldDrawStage::PreUnit,"Original Building Grid must be in DrawWorldPreUnit");
	Check(d.vertices&&d.heights,"Original Building Grid resource missing");
	const auto& layout=d.vertices->layout;Check(d.vertices->object&&d.vertices->revision&&CheckedNativeBufferByteSize(layout)==d.vertices->bytes.size(),"Original Building Grid storage differs");
	const unsigned scalarBytes=d.generatedPatch31657?4:12;
	Check(layout.kind==NativeBufferKind::Vertex&&layout.stride==scalarBytes&&layout.attributes.size()==1,"Original Building Grid vertex layout differs");
	const auto& a=layout.attributes[0];Check(a.id==0&&a.scalarType==NativeScalarType::Float32&&a.offset==0&&a.byteSize==scalarBytes&&!a.normalized,"Original Building Grid position attribute differs");
	if(d.generatedPatch31657){
		Check(layout.elements==2208&&d.firstVertex%2==0,"Pinned31657 Building Grid patch dimensions differ");
		Check(d.unusedIndices&&d.unusedInstances,"Pinned31657 Building Grid source attachments missing");
		const auto& index=*d.unusedIndices;const auto& instance=*d.unusedInstances;
		// Original indexVBO:Define(1) omits the type: engine01ef defaults to
		// GL_UNSIGNED_SHORT. Retain that real attachment, not a widened copy.
		Check(index.object&&index.revision&&index.layout.kind==NativeBufferKind::Index&&index.layout.elements==1&&
			index.layout.stride==sizeof(std::uint16_t)&&index.layout.attributes.size()==1&&CheckedNativeBufferByteSize(index.layout)==index.bytes.size(),
			"Pinned31657 Building Grid index attachment differs");
		const auto& indexAttribute=index.layout.attributes[0];
		Check(indexAttribute.id==0&&indexAttribute.scalarType==NativeScalarType::UInt16&&indexAttribute.valueShape==NativeValueShape::Scalar&&
			indexAttribute.declaredCount==1&&indexAttribute.offset==0&&indexAttribute.byteSize==sizeof(std::uint16_t)&&!indexAttribute.normalized,
			"Pinned31657 Building Grid index scalar declaration differs");
		std::uint16_t uploadedIndex;std::memcpy(&uploadedIndex,index.bytes.data(),sizeof(uploadedIndex));
		Check(uploadedIndex==0,"Pinned31657 Building Grid original index upload differs");
		Check(instance.object&&instance.revision&&instance.layout.kind==NativeBufferKind::Vertex&&instance.layout.elements==1&&instance.layout.stride==4&&instance.layout.attributes.size()==1&&CheckedNativeBufferByteSize(instance.layout)==instance.bytes.size(),"Pinned31657 Building Grid instance attachment differs");
		const auto& attribute=instance.layout.attributes[0];Check(attribute.id==1&&attribute.scalarType==NativeScalarType::Float32&&attribute.offset==0&&attribute.byteSize==4&&!attribute.normalized,"Pinned31657 Building Grid unused instance layout differs");
		Check(std::isfinite(d.cameraFade)&&d.cameraFade>=0&&d.cameraFade<1,"Pinned31657 Building Grid camera fade differs");Finite(d.lineParameters);
	}
	Check(d.firstVertex<=layout.elements&&d.vertexCount<=layout.elements-d.firstVertex&&d.vertexCount%2==0,"Original Building Grid range differs");
	Check(d.heights->width>=3&&d.heights->height>=3&&d.heights->corners.size()==size_t(d.heights->width)*d.heights->height,"Original Building Grid height storage differs");
	Check(d.mapWorldSize[0]==8.f*(d.heights->width-1)&&d.mapWorldSize[1]==8.f*(d.heights->height-1),"Original Building Grid map/height dimensions differ");
	Check(d.waterSurfaceMode==0||d.waterSurfaceMode==1,"Original Building Grid water mode differs");
	Check(d.view.clipDepth!=ClipDepthConvention::Unsupported&&std::isfinite(d.waterLevel),"Original Building Grid view/water invalid");
	Finite(d.view.viewProjection);Finite(d.view.cameraPosition);Finite(d.mousePosition);
}
void ValidateNativeBuildingSample(const NativeBuildingSample& d){
	Check(d.model&&((d.modelTextures&&NativeS3OTexturePixels(*d.modelTextures,0)&&NativeS3OTexturePixels(*d.modelTextures,1))||(!d.modelTextures&&d.model->textures[0]&&d.model->textures[1])),"Original building sample model/material missing");
	Check(std::isfinite(d.alpha)&&d.alpha>=.11f&&d.alpha<=1,"Original building sample alpha differs");
	Finite(d.world);Finite(d.legacyCameraPosition);
}
void ValidateNativeBuildingGridProgram(std::string_view vertex,std::string_view fragment,std::string_view engine,std::string_view defines,bool game31657,std::array<float,2> mapWorldSize){
	std::map<std::string,std::string> values;std::istringstream lines{std::string(defines)};std::string line;
	while(std::getline(lines,line)){std::istringstream words(line);std::string directive,name,value;Check(bool(words>>directive>>name)&&directive=="#define","Original Building Grid define malformed");std::getline(words,value);value.erase(0,value.find_first_not_of(" \t"));Check(values.emplace(name,value).second,"Original Building Grid define repeated");}
	if(game31657){
		Check(Hash(engine)==NativeBuildingGridEngineHash31657,"Pinned31657 Building Grid engine definitions differ");
		Check(values.size()==11&&(values["LINECOLOR"]=="vec3(0.69999999, 1, 0.69999999)"||values["LINECOLOR"]=="vec3(0.7, 1, 0.7)")&&values["GRIDRADIUS"]=="30"&&values["RADIUSFALLOFF"]=="2.5"&&values["SPACING"]=="48"&&values["STRONG_LINE_SPACING"]=="4"&&values["PATCH_HALF"]=="11"&&values["PATCH_LINES"]=="24","Pinned31657 Building Grid patch variant differs");
		for(unsigned axis=0;axis<2;++axis){Check(std::isfinite(mapWorldSize[axis])&&mapWorldSize[axis]>0,"Pinned31657 Building Grid map extent invalid");const std::string suffix=axis?"Z":"X";Check(values["LAST_LINE_"+suffix]==std::to_string(int(std::floor(mapWorldSize[axis]/48.f)))&&values["LAST_SEGMENT_"+suffix]==std::to_string(int(std::ceil(mapWorldSize[axis]/48.f))-1),"Pinned31657 Building Grid map-edge definitions differ");}
	}else Check(values.size()==4&&values["LINECOLOR"]=="vec3(0.69999999, 1, 0.69999999)"&&values["GRIDRADIUS"]=="30"&&values["RADIUSFALLOFF"]=="2.5"&&values["MAXVIEWDIST"]=="3000","Original Building Grid variant differs");
	std::string vs(vertex),fs(fragment);Replace(vs,engine,"//__ENGINEUNIFORMBUFFERDEFS__");Replace(fs,engine,"//__ENGINEUNIFORMBUFFERDEFS__");Replace(vs,defines,"//__DEFINES__");Replace(fs,defines,"//__DEFINES__");
	Check(Hash(vs)==(game31657?NativeBuildingGridVertexHash31657:NativeBuildingGridVertexHash)&&Hash(fs)==(game31657?NativeBuildingGridFragmentHash31657:NativeBuildingGridFragmentHash),"Original Building Grid shader identity differs");
}
float NativeBuildingGridAlpha(std::array<float,3> world,std::array<float,3> mouse,std::array<float,3> eye,float a){
	return (Smooth(480.f/(Distance(world,mouse)/2.5f)-2.5f)-Smooth(1-3000.f/Distance(eye,mouse))*1.75f)*a;
}
NativeWorldRasterMesh BuildNativeBuildingGrid(const NativeBuildingGridDraw& d,const std::array<double,4>& viewport,uint32_t width,uint32_t height,unsigned targetSamples){
	ValidateNativeBuildingGrid(d);if(d.vertexCount==0)return {};
	std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(d.vertexCount);
	for(size_t i=d.firstVertex;i<size_t(d.firstVertex)+d.vertexCount;++i){
		std::array<float,3> v;
		if(d.generatedPatch31657){
			// Translate the exact source integer gl_VertexID patch, including segment rejection.
			int segment=int(i)>>1;const bool alongX=segment<24*23;if(!alongX)segment-=24*23;
			const int line=segment/23,along=segment-line*23;
			const int startX=int(std::floor(d.mousePosition[0]/48.f))-11+(alongX?along:line);
			const int startZ=int(std::floor(d.mousePosition[2]/48.f))-11+(alongX?line:along);
			const int lastX=alongX?int(std::ceil(d.mapWorldSize[0]/48.f))-1:int(std::floor(d.mapWorldSize[0]/48.f));
			const int lastZ=alongX?int(std::floor(d.mapWorldSize[1]/48.f)):int(std::ceil(d.mapWorldSize[1]/48.f))-1;
			const float sx=startX*48.f,sz=startZ*48.f,ex=sx+(alongX?48.f:0),ez=sz+(alongX?0:48.f);
			const float dx=std::clamp(d.mousePosition[0],sx,ex)-d.mousePosition[0],dz=std::clamp(d.mousePosition[2],sz,ez)-d.mousePosition[2];
			if(startX<0||startZ<0||startX>lastX||startZ>lastZ||dx*dx+dz*dz>=480.f*480.f)continue;
			v={(i&1)?ex:sx,(i&1)?ez:sz,((alongX?startZ:startX)%4==0)?d.lineParameters[0]:d.lineParameters[1]};
		}else {std::memcpy(v.data(),d.vertices->bytes.data()+i*12,12);Finite(v);}
		float y=NativeBlueprintHeightLOD0(*d.heights,NativeBlueprintHeightUV({v[0],v[1]},d.mapWorldSize));
		if(d.generatedPatch31657)y=std::max(d.lineParameters[2],y);else if(d.waterSurfaceMode)y=std::max(y,d.waterLevel);
		NativeWorldDrawVertex out{};out.color={.7f,1,.7f,v[2]};out.uv={v[0],y,v[1],1};
		for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)out.position[r]+=d.view.viewProjection[c*4+r]*out.uv[c];
		if(d.view.clipDepth==ClipDepthConvention::ZeroToOne)out.position[2]=2*out.position[2]-out.position[3];vertices.push_back(out);
	}
	auto surface=d.surface;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};
	return BuildNativeWorldDrawGeometry(MakeNativeWorldDrawCommand(d.stage,NativeDrawPrimitive::Lines,vertices,surface,d.world),viewport,width,height,targetSamples);
}
std::vector<NativePlacementVertex> NativeBuildSquareVertices(std::array<float,3> pos,int nx,int nz,float h,bool canBuild,std::span<const uint8_t> status,uint8_t open,uint8_t occupied,uint8_t reclaimable){
	Check(nx>0&&nz>0&&size_t(nx)*nz==status.size(),"Original build footprint/status array mismatch");Finite(pos);Check(std::isfinite(h),"Original build height invalid");
	std::vector<NativePlacementVertex> out;out.reserve(size_t(nx)*nz*8+16);
	// SColor(float) truncates to bytes before the original VA_TYPE_C shader.
	const ScreenColor green{0,229/255.f,0,178/255.f},yellow{229/255.f,204/255.f,0,178/255.f},red{229/255.f,0,0,178/255.f};
	auto line=[&](std::array<float,3> a,std::array<float,3> b,ScreenColor c,ScreenColor e){out.push_back({a,c});out.push_back({b,e});};
	int sx=int(pos[0]/8)-(nx>>1),sz=int(pos[2]/8)-(nz>>1);
	for(int z=0;z<nz;++z)for(int x=0;x<nx;++x){auto s=status[z*nx+x];auto c=s==open?(canBuild?green:yellow):(s==occupied||s==reclaimable)?yellow:red;float px=(sx+x)*8,pz=(sz+z)*8;std::array<float,3> a{px,h,pz},b{px+8,h,pz},c2{px+8,h,pz+8},d{px,h,pz+8};line(a,b,c,c);line(b,c2,c,c);line(c2,d,c,c);line(d,a,c,c);}
	if(h<0){int x1=int(pos[0]-nx*.5f*8),x2=x1+nx*8,z1=int(pos[2]-nz*.5f*8),z2=z1+nz*8;ScreenColor a{0,0,1,128/255.f},b{0,128/255.f,1,1};std::array<std::array<float,3>,4> corners{{{float(x1),0,float(z1)},{float(x1),0,float(z2)},{float(x2),0,float(z2)},{float(x2),0,float(z1)}}};for(auto p:corners){auto under=p;under[1]=h;line(under,p,a,b);}for(unsigned i=0;i<4;++i)line(corners[i],corners[(i+1)%4],b,b);}
	return out;
}
}
