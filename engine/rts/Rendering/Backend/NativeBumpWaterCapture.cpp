/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeBumpWaterCapture.h"
#include "Rendering/Backend/NativeWorldParticleData.h"
#include "Game/GlobalUnsynced.h"
#include "Map/ReadMap.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/MapRendering.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/EventClient.h"
#include "System/EventHandler.h"
#include "System/Exceptions.h"
#include "System/FastMath.h"
#include "System/GlobalRNG.h"
#include "System/Misc/RectangleOverlapHandler.h"
#include "System/SpringMath.h"
#include <algorithm>
#include <limits>

namespace rendering {
namespace {
uint64_t nextGeneration=0; // Unsynced main-thread ownership, never a synced ID.
WorldVector3 Vector(const float3& v){return {v.x,v.y,v.z};}
void Check(bool v,const char* error){if(!v)throw content_error(error);}
std::shared_ptr<const std::vector<WorldVector3>> Geometry(bool radial)
{
	auto out=std::make_shared<std::vector<WorldVector3>>();out->reserve(486);
	if(radial){
		// Same operation order and engine fastmath as GenWaterPlaneBuffer.
		const float xsize=float((mapDims.mapx*SQUARE_SIZE)>>2),ysize=float((mapDims.mapy*SQUARE_SIZE)>>2);
		const float alphainc=math::TWOPI/32.f,size=std::min(xsize,ysize);
		for(int n=0;n<4;++n){const float k=n==3?.5f:1.f;const float r1=(n+0)*(n+0)*size,r2=(n+k)*(n+k)*size;
			for(float alpha=0;(alpha-math::TWOPI)<alphainc;alpha+=alphainc){
				out->push_back({r1*fastmath::sin(alpha)+2*xsize,0,r1*fastmath::cos(alpha)+2*ysize});
				out->push_back({r2*fastmath::sin(alpha)+2*xsize,0,r2*fastmath::cos(alpha)+2*ysize});
			}
		}
	}else{
		const int mapX=mapDims.mapx*SQUARE_SIZE,mapZ=mapDims.mapy*SQUARE_SIZE;
		for(int z=0;z<9;++z)for(int x=0;x<9;++x){
			const WorldVector3 v0{(x+0)*(mapX/9.f),0,(z+0)*(mapZ/9.f)},v1{(x+0)*(mapX/9.f),0,(z+1)*(mapZ/9.f)};
			const WorldVector3 v2{(x+1)*(mapX/9.f),0,(z+0)*(mapZ/9.f)},v3{(x+1)*(mapX/9.f),0,(z+1)*(mapZ/9.f)};
			for(const auto& p:{v0,v1,v2,v1,v3,v2})out->push_back(p);
		}
	}return out;
}
}
struct NativeBumpWaterCapture::State: CEventClient {
	State():CEventClient("[NativeBumpWater]",271923,false){eventHandler.AddClient(this);}
	~State()override{eventHandler.RemoveClient(this);}
	bool WantsEvent(const std::string& event)override{return material&&shore&&(event=="UnsyncedHeightMapUpdate");}
	bool GetFullRead()const override{return true;}
	int GetReadAllyTeam()const override{return AllAccessTeam;}
	void UnsyncedHeightMapUpdate(const SRectangle& rect)override{if(shore&&readMap->HasVisibleWater())heightUpdates.push_back(rect);}
	NativeBumpWaterSettings settings;
	std::shared_ptr<const NativeBumpWaterMaterial> material;
	std::shared_ptr<const std::vector<WorldVector3>> vertices;
	uint32_t screenWidth=0,screenHeight=0;
	bool shore=false,dynamic=false,radial=false;
	std::vector<uint8_t> offsets;
	uint64_t normalSequence=0,coastSequence=0,normalRetired=0,coastRetired=0;
	int lastUpdateFrame=std::numeric_limits<int>::min();
	std::vector<NativeBumpWaterNormalUpdate> normalUpdates;
	std::vector<std::shared_ptr<const NativeBumpWaterCoastUpdate>> coastUpdates;
	CRectangleOverlapHandler heightUpdates;
	struct Rect {int x1,y1,x2,y2;NativeBumpWaterCoastRect dto;};
	std::vector<Rect> pendingRects;
	std::optional<NativeBumpWaterCoastUpdate> pendingAtlas;
	void Normal(bool initialize){
		const uint8_t tiles=material->numTiles,ntiles=tiles*tiles;
		const int modFrame=(gs->frameNum+1)%60;
		if(modFrame==0)for(uint8_t i=0;i<ntiles;++i){do{offsets[i]=uint8_t(guRNG.NextFloat()*ntiles);}while(offsets[i]==i);}
		// ntiles intentionally remains uint8: original numTiles16 initializes
		// all256 offsets to0 and never enters the random-selection loop.
		normalUpdates.push_back({++normalSequence,gs->frameNum,initialize,offsets});
	}
	void UploadCoast(bool forceFull){
		heightUpdates.Process(forceFull);uint32_t currentPixels=0,numCoastRects=0;
		while(!heightUpdates.empty()){
			const auto& area=heightUpdates.front();if(currentPixels+area.GetArea()>512*512&&!forceFull)break;
			currentPixels+=area.GetArea();Rect r{std::max(area.x1-15,0),std::max(area.y1-15,0),std::min(area.x2+15,mapDims.mapx),std::min(area.y2+15,mapDims.mapy),{}};
			r.dto.mapUV={(r.x1+.5f)/mapDims.mapx,(r.y1+.5f)/mapDims.mapy,(r.x2+.5f)/mapDims.mapx,(r.y2+.5f)/mapDims.mapy};
			pendingRects.push_back(r);heightUpdates.pop_front();
		}
		CTextureAtlas atlas;
		const float* heights=gs->PreSimFrame()?readMap->GetCornerHeightMapSynced():readMap->GetCornerHeightMapUnsynced();
		for(size_t i=0;i<pendingRects.size();++i){auto& r=pendingRects[i];const int w=r.x2-r.x1,h=r.y2-r.y1;uint32_t ground=0;
			auto* pixels=static_cast<uint8_t*>(atlas.AddGetTex(std::to_string(i),w,h));
			for(int y=0;y<h;++y)for(int x=0;x<w;++x){const float value=heights[(y+r.y1)*mapDims.mapxp1+x+r.x1];
				const auto pixel=NativeBumpWaterCoastPixel(value);std::copy(pixel.begin(),pixel.end(),pixels+(y*w+x)*4);ground+=(value>0);}
			numCoastRects+=(r.dto.coastline=ground!=0&&ground!=uint32_t(w*h));
		}
		if(!numCoastRects||!atlas.Finalize()){pendingRects.clear();pendingAtlas.reset();return;}
		const auto packed=atlas.GetNativeAtlas();Check(packed&&packed->pages.size()==1&&!packed->pages.front().empty(),"Original coast CPU atlas did not produce one packed page");
		const auto& page=packed->pages.front().front();NativeBumpWaterCoastUpdate out;out.atlasWidth=page.width;out.atlasHeight=page.height;out.rgba=page.rgba;
		for(size_t i=0;i<pendingRects.size();++i){auto& r=pendingRects[i];const auto& tex=atlas.GetTexture(std::to_string(i));
			r.dto.atlasUV={tex.xstart,tex.ystart,tex.xend,tex.yend};out.rectangles.push_back(r.dto);}
		pendingAtlas=std::move(out);
	}
	void PublishCoast(bool initialize){
		if(!pendingAtlas)return;pendingAtlas->sequence=++coastSequence;pendingAtlas->initialize=initialize;
		coastUpdates.push_back(std::make_shared<const NativeBumpWaterCoastUpdate>(std::move(*pendingAtlas)));pendingAtlas.reset();pendingRects.clear();
	}
};
NativeBumpWaterCapture::NativeBumpWaterCapture()=default;
NativeBumpWaterCapture::~NativeBumpWaterCapture()=default;
void NativeBumpWaterCapture::Reset(){state.reset();}
bool NativeBumpWaterCapture::Initialized()const{return state&&state->material;}
void NativeBumpWaterCapture::Initialize(const NativeBumpWaterSettings& settings,TextureProvider provider)
{
	Check(bool(provider)&&readMap&&gs&&globalRendering,"Original water initialization lacks real engine inputs");
	auto next=std::make_unique<State>();next->settings=settings;auto m=std::make_shared<NativeBumpWaterMaterial>();
	Check(++nextGeneration!=0,"Original water resource generation overflow");m->generation=nextGeneration;
	const auto& w=*waterRendering;
	m->shaderViewport={globalRendering->viewPosX,globalRendering->viewPosY,globalRendering->viewSizeX,globalRendering->viewSizeY};m->initialVisibleWater=readMap->HasVisibleWater();
	m->surfaceColor=Vector(w.surfaceColor);m->diffuseColor=Vector(w.diffuseColor);m->specularColor=Vector(w.specularColor);m->sunDirection=Vector(ISky::GetSky()->GetLight()->GetLightDir());
	m->surfaceAlpha=w.surfaceAlpha;m->ambientFactor=w.ambientFactor;m->diffuseFactor=w.diffuseFactor;m->specularFactor=w.specularFactor;m->specularPower=w.specularPower;
	m->fresnelMin=w.fresnelMin;m->fresnelMax=w.fresnelMax;m->fresnelPower=w.fresnelPower;m->reflectionDistortion=w.reflDistortion;m->blurBase=w.blurBase;m->blurExponent=w.blurExponent;
	m->perlinStartFrequency=w.perlinStartFreq;m->perlinLacunarity=w.perlinLacunarity;m->perlinAmplitude=w.perlinAmplitude;m->waveOffsetFactor=w.waveOffsetFactor;m->waveLength=w.waveLength;
	m->waveFoamDistortion=w.waveFoamDistortion;m->waveFoamIntensity=w.waveFoamIntensity;m->causticsResolution=w.causticsResolution;m->causticsStrength=w.causticsStrength;
	m->shadowDensity=sunLighting->groundShadowDensity;m->numTiles=w.numTiles;m->shoreWaves=w.shoreWaves;m->hasWaterPlane=w.hasWaterPlane;m->forceRendering=w.forceRendering;
	m->foam=provider(w.foamTexture);m->normal=provider(w.normalTexture);Check(!w.causticTextures.empty(),"Original BumpWater has no caustic textures");
	for(const auto& path:w.causticTextures)m->caustics.push_back(provider(path));
	next->shore=settings.shoreWaves&&w.shoreWaves&&mapDims.mapx<=4096&&mapDims.mapy<=4096;
	next->dynamic=settings.dynamicWaves&&w.numTiles>1;
	next->radial=settings.endlessOcean&&w.hasWaterPlane&&(readMap->HasVisibleWater()||w.forceRendering);
	if(next->shore)m->waveRandom=provider("bitmaps/shorewaverand.png");
	next->material=std::move(m);next->vertices=Geometry(next->radial);next->screenWidth=globalRendering->viewSizeX;next->screenHeight=globalRendering->viewSizeY;
	if(next->shore){next->UnsyncedHeightMapUpdate(SRectangle(0,0,mapDims.mapx,mapDims.mapy));next->UploadCoast(true);next->PublishCoast(true);eventHandler.InsertEvent(next.get(),"UnsyncedHeightMapUpdate");}
	if(next->dynamic){next->offsets.resize(w.numTiles*w.numTiles);next->Normal(true);}
	state=std::move(next);
}
void NativeBumpWaterCapture::Update()
{
	Check(Initialized(),"Original native water Update before InitResources");
	if(!waterRendering->forceRendering&&!readMap->HasVisibleWater())return;
	Check(gs->frameNum!=state->lastUpdateFrame,"Original water Update duplicated within one new simulation frame");state->lastUpdateFrame=gs->frameNum;
	if(state->dynamic)state->Normal(false);
	if(state->shore){if(gs->frameNum%10==0&&!state->heightUpdates.empty())state->UploadCoast(false);if(gs->frameNum%10==5&&!state->pendingRects.empty())state->PublishCoast(false);}
}
std::shared_ptr<const NativeBumpWaterFrame> NativeBumpWaterCapture::Capture(const WorldFrame& world,bool shadows,bool infoTexture,bool wireframe)const
{
	Check(Initialized(),"Original native water Capture before InitResources");auto out=std::make_shared<NativeBumpWaterFrame>();
	out->generation=state->material->generation;out->material=state->material;out->settings=state->settings;out->vertices=state->vertices;out->radial=state->radial;
	out->sourceDrawFrame=world.sourceDrawFrame;out->simulationFrame=gs->frameNum;out->interpolationOffset=world.interpolationOffset;
	out->screenTextureWidth=state->screenWidth;out->screenTextureHeight=state->screenHeight;
	out->mapSquaresX=mapDims.mapx;out->mapSquaresZ=mapDims.mapy;out->paddedSquaresX=mapDims.pwr2mapx;out->paddedSquaresZ=mapDims.pwr2mapy;
	out->logicalWidth=globalRendering->winSizeX;out->logicalHeight=globalRendering->winSizeY;out->camera=world.viewCamera;
	out->drawWater=globalRendering->drawWater;out->voidWater=mapRendering->voidWater;out->belowSeaLevel=readMap->HasVisibleWater();
	out->shadows=shadows;out->infoTexture=infoTexture;out->wireframe=wireframe;std::copy_n(shadowHandler.GetShadowMatrixRaw(),16,out->shadowMatrix.begin());
	out->fogColor=world.lighting.fogColor;out->fogParameters=world.lighting.fogParameters;
	out->normalUpdates=state->normalUpdates;out->coastUpdates=state->coastUpdates;ValidateNativeBumpWater(*out);return out;
}
void NativeBumpWaterCapture::DidComplete(uint64_t generation,uint64_t normal,uint64_t coast)
{
	if(!Initialized()||state->material->generation!=generation)return;
	Check(normal>=state->normalRetired&&normal<=state->normalSequence&&coast>=state->coastRetired&&coast<=state->coastSequence,"Original water acknowledgment lies outside produced sequence range");
	state->normalRetired=normal;state->coastRetired=coast;
	std::erase_if(state->normalUpdates,[&](const auto& u){return u.sequence<=normal;});std::erase_if(state->coastUpdates,[&](const auto& u){return u->sequence<=coast;});
}
} // namespace rendering
