/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SMFRenderState.h"
#include "SMFGroundDrawer.h"
#include "SMFReadMap.h"
#include "Game/Camera.h"
#include "Map/MapInfo.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/CubeMapHandler.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/SkyLight.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Env/MapRendering.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/Map/InfoTexture/IInfoTextureHandler.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "Rendering/Shaders/Shader.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/Config/ConfigHandler.h"
#include "System/StringUtil.h"

#include "System/Misc/TracyDefs.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include <algorithm>
#include <stdexcept>
#include "Rendering/EngineBridge/NativeTerrainProgramSource.h"
#include "Rendering/EngineBridge/NativeTerrainImagePublication.h"
#include "Rendering/Backend/NativeTextureContext.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#endif

static constexpr float SMF_TEXSQUARE_SIZE = 1024.0f;

#ifdef RECOIL_METAL4_LIVE_SCENE
namespace {
[[noreturn]] void UnsupportedNativeCombinedTerrain(bool lua)
{
	throw std::runtime_error(lua
		? "Original Lua combined terrain selection requires the actual deferred program's G-buffer and forward-color MRT consumer; forward substitution is forbidden"
		: "Original SMF_COMBINED_PASS terrain selection requires its actual G-buffer and forward-color MRT consumer; forward substitution is forbidden");
}
}
#endif


ISMFRenderState* ISMFRenderState::GetInstance(bool luaShaders, bool noop) {
	if (noop)
		return new SMFRenderStateNOOP();
	else
		return new SMFRenderStateGLSL(luaShaders);
}

bool SMFRenderStateGLSL::Init(const CSMFGroundDrawer* smfGroundDrawer) {
	RECOIL_DETAILED_TRACY_ZONE;
	const std::string names[GLSL_SHADER_COUNT] = {
		"SMFShaderGLSL-Forward-Std",
		"SMFShaderGLSL-Forward-Adv",
		"SMFShaderGLSL-Deferred-Adv",
		"SMFShaderGLSL-Combined-Adv"
	};
	const std::string defs =
		("#define SMF_TEXSQUARE_SIZE " + FloatToString(                  SMF_TEXSQUARE_SIZE) + "\n") +
		("#define SMF_INTENSITY_MULT " + FloatToString(CGlobalRendering::SMF_INTENSITY_MULT) + "\n");


	if (useLuaShaders) {
		for (uint32_t n = GLSL_SHADER_FWD_ADV; n < GLSL_SHADER_COUNT; n++) {
#ifdef RECOIL_METAL4_LIVE_SCENE
			// Target slot 3 is an unused Lua wrapper; only slots 1 and 2 load shaderIDs.
			nativeShaderOwners[n]=rendering::CreateNativeTerrainProgramSource(
				n==GLSL_SHADER_FWD_ADV?rendering::NativeTerrainProgramSelection::LuaForward:rendering::NativeTerrainProgramSelection::LuaDeferred,
				names[n]+"-Lua",defs);
			glslShaders[n]=nativeShaderOwners[n].get();
#else
			glslShaders[n] = shaderHandler->CreateProgramObject("[SMFGroundDrawer::Lua]", names[n] + "-Lua");
#endif
			// release ID created by GLSLProgramObject's ctor; should be 0
			glslShaders[n]->Release();
		}
	} else {
		// the combined shader is only ever used when combined rendering is enabled
		const uint32_t lastShader = smfGroundDrawer->UseCombinedRendering() ? GLSL_SHADER_CMB_ADV : GLSL_SHADER_DFR_ADV;

		for (uint32_t n = GLSL_SHADER_FWD_STD; n <= lastShader; n++) {
			// load from VFS files
#ifdef RECOIL_METAL4_LIVE_SCENE
			const rendering::NativeTerrainProgramSelection choices[GLSL_SHADER_COUNT]{
				rendering::NativeTerrainProgramSelection::ForwardStandard,
				rendering::NativeTerrainProgramSelection::ForwardAdvanced,
				rendering::NativeTerrainProgramSelection::DeferredAdvanced,
				rendering::NativeTerrainProgramSelection::CombinedAdvanced
			};
			nativeShaderOwners[n]=rendering::CreateNativeTerrainProgramSource(choices[n],names[n],defs);
			glslShaders[n]=nativeShaderOwners[n].get();
#else
			glslShaders[n] = shaderHandler->CreateProgramObject("[SMFGroundDrawer::VFS]", names[n]);
			glslShaders[n]->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFVertProg.glsl", defs, GL_VERTEX_SHADER));
			glslShaders[n]->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFFragProg.glsl", defs, GL_FRAGMENT_SHADER));
#endif
			glslShaders[n]->BindAttribLocation("vertexPos", 0);
		}
	}

	currShader = glslShaders[CanUseAdvShading(smfGroundDrawer, GLSL_SHADER_FWD_ADV) ? GLSL_SHADER_FWD_ADV : GLSL_SHADER_FWD_STD];
	return true;
}

