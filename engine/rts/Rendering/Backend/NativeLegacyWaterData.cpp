/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeLegacyWaterData.h"
#include "NativeLegacyWaterTextureMatrices.h"
#include "NativeLegacyWaterTextureEnvironment.h"
#include "NativeLegacyWaterTexGen.h"
#include "NativeLegacyWaterLighting.h"
#include "Game/Camera.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Backend/NativeOriginalModelContext.h"
#include "System/SpringMath.h"
#include "Sim/Misc/GlobalSynced.h"
#include <algorithm>
#include <bit>
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include <limits>
#include "System/FileSystem/FileHandler.h"
#include <stdexcept>
#include "Rendering/Backend/NativeUIRecorder.h"
namespace rendering {
void NativeLegacyWaterCapture::Initialize(LegacyRefractionTarget target)
{
 static uint64_t generation=0;
 if(material)throw std::logic_error("Legacy water source initialized twice");
 refractionTarget=LegacyRefractionTarget::Unobserved;refractionAllocation={};
 if(target==LegacyRefractionTarget::Unobserved){
 CFileHandler shader("shaders/ARB/water.fp");std::string bytes;
 if(!shader.FileExists()||!shader.LoadStringData(bytes)||bytes!=R"ORIGINAL(!!ARBfp1.0
OPTION ARB_fog_linear;
PARAM bumpAdd = { -0.5, -0.5, 0, 0};
PARAM bumpMul = { 0.02, 0.02, 0.0, 0.0};
PARAM alphaMul = { 0.03, 0.0, 0.0, 1};
TEMP temp,temp2,temp3;
TEX temp3, fragment.texcoord[1], texture[1], 2D;
ADD temp3, temp3, bumpAdd;
DP4 temp.x, temp3, program.env[0];
DP4 temp.y, temp3, program.env[1];
MAD temp2, temp, bumpMul,fragment.texcoord[0];
TEX temp3, temp2, texture[0], 2D;
MOV temp2, fragment.color;
MAD temp2.w, temp.y, alphaMul.x, temp2.w;
MUL result.color, temp3, temp2;
END)ORIGINAL")
  throw std::runtime_error("Water1 runtime ARB program differs from the translated original");
 }else{
  const char* path=target==LegacyRefractionTarget::Rectangle?"shaders/ARB/waterRefractTR.fp":"shaders/ARB/waterRefractT2D.fp";
  CFileHandler refract(path);std::string actual;
  const std::string expected=target==LegacyRefractionTarget::Rectangle?R"TR(!!ARBfp1.0

# water reflective+refraction program for cards supporting GL_ARB_texture_rectangle
# program.env[2] is texture scale

OPTION ARB_fog_linear;
PARAM bumpAdd = { -0.5, -0.5, 0, 0};
PARAM bumpMul = { 0.02, 0.02, 0.0, 0.0};
TEMP temp,temp2,temp3,subsurfcol,sstexc,waterDepth,blendfactor;
TEX temp3, fragment.texcoord[1], texture[1], 2D;
ADD temp3, temp3, bumpAdd;
DP4 temp.x, temp3, program.env[0];
DP4 temp.y, temp3, program.env[1];
MAD temp2, temp, bumpMul,fragment.texcoord[0];
TEX temp3, temp2, texture[0], 2D;
MUL temp3, temp3, fragment.color;

# Shading texture with waterdepth (stored as 255+groundheight) encoded in alpha
TEX waterDepth, fragment.texcoord[3], texture[3], 2D;

# More depth gives more texture coordinate offset
SUB temp2, {1,1,1,1}, waterDepth.a;
MUL temp2, temp2, program.env[2];  # Texture coordinates for RECT mode are in pixels
MUL temp2, temp2, fragment.position.w;
MAD sstexc, temp, temp2, fragment.position;
TEX subsurfcol, sstexc, texture[2], RECT;

# deeper water has darker underwater surface color
MAD blendfactor, waterDepth.a, {0.5,0.5,0.5,0.0}, {0.4, 0.4, 0.4, 0.0};
LRP result.color, blendfactor, subsurfcol, temp3;
END
)TR":R"T2D(!!ARBfp1.0

# water reflective+refraction program for cards not supporting GL_ARB_texture_rectangle

OPTION ARB_fog_linear;
PARAM bumpAdd = { -0.5, -0.5, 0, 0};
PARAM bumpMul = { 0.02, 0.02, 0.0, 0.0};
PARAM texOffsetScale = program.env[2];
PARAM windowCoordMul = program.env[3]; # 1/next_power_of_two(screen dimensions)
TEMP temp,temp2,temp3,subsurfcol,sstexc,waterDepth,blendfactor;
TEX temp3, fragment.texcoord[1], texture[1], 2D;
ADD temp3, temp3, bumpAdd;
DP4 temp.x, temp3, program.env[0];
DP4 temp.y, temp3, program.env[1];
MAD temp2, temp, bumpMul,fragment.texcoord[0];
TEX temp3, temp2, texture[0], 2D;
MUL temp3, temp3, fragment.color;

# Shading texture with waterdepth (stored as 255+groundheight) encoded in alpha
TEX waterDepth, fragment.texcoord[3], texture[3], 2D;

# More depth gives more texture coordinate offset
SUB temp2, {1,1,1,1}, waterDepth.a;
MUL temp2, temp2, texOffsetScale;
MUL temp2, temp2, fragment.position.w;
MUL sstexc, fragment.position, windowCoordMul;
MAD sstexc, temp, temp2, sstexc;
TEX subsurfcol, sstexc, texture[2], 2D;

# deeper water has darker underwater surface color
MAD blendfactor, waterDepth.a, {0.5,0.5,0.5,0.0}, {0.4, 0.4, 0.4, 0.0};
LRP result.color, blendfactor, subsurfcol, temp3;
END
)T2D";
  if(!refract.FileExists()||!refract.LoadStringData(actual)||actual!=expected)throw std::runtime_error("Refractive runtime ARB program differs");
  if(globalRendering->viewSizeX<=0||globalRendering->viewSizeY<=0)throw std::runtime_error("Refractive original allocation is empty");
  refractionTarget=target;refractionAllocation={uint32_t(globalRendering->viewSizeX),uint32_t(globalRendering->viewSizeY)};
  if(target==LegacyRefractionTarget::PowerOfTwo)for(auto& size:refractionAllocation)size=std::bit_ceil(size);
 }
 if(generation==std::numeric_limits<uint64_t>::max())throw std::overflow_error("Legacy water generation overflow");
 auto next=std::make_shared<NativeLegacyWaterBumpMaterial>();next->generation=++generation;
 std::vector<unsigned char> scrap(512*512*4);unsigned rawIndex=0;
	for (int y = 0; y < 64; ++y) {
		for (int x = 0; x < 64; ++x) {
			scrap[(y*64 + x)*4 + 0] = 128;
			scrap[(y*64 + x)*4 + 1] = (unsigned char)(fastmath::sin(y * math::TWOPI / 64.0f) * 128 + 128);
			scrap[(y*64 + x)*4 + 2] = 0;
			scrap[(y*64 + x)*4 + 3] = 255;
		}
	}
	std::copy_n(scrap.begin(),64*64*4,next->rgba[rawIndex++].begin());

