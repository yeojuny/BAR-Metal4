/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeGroundDecalData.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool v,const char* s){if(!v)throw std::invalid_argument(s);}
template<class T>void Finite(const T& v){for(auto x:v)Check(std::isfinite(x),"Original ground decal has a non-finite input");}
}
void ValidateNativeGroundDecalAtlas(const NativeGroundDecalAtlas& a)
{
	Check(a.generation&&a.width&&a.height&&a.width<=16384&&a.height<=16384&&a.pages&&a.pages<=16,"Original ground decal atlas extent/lifetime invalid");
	Check(a.levels&&a.levels<=std::bit_width(std::max(a.width,a.height))&&a.padding<=16384,"Original ground decal atlas mip/padding invalid");
	Check(!a.sources.empty()&&!a.entries.empty(),"Original ground decal atlas sources missing");
	for(const auto& s:a.sources){Check(s&&!s->mips.empty(),"Original ground decal atlas image missing");
		const auto& m=s->mips.front();Check(m.width&&m.height&&m.width<=16384&&m.height<=16384,"Original ground decal source extent invalid");
		size_t block=0;switch(s->format){case WorldTextureFormat::RGBA8Unorm:case WorldTextureFormat::BGRA8Unorm:break;
		case WorldTextureFormat::BC1RGBA:case WorldTextureFormat::BC4Unorm:block=8;break;
		case WorldTextureFormat::BC2RGBA:case WorldTextureFormat::BC3RGBA:case WorldTextureFormat::BC5Unorm:case WorldTextureFormat::BC7RGBA:case WorldTextureFormat::BC7RGBASRGB:block=16;break;}
		const size_t row=block?((m.width+3)/4)*block:size_t(m.width)*4,rows=block?(m.height+3)/4:m.height;
		Check(m.bytesPerRow==row&&m.bytes.size()==row*rows,"Original ground decal source bytes differ");
	}
	for(const auto& e:a.entries){Check(e.source<a.sources.size()&&e.page<a.pages,"Original ground decal atlas entry references missing source/page");Finite(e.sourceUV);
		Check(e.rectangle[0]>=0&&e.rectangle[1]>=0&&e.rectangle[2]>=e.rectangle[0]&&e.rectangle[3]>=e.rectangle[1]&&uint32_t(e.rectangle[2])<a.width&&uint32_t(e.rectangle[3])<a.height,"Original ground decal atlas entry outside allocation");
		Check(e.sourceUV[0]<=e.sourceUV[2]&&e.sourceUV[1]<=e.sourceUV[3],"Original ground decal source UV inverted");}
}
std::vector<NativeGroundDecalAtlasDraw> MakeNativeGroundDecalAtlasDraws(const NativeGroundDecalAtlas& a)
{
	ValidateNativeGroundDecalAtlas(a);std::vector<NativeGroundDecalAtlasDraw> result;
	for(uint32_t page=0;page<a.pages;++page)for(uint32_t level=0;level<a.levels;++level){
		const uint32_t mipW=std::max(a.width>>level,1u),mipH=std::max(a.height>>level,1u),scale=1u<<level;
		const float invMipW=1.f/static_cast<float>(mipW),invMipH=1.f/static_cast<float>(mipH);
		const int halfPad=level<a.levels-1?a.padding/2:0;
		for(const auto& e:a.entries){if(e.page!=page)continue;
			const auto [px1,py1,px2,py2]=e.rectangle;const float entryW=float(px2-px1+1),entryH=float(py2-py1+1);
			const int x1=std::max(int(std::floor(float(px1-halfPad)/float(scale))),0),y1=std::max(int(std::floor(float(py1-halfPad)/float(scale))),0);
			const int x2=std::min(int(std::ceil(float(px2+1+halfPad)/float(scale))),int(mipW)),y2=std::min(int(std::ceil(float(py2+1+halfPad)/float(scale))),int(mipH));
			const float nx1=2.f*float(x1)*invMipW-1.f,ny1=2.f*float(y1)*invMipH-1.f,nx2=2.f*float(x2)*invMipW-1.f,ny2=2.f*float(y2)*invMipH-1.f;
			const auto& uv=e.sourceUV;const float sw=uv[2]-uv[0],sh=uv[3]-uv[1];
			const float u1=uv[0]+(float(x1*int(scale)-px1)/entryW)*sw,v1=uv[1]+(float(y1*int(scale)-py1)/entryH)*sh;
			const float u2=uv[0]+(float(x2*int(scale)-px1)/entryW)*sw,v2=uv[1]+(float(y2*int(scale)-py1)/entryH)*sh;
			const std::array<NativeGroundDecalAtlasVertex,4> q={NativeGroundDecalAtlasVertex{{nx1,ny1},{u1,v1}},NativeGroundDecalAtlasVertex{{nx2,ny1},{u2,v1}},NativeGroundDecalAtlasVertex{{nx2,ny2},{u2,v2}},NativeGroundDecalAtlasVertex{{nx1,ny2},{u1,v2}}};
			NativeGroundDecalAtlasDraw d;d.source=e.source;d.page=page;d.level=level;d.sourceClamp=uv;size_t n=0;
			for(unsigned i:{3u,0u,1u,3u,1u,2u})d.vertices[n++]=q[i];result.push_back(d);
		}
	}
	return result;
}
void ValidateNativeGroundDecalFrame(const NativeGroundDecalFrame& f)
{
	Check(f.atlas&&f.atlas->generation&&f.instances&&f.instances->revision,"Original ground decal immutable resources unavailable");
	Check(f.instances->values.size()<=1'000'000,"Original ground decal instance count invalid");
	Finite(f.viewProjection);Finite(f.inverseViewProjection);Finite(f.shadow);Finite(f.mapDims);Finite(f.mapDimsPO2);Finite(f.groundAmbient);Finite(f.groundDiffuse);Finite(f.sunDirection);Finite(f.waterMin);Finite(f.waterBase);Finite(f.waterAbsorb);
	Check(f.mapDims[0]>0&&f.mapDims[1]>0&&f.viewport[2]>0&&f.viewport[3]>0&&std::isfinite(f.adjustedFrame),"Original ground decal frame extent invalid");
	Check(f.sampleCount&&std::has_single_bit(f.sampleCount)&&f.sampleCount<=32&&(!f.highQuality||f.sampleCount>1),"Original ground decal sample-shading contract invalid");
}
NativeGroundDecalUniforms MakeNativeGroundDecalUniforms(const NativeGroundDecalFrame& f,std::array<float,4> mapping,uint32_t flips)
{
	ValidateNativeGroundDecalFrame(f);Finite(mapping);NativeGroundDecalUniforms u;u.viewProjection=f.viewProjection;u.inverseViewProjection=f.inverseViewProjection;u.shadow=f.shadow;
	u.mapDims=f.mapDims;u.mapDimsPO2=f.mapDimsPO2;u.groundAmbient=f.groundAmbient;
	auto xyz=[](WorldVector3 v){return std::array<float,4>{v[0],v[1],v[2],0};};
	u.groundDiffuse=xyz(f.groundDiffuse);u.sunDirection=xyz(f.sunDirection);u.waterMin=xyz(f.waterMin);u.waterBase=xyz(f.waterBase);u.waterAbsorb=xyz(f.waterAbsorb);
	u.frameScreen={f.adjustedFrame,1.f/f.viewport[2],1.f/f.viewport[3],0};u.fragmentToLogical=mapping;u.flags={f.zeroToOneDepth,f.shadows,f.waterAbsorption,flips};return u;
}
}