void SMFRenderStateGLSL::Kill() {
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	for(auto& owner:nativeShaderOwners){if(owner)owner->Release();owner.reset();}
	glslShaders.fill(nullptr);currShader=nullptr;
	return;
#endif
	if (useLuaShaders) {
		// make sure SH deletes only the wrapper objects; programs are managed by LuaShaders
		for (uint32_t n = GLSL_SHADER_FWD_STD; n < GLSL_SHADER_COUNT; n++) {
			if (glslShaders[n] != nullptr) {
				glslShaders[n]->LoadFromID(0);
			}
		}

		shaderHandler->ReleaseProgramObjects("[SMFGroundDrawer::Lua]");
	} else {
		shaderHandler->ReleaseProgramObjects("[SMFGroundDrawer::VFS]");
	}
}

void SMFRenderStateGLSL::Update(
	const CSMFGroundDrawer* smfGroundDrawer,
	const LuaMapShaderData* luaMapShaderData
) {
	RECOIL_DETAILED_TRACY_ZONE;
	if (useLuaShaders) {
		assert(luaMapShaderData != nullptr);

		// load from LuaShader ID; should be a linked and valid program (or 0)
		// NOTE: only non-custom shaders get to have engine flags and uniforms!
		for (uint32_t n = GLSL_SHADER_FWD_ADV; n <= GLSL_SHADER_DFR_ADV; n++) {
#ifdef RECOIL_METAL4_LIVE_SCENE
			rendering::LoadNativeTerrainLuaProgram(glslShaders[n],luaMapShaderData->shaderIDs[n-1]);
#else
			glslShaders[n]->LoadFromID(luaMapShaderData->shaderIDs[n - 1]);
#endif
		}

		luaCombinedShader = luaMapShaderData->combinedShader;

		// currShader is null from Init() (GLSL_SHADER_FWD_STD is never created for
		// the Lua state), so re-evaluate it now that program IDs changed; otherwise
		// HasValidShader(Normal) stays false and a forward-only Lua map shader is
		// never selected until a deferred Lua draw happens to run first
		SetCurrentShader(smfGroundDrawer, DrawPass::Normal);
	} else {
		assert(luaMapShaderData == nullptr);

		const CSMFReadMap* smfMap = smfGroundDrawer->GetReadMap();

		const int2 normTexSize = smfMap->GetTextureSize(MAP_BASE_NORMALS_TEX);
		// const int2 specTexSize = smfMap->GetTextureSize(MAP_SSMF_SPECULAR_TEX);

		const uint32_t lastShader = smfGroundDrawer->UseCombinedRendering() ? GLSL_SHADER_CMB_ADV : GLSL_SHADER_DFR_ADV;

		for (uint32_t n = GLSL_SHADER_FWD_STD; n <= lastShader; n++) {
			const bool isAdv = (n != GLSL_SHADER_FWD_STD);

			if (isAdv) {
				glslShaders[n]->SetFlag("SMF_ADV_SHADING",                      true);
				glslShaders[n]->SetFlag("SMF_VOID_WATER",                       mapRendering->voidWater);
				glslShaders[n]->SetFlag("SMF_VOID_GROUND",                      mapRendering->voidGround);
				glslShaders[n]->SetFlag("SMF_SPECULAR_LIGHTING",                smfMap->GetSpecularTexture() != 0);
				glslShaders[n]->SetFlag("SMF_DETAIL_TEXTURE_SPLATTING",        (smfMap->GetSplatDistrTexture() != 0 && smfMap->GetSplatDetailTexture() != 0));
				glslShaders[n]->SetFlag("SMF_DETAIL_NORMAL_TEXTURE_SPLATTING", (smfMap->GetSplatDistrTexture() != 0 && smfMap->HaveSplatNormalTexture()));
				glslShaders[n]->SetFlag("SMF_DETAIL_NORMAL_DIFFUSE_ALPHA",      mapRendering->splatDetailNormalDiffuseAlpha);
				glslShaders[n]->SetFlag("SMF_WATER_ABSORPTION",                 smfMap->HasVisibleWater());
				glslShaders[n]->SetFlag("SMF_SKY_REFLECTIONS",                  smfMap->GetSkyReflectModTexture() != 0);
				glslShaders[n]->SetFlag("SMF_BLEND_NORMALS",                    smfMap->GetBlendNormalsTexture() != 0);
				glslShaders[n]->SetFlag("SMF_LIGHT_EMISSION",                   smfMap->GetLightEmissionTexture() != 0);
				glslShaders[n]->SetFlag("SMF_PARALLAX_MAPPING",                 smfMap->GetParallaxHeightTexture() != 0);
			}

			// both are runtime set in ::Enable, but AMD drivers need values from the beginning
			glslShaders[n]->SetFlag("HAVE_SHADOWS", false);
			glslShaders[n]->SetFlag("HAVE_INFOTEX", false);

			if (n == GLSL_SHADER_DFR_ADV || n == GLSL_SHADER_CMB_ADV) {
				glslShaders[n]->SetFlag((n == GLSL_SHADER_DFR_ADV) ? "DEFERRED_MODE" : "SMF_COMBINED_PASS", true);
				glslShaders[n]->SetFlag("GBUFFER_NORMTEX_IDX", GL::GeometryBuffer::ATTACHMENT_NORMTEX);
				glslShaders[n]->SetFlag("GBUFFER_DIFFTEX_IDX", GL::GeometryBuffer::ATTACHMENT_DIFFTEX);
				glslShaders[n]->SetFlag("GBUFFER_SPECTEX_IDX", GL::GeometryBuffer::ATTACHMENT_SPECTEX);
				glslShaders[n]->SetFlag("GBUFFER_EMITTEX_IDX", GL::GeometryBuffer::ATTACHMENT_EMITTEX);
				glslShaders[n]->SetFlag("GBUFFER_MISCTEX_IDX", GL::GeometryBuffer::ATTACHMENT_MISCTEX);
				glslShaders[n]->SetFlag("GBUFFER_COLORTEX_IDX", GL::GeometryBuffer::ATTACHMENT_COLORTEX);
				glslShaders[n]->SetFlag("GBUFFER_ZVALTEX_IDX", GL::GeometryBuffer::ATTACHMENT_ZVALTEX);
			}

			glslShaders[n]->Link();
			glslShaders[n]->Enable();

			glslShaders[n]->SetUniform("diffuseTex",             0);
			glslShaders[n]->SetUniform("heightMapTex",           1);
			glslShaders[n]->SetUniform("detailTex",              2);
			glslShaders[n]->SetUniform("infoTex",               14);
			if (isAdv) {
				glslShaders[n]->SetUniform("shadowTex", 4);
				glslShaders[n]->SetUniform("normalsTex", 5);
				glslShaders[n]->SetUniform("specularTex", 6);
				glslShaders[n]->SetUniform("splatDetailTex", 7);
				glslShaders[n]->SetUniform("splatDistrTex", 8);
				glslShaders[n]->SetUniform("skyReflectTex", 9);
				glslShaders[n]->SetUniform("skyReflectModTex", 10);
				glslShaders[n]->SetUniform("blendNormalsTex", 11);
				glslShaders[n]->SetUniform("lightEmissionTex", 12);
				glslShaders[n]->SetUniform("parallaxHeightTex", 13);
				glslShaders[n]->SetUniform("splatDetailNormalTex1", 15);
				glslShaders[n]->SetUniform("splatDetailNormalTex2", 16);
				glslShaders[n]->SetUniform("splatDetailNormalTex3", 17);
				glslShaders[n]->SetUniform("splatDetailNormalTex4", 18);
				glslShaders[n]->SetUniform("shadowColorTex", 19);

				glslShaders[n]->SetUniform4v("lightDir", &ISky::GetSky()->GetLight()->GetLightDir()[0]);
				glslShaders[n]->SetUniform3v("cameraPos", &camera->GetPos()[0]);

				glslShaders[n]->SetUniform3v("groundAmbientColor", &sunLighting->groundAmbientColor[0]);
				glslShaders[n]->SetUniform3v("groundDiffuseColor", &sunLighting->groundDiffuseColor[0]);
				glslShaders[n]->SetUniform3v("groundSpecularColor", &sunLighting->groundSpecularColor[0]);
				glslShaders[n]->SetUniform("groundSpecularExponent", sunLighting->specularExponent);
				glslShaders[n]->SetUniform("groundShadowDensity", sunLighting->groundShadowDensity);

				glslShaders[n]->SetUniformMatrix4x4("shadowMat", false, shadowHandler.GetShadowMatrixRaw());

				glslShaders[n]->SetUniform3v("waterMinColor", &waterRendering->minColor[0]);
				glslShaders[n]->SetUniform3v("waterBaseColor", &waterRendering->baseColor[0]);
				glslShaders[n]->SetUniform3v("waterAbsorbColor", &waterRendering->absorb[0]);

				glslShaders[n]->SetUniform4v("splatTexScales", &mapRendering->splatTexScales[0]);
				glslShaders[n]->SetUniform4v("splatTexMults", &mapRendering->splatTexMults[0]);

				glslShaders[n]->SetUniform("normalTexGen", 1.0f / ((normTexSize.x - 1) * SQUARE_SIZE), 1.0f / ((normTexSize.y - 1) * SQUARE_SIZE));
			}
			else {
				glslShaders[n]->SetUniform("shadingTex", 3);
			}

			glslShaders[n]->SetUniform("infoTexGen", 1.0f / (mapDims.pwr2mapx * SQUARE_SIZE), 1.0f / (mapDims.pwr2mapy * SQUARE_SIZE));
			glslShaders[n]->SetUniform("infoTexIntensityMul", 1.0f);
			glslShaders[n]->SetUniform("specularTexGen", 1.0f / (mapDims.mapx * SQUARE_SIZE), 1.0f / (mapDims.mapy * SQUARE_SIZE));

			glslShaders[n]->Disable();
			glslShaders[n]->Validate();
		}
	}
}