	for (int y = 0; y < 64; ++y) {
		for (int x = 0; x < 64; ++x) {
			const float ang = 26.5f*math::DEG_TO_RAD;
			const float pos = y*2+x;

			scrap[(y*64 + x)*4 + 0] = (unsigned char)((fastmath::sin(pos*math::TWOPI / 64.0f)) * 128 * fastmath::sin(ang)) + 128;
			scrap[(y*64 + x)*4 + 1] = (unsigned char)((fastmath::sin(pos*math::TWOPI / 64.0f)) * 128 * fastmath::cos(ang)) + 128;
		}
	}

	std::copy_n(scrap.begin(),64*64*4,next->rgba[rawIndex++].begin());

	for (int y = 0; y < 64; ++y) {
		for (int x = 0; x < 64; ++x) {
			const float ang = -19.0f * math::DEG_TO_RAD;
			const float pos = 3.0f * y - x;

			scrap[(y*64 + x)*4 + 0] = (unsigned char)((fastmath::sin(pos*math::TWOPI / 64.0f)) * 128 * fastmath::sin(ang)) + 128;
			scrap[(y*64 + x)*4 + 1] = (unsigned char)((fastmath::sin(pos*math::TWOPI / 64.0f)) * 128 * fastmath::cos(ang)) + 128;
		}
	}

