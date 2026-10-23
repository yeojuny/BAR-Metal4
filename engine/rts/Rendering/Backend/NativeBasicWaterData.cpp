/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBasicWaterData.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value,const char* message){if(!value)throw std::invalid_argument(message);}
template<class T> void Finite(const T& values){for(auto value:values)Check(std::isfinite(value),"Original BasicWater has a non-finite input");}
}
NativeBasicWaterMesh MakeNativeBasicWaterMesh(uint32_t mapX,uint32_t mapZ,uint32_t tw,uint32_t th,
	const std::string& path,float repeatX,float repeatZ)
{
	Check(mapX&&mapZ&&mapX<=65536&&mapZ<=65536&&tw&&th,"Original BasicWater geometry extent invalid");
	Check(std::isfinite(repeatX)&&std::isfinite(repeatZ),"Original BasicWater repeat is non-finite");
	// CBasicWater::GenWaterQuadsRB operation order, including its historical
	// ocean.jpg exception and independent map/bitmap aspect factors.
	constexpr float div16=1.f/16.f;
	const float mapSizeX=mapX*8,mapSizeZ=mapZ*8;
	float rx=65536.f/mapX,rz=65536.f/mapZ*tw/th;
	if(path=="bitmaps/ocean.jpg"){rx/=4;rz/=4;}
	rx=(repeatX!=0?repeatX:rx)/16;rz=(repeatZ!=0?repeatZ:rz)/16;
	NativeBasicWaterMesh out;out.vertices.reserve(4*16*16);out.indices.reserve(6*16*16);
	for(int z=0;z<16;++z)for(int x=0;x<16;++x){
		const uint32_t first=uint32_t(out.vertices.size());
		out.vertices.push_back({{(x+0)*mapSizeX*div16,0,(z+0)*mapSizeZ*div16},{(x+0)*rx,(z+0)*rz}});
		out.vertices.push_back({{(x+0)*mapSizeX*div16,0,(z+1)*mapSizeZ*div16},{(x+0)*rx,(z+1)*rz}});
		out.vertices.push_back({{(x+1)*mapSizeX*div16,0,(z+1)*mapSizeZ*div16},{(x+1)*rx,(z+1)*rz}});
		out.vertices.push_back({{(x+1)*mapSizeX*div16,0,(z+0)*mapSizeZ*div16},{(x+1)*rx,(z+0)*rz}});
		for(uint32_t i:{3u,0u,1u,3u,1u,2u})out.indices.push_back(first+i);
	}
	return out;
}
bool NativeBasicWaterVisible(const NativeBasicWaterFrame& f)
{
	// CWorldDrawer's outer guard precedes BasicWater's forceRendering test.
	// HasVisibleWater already excludes void-water and wholly dry maps.
	return f.drawWater&&f.hasVisibleWater;
}
void ValidateNativeBasicWaterMaterial(const NativeBasicWaterMaterial& material)
{
	Check(material.generation&&material.mesh&&material.texture,"Original BasicWater resource lifetime missing");
	const auto& mesh=*material.mesh;
	Check(mesh.vertices.size()==1024&&mesh.indices.size()==1536,"Original BasicWater is not its 16x16 indexed mesh");
	for(const auto& v:mesh.vertices){Finite(v.position);Finite(v.uv);}
	for(auto i:mesh.indices)Check(i<mesh.vertices.size(),"Original BasicWater index out of range");
	const auto& t=*material.texture;Check(!t.mips.empty(),"Original BasicWater image absent");
	const auto& base=t.mips.front();Check(base.width&&base.height&&base.width<=16384&&base.height<=16384,"Original BasicWater image extent invalid");
	Check(t.mips.size()<=std::bit_width(std::max(base.width,base.height)),"Original BasicWater excessive mip chain");
	size_t block=0;
	switch(t.format){case WorldTextureFormat::RGBA8Unorm:case WorldTextureFormat::BGRA8Unorm:break;
		case WorldTextureFormat::BC1RGBA:case WorldTextureFormat::BC4Unorm:block=8;break;
		case WorldTextureFormat::BC2RGBA:case WorldTextureFormat::BC3RGBA:case WorldTextureFormat::BC5Unorm:case WorldTextureFormat::BC7RGBA:case WorldTextureFormat::BC7RGBASRGB:block=16;break;
		default:Check(false,"Original BasicWater image format unknown");}
	Check(!t.generateMipmaps||t.mips.size()==1,"Original BasicWater generated mips must not replace an embedded chain");
	for(size_t n=0;n<t.mips.size();++n){const auto& mip=t.mips[n];
		Check(mip.width==std::max(1u,base.width>>n)&&mip.height==std::max(1u,base.height>>n),"Original BasicWater mip dimensions differ");
		const size_t pitch=block?((mip.width+3)/4)*block:size_t(mip.width)*4,rows=block?(mip.height+3)/4:mip.height;
		Check(mip.bytesPerRow==pitch&&mip.bytes.size()==pitch*rows,"Original BasicWater mip byte extent differs");
	}
}
void ValidateNativeBasicWater(const NativeBasicWaterFrame& f)
{
	// The immutable resource payload is validated at initialization/upload;
	// ordinary captured frames inspect only their current camera and lifetime.
	Check(f.material&&f.material->generation&&f.material->mesh&&f.material->texture,"Original BasicWater resource lifetime missing");
	Finite(f.camera.view);Finite(f.camera.projection);
	Check(f.camera.clipDepth==ClipDepthConvention::MinusOneToOne||f.camera.clipDepth==ClipDepthConvention::ZeroToOne,"Original BasicWater camera depth unknown");
	Check(f.camera.viewport[2]>0&&f.camera.viewport[3]>0,"Original BasicWater viewport empty");
}
NativeBasicWaterUniforms MakeNativeBasicWaterUniforms(const NativeBasicWaterFrame& f)
{
	ValidateNativeBasicWater(f);NativeBasicWaterUniforms out;
	for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned i=0;i<4;++i)out.viewProjection[c*4+r]+=f.camera.projection[i*4+r]*f.camera.view[c*4+i];
	out.flags={f.camera.clipDepth==ClipDepthConvention::MinusOneToOne,f.material->texture->flipV,0,0};return out;
}
} // namespace rendering