bool SMFRenderStateGLSL::HasValidShader(const DrawPass::e& drawPass) const {
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	// An explicit combined query must not borrow the currently selected forward owner.
	if (drawPass == DrawPass::TerrainCombined)
		if(useLuaShaders)UnsupportedNativeCombinedTerrain(true);
#endif
	Shader::IProgramObject* shader = (drawPass == DrawPass::TerrainDeferred) ? glslShaders[GLSL_SHADER_DFR_ADV] :
		(drawPass == DrawPass::TerrainCombined) ? glslShaders[useLuaShaders?GLSL_SHADER_DFR_ADV:GLSL_SHADER_CMB_ADV] : currShader;
#ifdef RECOIL_METAL4_LIVE_SCENE
	return rendering::RequireNativeTerrainProgramValid(shader);
#else
	return (shader != nullptr && shader->IsValid());
#endif
}

bool SMFRenderStateGLSL::CanDrawDeferred(const CSMFGroundDrawer* smfGroundDrawer) const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return CanUseAdvShading(smfGroundDrawer, GLSL_SHADER_DFR_ADV);
}

bool SMFRenderStateGLSL::CanDrawCombined(const CSMFGroundDrawer* smfGroundDrawer) const
{
	RECOIL_DETAILED_TRACY_ZONE;
	// Lua shaders have to declare combined support (Spring.SetMapShader)
	if (useLuaShaders)
		return (luaCombinedShader && CanUseAdvShading(smfGroundDrawer, GLSL_SHADER_DFR_ADV));

	return CanUseAdvShading(smfGroundDrawer, GLSL_SHADER_CMB_ADV);
}

