/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBumpWaterData.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool value,const char* error) {if(!value)throw std::invalid_argument(error);}
template<class T> void Finite(const T& values) {for(auto x:values)Check(std::isfinite(x),"Non-finite original BumpWater input");}
void Image(const std::shared_ptr<const WorldTexture>& image) {
	Check(image&&!image->mips.empty(),"Missing actual original BumpWater image");
	const auto& base=image->mips.front();Check(base.width&&base.height&&base.width<=16384&&base.height<=16384,"Invalid original water bitmap dimensions");
	Check(image->mips.size()<=std::bit_width(std::max(base.width,base.height)),"Original water bitmap has excessive mip levels");
	bool compressed=false;size_t block=0;
	switch(image->format){
		case WorldTextureFormat::RGBA8Unorm:case WorldTextureFormat::BGRA8Unorm:break;
		case WorldTextureFormat::BC1RGBA:case WorldTextureFormat::BC4Unorm:compressed=true;block=8;break;
		case WorldTextureFormat::BC2RGBA:case WorldTextureFormat::BC3RGBA:case WorldTextureFormat::BC5Unorm:case WorldTextureFormat::BC7RGBA:case WorldTextureFormat::BC7RGBASRGB:compressed=true;block=16;break;
		default:Check(false,"Original water bitmap format unknown");
	}
	for(size_t n=0;n<image->mips.size();++n){const auto& mip=image->mips[n];
		Check(mip.width==std::max(1u,base.width>>n)&&mip.height==std::max(1u,base.height>>n),"Original water bitmap mip dimensions differ");
		const size_t rows=compressed?(mip.height+3)/4:mip.height,rowBytes=compressed?((mip.width+3)/4)*block:size_t(mip.width)*4;
		Check(mip.bytesPerRow==rowBytes&&mip.bytes.size()==rows*rowBytes,"Original water bitmap mip byte extent differs");
	}
}
WorldMatrix Multiply(const WorldMatrix& a,const WorldMatrix& b) {
	WorldMatrix result{};for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r)for(unsigned i=0;i<4;++i)result[c*4+r]+=a[i*4+r]*b[c*4+i];return result;
}
std::array<float,4> Vector(const WorldVector3& v,float w) {return {v[0],v[1],v[2],w};}
}
bool NativeBumpWaterVisible(const NativeBumpWaterFrame& f) {
	// Original CWorldDrawer::DrawAlphaObjects outer eligibility. forceRendering
	// alone does not bypass this world-stage/map void-water decision.
	return f.drawWater&&!f.voidWater&&f.belowSeaLevel;
}
bool NativeBumpWaterDynamicNormals(const NativeBumpWaterFrame& f) {
	return f.material&&f.settings.dynamicWaves&&f.material->numTiles>1;
}
bool NativeBumpWaterShoreWaves(const NativeBumpWaterFrame& f) {
	// The original tests a 4096 square proxy, not the device's full limit.
	return f.material&&f.settings.shoreWaves&&f.material->shoreWaves&&f.mapSquaresX<=4096&&f.mapSquaresZ<=4096;
}
size_t NativeBumpWaterCausticIndex(int64_t frame,size_t count) {
	Check(frame>=-1&&frame<=INT32_MAX&&count,"Invalid original caustic frame/count");
	// gs->frameNum is signed32, caustTextures.size() is size_t. Preserve the
	// unsigned conversion even at pre-simulation -1, including non-power-two.
	return static_cast<size_t>(static_cast<int32_t>(frame))%count;
}
float NativeBumpWaterNormalAlpha(const NativeBumpWaterNormalUpdate& u) {
	Check(u.simulationFrame>=-1,"Original normal update predates pre-simulation frame");
	return u.initialize?1.f:float((u.simulationFrame+1)%60+1)/600.f;
}
std::array<uint8_t,4> NativeBumpWaterCoastPixel(float h) {
	Check(std::isfinite(h)&&std::abs(h)<1e7f,"Invalid original coast height");
	return {uint8_t(h>10?255:0),uint8_t(h>0?255:0),uint8_t(h<0?std::max(0,255+int(10.f*h)):255),0};
}
void ValidateNativeBumpWater(const NativeBumpWaterFrame& f) {
	Check(f.generation&&f.material&&f.material->generation==f.generation,"Missing/mixed original water generation");
	const auto& m=*f.material;const auto& s=f.settings;
	Check(f.mapSquaresX&&f.mapSquaresZ&&f.paddedSquaresX>=f.mapSquaresX&&f.paddedSquaresZ>=f.mapSquaresZ,"Invalid original water map dimensions");
	Check(f.mapSquaresX<=16384&&f.mapSquaresZ<=16384,"Original water map dimensions exceed native range");
	Check(f.screenTextureWidth&&f.screenTextureHeight,"Original screen-copy allocation extent missing");
	Check(f.logicalWidth&&f.logicalHeight&&f.camera.viewport[2]>0&&f.camera.viewport[3]>0,"Invalid original water viewport");
	Check(m.shaderViewport[0]>=0&&m.shaderViewport[1]>=0&&m.shaderViewport[2]>0&&m.shaderViewport[3]>0,"Original water shader initialization viewport missing");
	Check(f.camera.viewport[0]>=0&&f.camera.viewport[1]>=0&&uint64_t(f.camera.viewport[0])+f.camera.viewport[2]<=f.logicalWidth&&uint64_t(f.camera.viewport[1])+f.camera.viewport[3]<=f.logicalHeight,"Original water viewport escapes logical drawable");
	Check(f.camera.clipDepth!=ClipDepthConvention::Unsupported,"Original water projection convention missing");
	Finite(f.camera.view);Finite(f.camera.projection);Finite(f.camera.position);Finite(f.shadowMatrix);Finite(f.fogColor);Finite(f.fogParameters);
	Check(f.simulationFrame>=-1&&std::isfinite(f.interpolationOffset),"Invalid original water frame time");
	Check(s.reflection<=2&&s.refraction<=1,"Unimplemented original water setting variant");
	Check(!s.refraction||s.depthCopy,"Original BumpWater refraction requires opt_depth/ConvertDepthToEyeZ");
	Check(s.reflectionSize>=32&&s.reflectionSize<=16384&&(s.reflectionSize&(s.reflectionSize-1))==0,"Original water reflection size must be bit-ceiled");
	Check(s.reflectionDepthBits>=16&&s.reflectionDepthBits<=32&&std::isfinite(s.anisotropy)&&s.anisotropy>=0,"Invalid original water depth/anisotropy setting");
	Check(m.numTiles>=1&&m.numTiles<=16,"Original water numTiles outside1..16");
	Finite(m.surfaceColor);Finite(m.diffuseColor);Finite(m.specularColor);Finite(m.sunDirection);
	Finite(std::array{m.surfaceAlpha,m.ambientFactor,m.diffuseFactor,m.specularFactor,m.specularPower,m.fresnelMin,m.fresnelMax,m.fresnelPower,m.reflectionDistortion,m.blurBase,m.blurExponent,m.perlinStartFrequency,m.perlinLacunarity,m.perlinAmplitude,m.waveOffsetFactor,m.waveLength,m.waveFoamDistortion,m.waveFoamIntensity,m.causticsResolution,m.causticsStrength,m.shadowDensity});
	Image(m.normal);Image(m.foam);Check(!m.caustics.empty(),"Original BumpWater has no caustic textures");for(const auto& image:m.caustics)Image(image);
	if(NativeBumpWaterShoreWaves(f)){Image(m.waveRandom);Check(m.waveLength!=0,"Original shore wave length is zero");}
	Check(f.vertices&&!f.vertices->empty(),"Original water surface geometry absent");for(const auto& p:*f.vertices)Finite(p);
	const bool radial=s.endlessOcean&&m.hasWaterPlane&&(m.initialVisibleWater||m.forceRendering);Check(radial==f.radial,"Original endless-ocean initialization geometry selection differs");
	Check(f.radial||f.vertices->size()==486,"Original ordinary water must retain9x9surface triangles");
	uint64_t sequence=0;for(const auto& u:f.normalUpdates){Check(u.sequence>sequence,"Original normal operation sequence is not increasing");sequence=u.sequence;Check(NativeBumpWaterDynamicNormals(f),"Normal update exists while original dynamic waves are disabled");Check(u.tileOffsets.size()==m.numTiles*m.numTiles,"Original normal tile count differs");for(auto n:u.tileOffsets)Check(n<u.tileOffsets.size(),"Original normal tile offset outside atlas");NativeBumpWaterNormalAlpha(u);}
	sequence=0;for(const auto& update:f.coastUpdates){Check(bool(update),"Missing immutable original coast update");const auto& u=*update;Check(u.sequence>sequence,"Original coast operation sequence is not increasing");sequence=u.sequence;Check(NativeBumpWaterShoreWaves(f),"Coast update exists while original shore waves are disabled");Check(u.atlasWidth&&u.atlasHeight&&u.atlasWidth<=16384&&u.atlasHeight<=16384&&uint64_t(u.atlasWidth)*u.atlasHeight*4==u.rgba.size(),"Original coast atlas bytes/dimensions differ");Check(!u.rectangles.empty(),"Original coast operation has no rectangles");for(const auto& r:u.rectangles){Finite(r.mapUV);Finite(r.atlasUV);for(auto x:r.atlasUV)Check(x>=0&&x<=1,"Original coast atlas coordinate invalid");Check(r.mapUV[2]>r.mapUV[0]&&r.mapUV[3]>r.mapUV[1]&&r.atlasUV[2]>r.atlasUV[0]&&r.atlasUV[3]>r.atlasUV[1],"Original coast rectangle empty/reversed");}}
}
NativeBumpWaterUniforms MakeNativeBumpWaterUniforms(const NativeBumpWaterFrame& f,uint32_t rw,uint32_t rh,uint32_t flips) {
	ValidateNativeBumpWater(f);Check(rw&&rh,"Native water render target empty");const auto& m=*f.material;const auto& s=f.settings;NativeBumpWaterUniforms u;
	u.viewProjection=Multiply(f.camera.projection,f.camera.view);u.view=f.camera.view;u.shadow=f.shadowMatrix;
	u.eyeFrame=Vector(f.camera.position,(float(f.simulationFrame)+f.interpolationOffset)/15000.f);
	const float mx=float(f.mapSquaresX)*8,mz=float(f.mapSquaresZ)*8,vx=float(m.shaderViewport[2]),vy=float(m.shaderViewport[3]);
	u.mapMidSpecularPower={mx*.5f,0,mz*.5f,m.specularPower};u.screenInverseTextureInverse={1/vx,1/vy,1.f/f.screenTextureWidth,1.f/f.screenTextureHeight};
	u.viewportOriginProjection={float(m.shaderViewport[0]),float(m.shaderViewport[1]),f.camera.projection[14],f.camera.projection[10]};
	const float sx=mx>mz?float(f.mapSquaresZ>>6)/16.f*mx/mz:float(f.mapSquaresX>>6)/16.f;
	const float sz=mx>mz?float(f.mapSquaresZ>>6)/16.f:float(f.mapSquaresX>>6)/16.f*mz/mx;
	u.texGenPlane={1/mx,1/mz,sx/mx,sz/mz};const float shadeX=float(f.mapSquaresX)/f.paddedSquaresX,shadeZ=float(f.mapSquaresZ)/f.paddedSquaresZ;u.shadingPlane={shadeX/mx,shadeZ/mz,shadeX,shadeZ};
	u.surfaceColor={m.surfaceColor[0]*.4f,m.surfaceColor[1]*.4f,m.surfaceColor[2]*.4f,m.surfaceAlpha};u.diffuseColorFactor=Vector(m.diffuseColor,m.diffuseFactor*15);u.specularColorFactor=Vector(m.specularColor,m.specularFactor);u.sunDirectionAmbient=Vector(m.sunDirection,m.ambientFactor);
	u.fresnelDistortion={m.fresnelMin,m.fresnelMax,m.fresnelPower,m.reflectionDistortion};u.blurPerlin={m.blurBase/vy,m.blurExponent,m.perlinStartFrequency,m.perlinLacunarity};u.perlinWave={m.perlinAmplitude,m.waveOffsetFactor,m.waveLength,m.waveFoamDistortion};u.waveCaustic={m.waveFoamIntensity,m.causticsResolution,m.causticsStrength,m.shadowDensity};u.fogColor=f.fogColor;u.fogParameters=f.fogParameters;
	u.fragmentToLogical={float(f.logicalWidth)/rw,-float(f.logicalHeight)/rh,0,float(f.logicalHeight)};
	u.flags={uint32_t((s.reflection?BumpReflection:0)|(s.refraction?BumpRefraction:0)|(NativeBumpWaterShoreWaves(f)?BumpShore:0)|(s.depthCopy?BumpDepth:0)|(s.blurReflection?BumpBlur:0)|(f.radial?BumpEndless:0)|(f.shadows?BumpShadow:0)|(f.infoTexture?BumpInfo:0)|(f.camera.clipDepth==ClipDepthConvention::MinusOneToOne?BumpMinusOneDepth:0)),flips,0,0};
	return u;
}
namespace {
void AppendQuad(std::vector<NativeBumpWaterUpdateVertex>& out,const std::array<float,4>& destination,const std::array<float,4>& source)
{
	const NativeBumpWaterUpdateVertex vertices[4]={
		{{destination[0],destination[1]},{source[0],source[1],0,0}},
		{{destination[0],destination[3]},{source[0],source[3],0,1}},
		{{destination[2],destination[3]},{source[2],source[3],1,1}},
		{{destination[2],destination[1]},{source[2],source[1],1,0}}};
	for(unsigned i:{3,0,1,3,1,2})out.push_back(vertices[i]);
}
}
std::vector<NativeBumpWaterUpdateVertex> MakeNativeBumpWaterNormalVertices(uint32_t tiles,const NativeBumpWaterNormalUpdate& update)
{
	Check(tiles>=2&&tiles<=16&&update.tileOffsets.size()==tiles*tiles,"Original water normal tile stream invalid");
	std::vector<NativeBumpWaterUpdateVertex> out;out.reserve(tiles*tiles*6);
	const float size=1.f/tiles;
	for(uint32_t y=0;y<tiles;++y)for(uint32_t x=0;x<tiles;++x){
		const uint8_t offset=update.tileOffsets[y*tiles+x];Check(offset<tiles*tiles,"Original normal tile outside atlas");
		const uint8_t tx=offset%tiles,ty=(offset-tx)/tiles;
		AppendQuad(out,{x*size,y*size,(x+1)*size,(y+1)*size},{tx*size,ty*size,(tx+1)*size,(ty+1)*size});
	}
	return out;
}
std::vector<NativeBumpWaterUpdateVertex> MakeNativeBumpWaterCoastVertices(const NativeBumpWaterCoastUpdate& update,uint32_t radius)
{
	Check(radius<=10,"Original coast iteration outside0..10");std::vector<NativeBumpWaterUpdateVertex> out;
	for(const auto& r:update.rectangles){
		if(radius&&!r.coastline)continue;
		if(radius&1)AppendQuad(out,r.atlasUV,r.mapUV);else AppendQuad(out,r.mapUV,r.atlasUV);
	}
	return out;
}
} // namespace rendering