	std::copy_n(scrap.begin(),64*64*4,next->rgba[rawIndex++].begin());


 if(refractionTarget!=LegacyRefractionTarget::Unobserved){
  auto& textures=OriginalNativeTextureContext();
  refractionPublication.Create(textures.Registry(),textures.History(),refractionTarget==LegacyRefractionTarget::Rectangle?NativeCUSTextureTarget::Rectangle:NativeCUSTextureTarget::Image2D,refractionAllocation[0],refractionAllocation[1]);
 }
 material=std::move(next);
 surfaceColor={waterRendering->surfaceColor.x,waterRendering->surfaceColor.y,waterRendering->surfaceColor.z};
}
std::shared_ptr<const NativeLegacyWaterFrame> NativeLegacyWaterCapture::Capture(const WorldFrame& frame,bool wireframe)const
{
 if(!material||!camera)throw std::logic_error("Legacy water source missing actual owner/camera");
 auto out=std::make_shared<NativeLegacyWaterFrame>();out->sourceDrawFrame=frame.sourceDrawFrame;
 out->simulationFrame=gs->frameNum;out->captureCamera=frame.viewCamera;out->wireframe=wireframe;
 out->viewProjection=std::to_array(camera->GetViewProjectionMatrix().m);
 out->fogColor=OriginalNativeModelContext().fog.color;out->fogParameters=OriginalNativeModelContext().fog.parameters;
 out->fogEnabled=OriginalNativeModelContext().fogEnabled;
 // Original bump pass never loads a texture matrix. Its source UVs require
 // actual identity on unit0 for the current native fixed-function profile.
 OriginalNativeLegacyWaterTextureMatrices().RequireIdentity(0);
 OriginalNativeLegacyWaterLighting().RequireDisabled();
 OriginalNativeLegacyWaterTexGen().RequireDisabled(0,11);
 const auto environment=OriginalNativeLegacyWaterTextureEnvironment().Snapshot(0);
 if(!environment.observed||environment.value!=NativeLegacyWaterTextureEnvironment::Modulate)throw std::runtime_error("Legacy bump texture environment is not observed MODULATE");
 const auto& textures=OriginalNativeTextureContext().History();
 if(textures.ActiveUnit()!=std::optional<unsigned>(0))throw std::runtime_error("Legacy bump requires actual source active unit0");
 // UpdateWater only enables 2D on the current unit; it does not disable any
 // inherited other fixed-function stages. The native five-layer pass models
 // only this one stage, so do not silently discard another enabled target.
 for(unsigned unit=0;unit<NativeCUSTextureHistory::Units;++unit)for(unsigned target=0;target<unsigned(NativeCUSTextureTarget::Count);++target){
  if(unit==0&&target==unsigned(NativeCUSTextureTarget::Image2D))continue;
  const auto binding=textures.Binding(unit,static_cast<NativeCUSTextureTarget>(target));
  if(!binding.enableObserved||binding.enabled)throw std::runtime_error("Legacy bump has unobserved or additional enabled texture stages");
 }
 auto bump=std::make_shared<NativeLegacyWaterBumpFrame>();bump->sourceDrawFrame=frame.sourceDrawFrame;
 bump->simulationFrame=gs->frameNum;bump->material=material;
 const auto state=ui::GetRecorder().CurrentState().draw;
 const auto world=ui::GetRecorder().CurrentWorldState();
 if(state.alphaTest.enabled||state.blend.colorEquation!=ScreenBlendEquation::Add||state.blend.alphaEquation!=ScreenBlendEquation::Add||
  state.colorWrite!=std::array<bool,4>{true,true,true,true}||world.colorWrite!=std::array<bool,4>{true,true,true,true}||
  world.stencilTest||world.alphaToCoverage||world.polygonOffset||world.inheritedClipPlane||
  world.frontPolygonMode!=world.backPolygonMode||world.frontPolygonMode==NativePolygonMode::Point)
  throw std::runtime_error("Legacy water bump inherited raster state is outside the validated source contract");
 bump->fogEnabled=out->fogEnabled;bump->fogColor=out->fogColor;bump->fogParameters=out->fogParameters;
 bump->cull=world.cullEnabled?static_cast<LegacyWaterBumpCull>(world.cull):LegacyWaterBumpCull::None;
 bump->wireframe=world.frontPolygonMode==NativePolygonMode::Line;bump->scissorEnabled=state.scissor.has_value();
 bump->sourceStateObserved=true;ValidateNativeLegacyWaterBump(*bump);out->bump=std::move(bump);

 return out;
}