void SMFRenderStateGLSL::Enable(const CSMFGroundDrawer* smfGroundDrawer, const DrawPass::e& drawPass) {
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	// SourceProgram::Enable remains usable for initial named uniform writes.
	// A render-pass enable, including Lua EnableRaw, is an actual draw selection.
	if (drawPass == DrawPass::TerrainCombined)
		if(useLuaShaders)UnsupportedNativeCombinedTerrain(true);
#endif
	if (useLuaShaders) {
		// use raw, GLSLProgramObject::Enable also calls RecompileIfNeeded
		currShader->EnableRaw();
		// diffuse textures are always bound (SMFGroundDrawer::SetupBigSquare)
#ifdef RECOIL_METAL4_LIVE_SCENE
		rendering::OriginalNativeTextureContext().History().ActiveTexture(0);
#else
		glActiveTexture(GL_TEXTURE0);
#endif
		return;
	}

	const auto shaderStage =
		(drawPass == DrawPass::TerrainDeferred) ? GLSL_SHADER_DFR_ADV :
		(drawPass == DrawPass::TerrainCombined) ? GLSL_SHADER_CMB_ADV : GLSL_SHADER_FWD_ADV;
	const bool isAdv = CanUseAdvShading(smfGroundDrawer, shaderStage);

	const CSMFReadMap* smfMap = smfGroundDrawer->GetReadMap();

#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& history=rendering::OriginalNativeTextureContext().History();
	auto& registry=rendering::OriginalNativeTextureContext().Registry();
	auto& images=rendering::OriginalNativeTerrainImagePublication();
	auto& recorder=rendering::ui::GetRecorder();
	// Original glLoadIdentity/glMultMatrix affect the ACTUAL current stack.
	// Do not add a ModelView selector that the source did not write.
	recorder.LoadMatrix(rendering::ui::IdentityMatrix());
	const float* actualView=camera->GetViewMatrix();
	rendering::ui::NativeUIMatrix view;std::copy_n(actualView,16,view.begin());recorder.MultMatrix(view);
	const auto mapBind=[&](unsigned unit,rendering::NativeTerrainImageRole role){
		history.ActiveTexture(unit);smfMap->BindNativeTerrainTexture(role);
	};
	if(isAdv&&shadowHandler.ShadowsLoaded()){
		history.ActiveTexture(4);images.BindMap(&shadowHandler,rendering::NativeTerrainImageRole::ShadowDepth,shadowHandler.GetShadowTextureID());
		history.Enable(4,rendering::NativeCUSTextureTarget::Image2D,true);
		const auto binding=history.Binding(4,rendering::NativeCUSTextureTarget::Image2D);
		auto sampling=binding.object?binding.object->sampling:binding.defaultSampling;
		sampling.compareMode=GL_COMPARE_R_TO_TEXTURE;sampling.compareFunction=GL_LEQUAL;sampling.depthMode=GL_LUMINANCE;
		registry.SetBoundSampling(4,rendering::NativeCUSTextureTarget::Image2D,sampling);
		history.ActiveTexture(19);images.BindMap(&shadowHandler,rendering::NativeTerrainImageRole::ShadowColor,shadowHandler.GetColorTextureID());
	}
	mapBind(1,rendering::NativeTerrainImageRole::Height);
	mapBind(2,rendering::NativeTerrainImageRole::Detail);
	history.ActiveTexture(14);images.BindMap(infoTextureHandler.get(),rendering::NativeTerrainImageRole::Info,infoTextureHandler->GetCurrentInfoTexture());
	if(isAdv){
		mapBind(5,rendering::NativeTerrainImageRole::Normals);mapBind(6,rendering::NativeTerrainImageRole::Specular);
		mapBind(7,rendering::NativeTerrainImageRole::SplatDetail);mapBind(8,rendering::NativeTerrainImageRole::SplatDistribution);
		history.ActiveTexture(9);images.BindMap(&cubeMapHandler,rendering::NativeTerrainImageRole::SkyReflection,cubeMapHandler.GetSkyReflectionTextureID());
		mapBind(10,rendering::NativeTerrainImageRole::SkyReflectionModulation);mapBind(11,rendering::NativeTerrainImageRole::BlendNormals);
		mapBind(12,rendering::NativeTerrainImageRole::Emission);mapBind(13,rendering::NativeTerrainImageRole::ParallaxHeight);
		for(int i=0;i<CSMFReadMap::NUM_SPLAT_DETAIL_NORMALS;++i)if(smfMap->GetSplatNormalTexture(i)!=0)
			mapBind(15+i,rendering::NativeTerrainImageRole(unsigned(rendering::NativeTerrainImageRole::SplatNormal0)+i));
	}else mapBind(3,rendering::NativeTerrainImageRole::Shading);
	history.ActiveTexture(0);
#else
	// already on the MV stack at this point
	glLoadIdentity();
	glMultMatrixf(camera->GetViewMatrix());

	if (isAdv && shadowHandler.ShadowsLoaded()) {
		shadowHandler.SetupShadowTexSampler(GL_TEXTURE4, true);
		glActiveTexture(GL_TEXTURE19); glBindTexture(GL_TEXTURE_2D, shadowHandler.GetColorTextureID());
	}

	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, smfMap->GetHeightMapTexture());
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, smfMap->GetDetailTexture());
	glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, infoTextureHandler->GetCurrentInfoTexture());
	if (isAdv) {
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, smfMap->GetNormalsTexture());
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, smfMap->GetSpecularTexture());
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, smfMap->GetSplatDetailTexture());
		glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_2D, smfMap->GetSplatDistrTexture());
		glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, cubeMapHandler.GetSkyReflectionTextureID());
		glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, smfMap->GetSkyReflectModTexture());
		glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, smfMap->GetBlendNormalsTexture());
		glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, smfMap->GetLightEmissionTexture());
		glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, smfMap->GetParallaxHeightTexture());

		for (int i = 0; i < CSMFReadMap::NUM_SPLAT_DETAIL_NORMALS; i++) {
			if (smfMap->GetSplatNormalTexture(i) != 0) {
				glActiveTexture(GL_TEXTURE15 + i); glBindTexture(GL_TEXTURE_2D, smfMap->GetSplatNormalTexture(i));
			}
		}
	}
	else {
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, smfMap->GetShadingTexture());
	}

	glActiveTexture(GL_TEXTURE0);
#endif

	if (isAdv)
		currShader->SetFlag("HAVE_SHADOWS", shadowHandler.ShadowsLoaded());

	currShader->SetFlag("HAVE_INFOTEX", infoTextureHandler->IsEnabled());

	currShader->Enable();
	currShader->SetUniform("mapHeights", readMap->GetCurrMinHeight(), readMap->GetCurrMaxHeight());
	currShader->SetUniform("infoTexIntensityMul", float(infoTextureHandler->InMetalMode()) + 1.0f);

	if (isAdv) {
		currShader->SetUniform3v("cameraPos", &camera->GetPos()[0]);
		if (shadowHandler.ShadowsLoaded())
			currShader->SetUniformMatrix4x4("shadowMat", false, shadowHandler.GetShadowMatrixRaw());
	}
}

void SMFRenderStateGLSL::Disable(const CSMFGroundDrawer* smfGroundDrawer, const DrawPass::e& drawPass) {
	RECOIL_DETAILED_TRACY_ZONE;
	if (useLuaShaders) {
#ifdef RECOIL_METAL4_LIVE_SCENE
		rendering::OriginalNativeTextureContext().History().ActiveTexture(0);
#else
		glActiveTexture(GL_TEXTURE0);
#endif
		currShader->DisableRaw();
		return;
	}

	currShader->Disable();

	const auto shaderStage =
		(drawPass == DrawPass::TerrainDeferred) ? GLSL_SHADER_DFR_ADV :
		(drawPass == DrawPass::TerrainCombined) ? GLSL_SHADER_CMB_ADV : GLSL_SHADER_FWD_ADV;
	const bool isAdv = CanUseAdvShading(smfGroundDrawer, shaderStage);

	const CSMFReadMap* smfMap = smfGroundDrawer->GetReadMap();

#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& history=rendering::OriginalNativeTextureContext().History();
	auto& registry=rendering::OriginalNativeTextureContext().Registry();
	auto& images=rendering::OriginalNativeTerrainImagePublication();
	const auto clear=[&](unsigned unit,rendering::NativeCUSTextureTarget target=rendering::NativeCUSTextureTarget::Image2D){
		history.ActiveTexture(unit);images.BindDefault(target);
	};
	if(isAdv&&shadowHandler.ShadowsLoaded()){
		clear(4);history.Enable(4,rendering::NativeCUSTextureTarget::Image2D,false);
		auto sampling=history.Binding(4,rendering::NativeCUSTextureTarget::Image2D).defaultSampling;
		sampling.compareMode=GL_NONE;sampling.depthMode=GL_LUMINANCE;
		registry.SetBoundSampling(4,rendering::NativeCUSTextureTarget::Image2D,sampling);
		clear(19);
	}
	clear(1);clear(2);clear(14);
	if(isAdv){
		clear(5);clear(6);clear(7);clear(8);clear(9,rendering::NativeCUSTextureTarget::Cube);
		clear(10);clear(11);clear(12);clear(13);
		for(int i=0;i<CSMFReadMap::NUM_SPLAT_DETAIL_NORMALS;++i)if(smfMap->GetSplatNormalTexture(i)!=0)clear(15+i);
	}else clear(3);
	history.ActiveTexture(0);
#else
	if (isAdv && shadowHandler.ShadowsLoaded()) {
		shadowHandler.ResetShadowTexSampler(GL_TEXTURE4, true);
		glActiveTexture(GL_TEXTURE19); glBindTexture(GL_TEXTURE_2D, 0);
	}

	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE14); glBindTexture(GL_TEXTURE_2D, 0);

	if (isAdv) {
		glActiveTexture(GL_TEXTURE5); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE6); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE7); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE8); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE9); glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, 0);
		glActiveTexture(GL_TEXTURE10); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE11); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE12); glBindTexture(GL_TEXTURE_2D, 0);
		glActiveTexture(GL_TEXTURE13); glBindTexture(GL_TEXTURE_2D, 0);

		for (int i = 0; i < CSMFReadMap::NUM_SPLAT_DETAIL_NORMALS; i++) {
			if (smfMap->GetSplatNormalTexture(i) != 0) {
				glActiveTexture(GL_TEXTURE15 + i); glBindTexture(GL_TEXTURE_2D, 0);
			}
		}
	}
	else {
		glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, 0);
	}

	glActiveTexture(GL_TEXTURE0);