std::shared_ptr<const NativeLegacyWaterFrame> NativeLegacyWaterCapture::FinalizeSurface(std::shared_ptr<const NativeLegacyWaterFrame> source,const WorldCamera& actual,bool wireframe)
{
 if(!source||!source->bump||source->bump->material!=material||!camera)throw std::logic_error("Legacy water surface lost its bump source owner");
 const auto state=ui::GetRecorder().CurrentState().draw;
 const auto world=ui::GetRecorder().CurrentWorldState();
 if((refractionTarget==LegacyRefractionTarget::Unobserved && (state.blend.colorEquation!=ScreenBlendEquation::Add||state.blend.alphaEquation!=ScreenBlendEquation::Add||
  state.blend.sourceColor!=ScreenBlendFactor::SourceAlpha||state.blend.sourceAlpha!=ScreenBlendFactor::SourceAlpha||
  state.blend.destinationColor!=ScreenBlendFactor::OneMinusSourceAlpha||state.blend.destinationAlpha!=ScreenBlendFactor::OneMinusSourceAlpha))||
  !world.depthTest||world.depthCompare!=ScreenAlphaCompare::LessEqual||world.depthClamp||
  state.colorWrite!=std::array<bool,4>{true,true,true,true}||world.colorWrite!=std::array<bool,4>{true,true,true,true}||
  state.scissor||world.stencilTest||world.alphaToCoverage||world.polygonOffset||world.inheritedClipPlane)
  throw std::runtime_error("Legacy water surface inherited raster state is outside the validated source contract");
 // With identity texture matrices the ARB TEX2D surface reads only S/T.
 // Draw overwrites unit1 S/T (and refractive3 S/T); inherited R/Q cannot
 // affect those XY coordinates. Unit0 S/T must remain the explicit VA input.
 OriginalNativeLegacyWaterLighting().RequireDisabled();
 OriginalNativeLegacyWaterTexGen().RequireDisabled(0,3);
 // Read after reflection callbacks, at the actual original Draw boundary.
 OriginalNativeLegacyWaterTextureMatrices().RequireIdentity(0);
 OriginalNativeLegacyWaterTextureMatrices().RequireIdentity(1);
 if(refractionTarget!=LegacyRefractionTarget::Unobserved)OriginalNativeLegacyWaterTextureMatrices().RequireIdentity(3);
 auto out=std::make_shared<NativeLegacyWaterFrame>(*source);
 // Surface Draw follows reflection callbacks: never reuse pre-reflection camera/mesh/fog.
 out->captureCamera=actual;out->viewProjection=std::to_array(camera->GetViewProjectionMatrix().m);
 out->simulationFrame=gs->frameNum;out->wireframe=wireframe;
 out->fogColor=OriginalNativeModelContext().fog.color;out->fogParameters=OriginalNativeModelContext().fog.parameters;
 out->fogEnabled=OriginalNativeModelContext().fogEnabled;
 out->vertices.clear();out->indices.clear();
 if(refractionTarget!=LegacyRefractionTarget::Unobserved){
  auto refract=std::make_shared<NativeLegacyRefraction>();refract->target=refractionTarget;refract->allocation=refractionAllocation;
  refract->copyViewport={globalRendering->viewPosX,globalRendering->viewPosY,globalRendering->viewSizeX,globalRendering->viewSizeY};
  // Source allocated only at LoadGfx. Reject resize/undefined padding rather than reallocating or fabricating pixels.
  if(refract->copyViewport[2]<=0||refract->copyViewport[3]<=0||uint32_t(refract->copyViewport[2])!=refractionAllocation[0]||uint32_t(refract->copyViewport[3])!=refractionAllocation[1])
   throw std::runtime_error("Refractive resize or unwritten POT padding is outside validated coverage");
  if(mapDims.mapxp1<=0||mapDims.mapyp1<=0)throw std::runtime_error("Refractive object texgen map extent missing");
  refract->inverseMapExtent={1.f/(mapDims.mapxp1*SQUARE_SIZE),1.f/(mapDims.mapyp1*SQUARE_SIZE)};
  // Resolve the exact GetShadingTexture name, including a real Lua override. Unsupported backings reject.
  refract->shading.object=OriginalNativeTextureContext().Registry().Find(readMap->GetShadingTexture());
  if(!refract->shading.object||!std::dynamic_pointer_cast<const NativeTerrainShadingBacking>(refract->shading.object->backing))
   throw std::runtime_error("Refractive shading lacks its actual retained map producer");
  refract->shading.knowledge=NativeCUSBindingKnowledge::BoundObject;refract->shading.enabled=refract->shading.enableObserved=true;
  refract->copiedColor=refractionPublication.Copy(source->sourceDrawFrame);
  out->refraction=std::move(refract);
 }

	float3 base = camera->CalcPixelDir(globalRendering->viewPosX, globalRendering->viewSizeY);
	float3 dv   = camera->CalcPixelDir(globalRendering->viewPosX, 0) - camera->CalcPixelDir(globalRendering->viewPosX, globalRendering->viewSizeY);
	float3 dh   = camera->CalcPixelDir(globalRendering->viewPosX + globalRendering->viewSizeX, 0) - camera->CalcPixelDir(globalRendering->viewPosX, 0);

	float3 xbase;
	float3 forward = camera->GetDir();
	float3 dir;
	float3 zpos;

	const int numDivs = 20;

	base *= numDivs;
	float maxY = -0.1f;
	float yInc = 1.0f / numDivs;
	float screenY = 1.0f;


 unsigned char col[4];
 for(unsigned i=0;i<3;++i)col[i]=(unsigned char)(surfaceColor[i]*255);
 forward.ANormalize2D();out->forward={forward.z,forward.x};
 size_t stripStart=0;
 const auto AddVertex=[&](const float3& p,float u,float v,const unsigned char* c){
  out->vertices.push_back({{p.x,p.y,p.z},{u,v},{c[0],c[1],c[2],c[3]}});
 };
 const auto EndStrip=[&]{
  const size_t end=out->vertices.size();
  for(size_t i=stripStart+2;i<end;++i){
   // Preserve GL triangle-strip alternating front-face order.
   if((i-stripStart)&1)out->indices.insert(out->indices.end(),{uint32_t(i-1),uint32_t(i-2),uint32_t(i)});
   else out->indices.insert(out->indices.end(),{uint32_t(i-2),uint32_t(i-1),uint32_t(i)});
  }
  stripStart=end;
 };
	for (int a = 0; a < 5; ++a) { //! CAUTION: loop count must match EnlargeArrays above
		bool maxReached = false;

		for (int y = 0; y < numDivs; ++y) {
			dir = base;
			dir.ANormalize();

			if (dir.y >= maxY) {
				maxReached = true;
				break;
			}

			xbase = base;

			for (int x = 0; x < numDivs + 1; ++x) { //! CAUTION: loop count must match EnlargeArrays above
				dir = xbase + dv;
				dir.ANormalize();

				zpos = camera->GetPos() + dir * (camera->GetPos().y / -dir.y);
				zpos.y = fastmath::sin(zpos.z * 0.1f + gs->frameNum * 0.06f) * 0.06f + 0.05f;

				col[3] = (unsigned char)((0.8f + 0.7f * dir.y) * 255);
				AddVertex(zpos, x * (1.0f / numDivs), screenY - yInc, col);

				dir = xbase;
				dir.ANormalize();

				zpos = camera->GetPos() + dir * (camera->GetPos().y / -dir.y);
				zpos.y = fastmath::sin(zpos.z * 0.1f + gs->frameNum * 0.06f) * 0.06f + 0.05f;

				col[3] = (unsigned char)((0.8f + 0.7f * dir.y) * 255);
				AddVertex(zpos, x * (1.0f / numDivs), screenY, col);

				xbase += dh;
			}
			EndStrip();
			base += dv;
			screenY -= yInc;
		}

		if (!maxReached)
			break;

		dv   *= 0.5f;
		maxY *= 0.5f;
		yInc *= 0.5f;
	}

 out->cull=world.cullEnabled?static_cast<LegacyWaterBumpCull>(world.cull):LegacyWaterBumpCull::None;
 return out;
}
}