#endif
}

void SMFRenderStateGLSL::SetSquareTexGen(const int sqx, const int sqy) const {
	RECOIL_DETAILED_TRACY_ZONE;
	// needs to be set even for Lua shaders, is unknowable otherwise
	// (works because SMFGroundDrawer::SetupBigSquare always calls us)
	currShader->SetUniform("texSquare", sqx, sqy);
}

void SMFRenderStateGLSL::SetCurrentShader(const CSMFGroundDrawer* smfGroundDrawer, const DrawPass::e& drawPass) {
	RECOIL_DETAILED_TRACY_ZONE;
	if (drawPass == DrawPass::TerrainDeferred) {
		currShader = glslShaders[GLSL_SHADER_DFR_ADV];
		return;
	}

	if (drawPass == DrawPass::TerrainCombined) {
		// Lua shaders have no separate combined program, their deferred one writes the color too
		currShader = glslShaders[useLuaShaders ? GLSL_SHADER_DFR_ADV : GLSL_SHADER_CMB_ADV];
#ifdef RECOIL_METAL4_LIVE_SCENE
		if(useLuaShaders)UnsupportedNativeCombinedTerrain(true);
#endif
		return;
	}

	if (CanUseAdvShading(smfGroundDrawer, GLSL_SHADER_FWD_ADV))
		currShader = glslShaders[GLSL_SHADER_FWD_ADV];
	else
		currShader = glslShaders[GLSL_SHADER_FWD_STD];
}

void SMFRenderStateGLSL::UpdateShaderSkyUniforms()
{
	RECOIL_DETAILED_TRACY_ZONE;
	assert(currShader && !currShader->IsBound());

	for (uint32_t n = GLSL_SHADER_FWD_ADV; n < GLSL_SHADER_COUNT; n++) {
		if (glslShaders[n] == nullptr)
			continue;

		glslShaders[n]->Enable();
		glslShaders[n]->SetUniform4v("lightDir", &ISky::GetSky()->GetLight()->GetLightDir().x);
		glslShaders[n]->SetUniform("groundShadowDensity", sunLighting->groundShadowDensity);
		glslShaders[n]->SetUniform3v("groundAmbientColor", &sunLighting->groundAmbientColor[0]);
		glslShaders[n]->SetUniform3v("groundDiffuseColor", &sunLighting->groundDiffuseColor[0]);
		glslShaders[n]->SetUniform3v("groundSpecularColor", &sunLighting->groundSpecularColor[0]);
		glslShaders[n]->Disable();
	}
}

bool SMFRenderStateGLSL::CanUseAdvShading(const CSMFGroundDrawer* smfGroundDrawer, ShaderStage shStage) const
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	return smfGroundDrawer->UseAdvShading() && glslShaders[shStage] != nullptr && rendering::RequireNativeTerrainProgramValid(glslShaders[shStage]);
#else
	return smfGroundDrawer->UseAdvShading() && glslShaders[shStage] != nullptr && glslShaders[shStage]->IsValid();
#endif
}
