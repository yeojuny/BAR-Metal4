/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "SMFReadMap.h"
#include "SMFGroundDrawer.h"
#include "SMFGroundTextures.h"
#include "SMFRenderState.h"
#include "Game/Camera.h"
#include "Map/MapInfo.h"
#include "Map/ReadMap.h"
#include "Map/SMF/Basic/BasicMeshDrawer.h"
#include "Map/SMF/ROAM/RoamMeshDrawer.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Env/MapRendering.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Shaders/Shader.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/EngineBridge/NativeWaterGroundSourceCapture.h"
#pragma push_macro("camera")
#undef camera
#include "Rendering/EngineBridge/NativeOriginalShadowCapture.h"
#pragma pop_macro("camera")
#include "Rendering/EngineBridge/NativeTerrainImageSource.h"
#include "Rendering/EngineBridge/NativeTerrainProgramSource.h"
#include "System/ScopedResource.h"
#include <stdexcept>
#endif
#include "System/Config/ConfigHandler.h"
#include "System/EventHandler.h"
#include "System/FastMath.h"
#include "System/SafeUtil.h"
#include "System/Log/ILog.h"
#include "System/SpringMath.h"

#include "System/Misc/TracyDefs.h"

//Basic, ROAM
static constexpr int MIN_GROUND_DETAIL[] = {                               0,   4};
static constexpr int MAX_GROUND_DETAIL[] = {CBasicMeshDrawer::LOD_LEVELS - 1, 200};

CONFIG(int, GroundDetail)
	.defaultValue(60)
	.headlessValue(0)
	.minimumValue(MIN_GROUND_DETAIL[1])
	.maximumValue(MAX_GROUND_DETAIL[1])
	.description("Controls how detailed the map geometry will be. On lowered settings, cliffs may appear to be jagged or \"melting\".");
CONFIG(bool, MapBorder).defaultValue(true).description("Draws a solid border at the edges of the map.");


CONFIG(int, MaxDynamicMapLights)
	.defaultValue(1)
	.minimumValue(0).description("Maximum number of map-global dynamic lights that will be rendered at once. High numbers of lights cost performance, as they affect every map fragment.");

CONFIG(bool, AdvMapShading).defaultValue(true).safemodeValue(false).description("Enable shaders for terrain rendering.");
CONFIG(bool, AllowDeferredMapRendering).defaultValue(false).safemodeValue(false).description("Enable rendering the map to the map deferred buffers.");
CONFIG(bool, AllowCombinedMapRendering).defaultValue(false).safemodeValue(false).description("Rasterize the map only once when both forward and deferred map rendering are enabled: the deferred pass also produces the lit forward color, which is composited into the main framebuffer instead of drawing the mesh a second time. Requires AllowDeferredMapRendering. Trade-offs: ground-interior pixels are not multi-sampled (unit and feature silhouettes against the ground keep their MSAA), and the G-buffer diffuse contains what is on screen, info-texture overlay included, instead of the plain albedo.");
CONFIG(bool, AllowDrawMapPostDeferredEvents).defaultValue(false).description("Enable DrawGroundPostDeferred Lua callin.");
CONFIG(bool, AllowDrawMapDeferredEvents).defaultValue(false).description("Enable DrawGroundDeferred Lua callin.");


CONFIG(int, ROAM)
	.defaultValue(1)
	.minimumValue(0)
	.maximumValue(1)
	.description("Use ROAM for terrain mesh rendering: 0 to disable, 1=VBO mode to enable.");

CONFIG(bool, AlwaysSendDrawGroundEvents)
	.defaultValue(false)
	.description("Always send DrawGround{Pre,Post}{Forward,Deferred} events");

namespace Shader {
	struct IProgramObject;
}

CSMFGroundDrawer::CSMFGroundDrawer(CSMFReadMap* rm)
	: smfMap(rm)
	, meshDrawer(nullptr)
	, smfRenderStates{}
	, geomBuffer{"GROUNDDRAWER-GBUFFER", configHandler->GetBool("AllowCombinedMapRendering")}
{
#ifdef RECOIL_METAL4_LIVE_SCENE
	// A failed native link leaves this constructor before the map owns the
	// drawer, so its destructor cannot release these raw-pointer resources.
	spring::ScopedResource rollback(this, [](CSMFGroundDrawer* drawer) {
		for (const auto state: {RENDER_STATE_SSP, RENDER_STATE_LUA, RENDER_STATE_NOP})
			ISMFRenderState::FreeInstance(drawer->smfRenderStates[state]);
		// Native state destruction releases each actual program owner and its
		// registration; the selected state is only an alias of those owners.
		drawer->smfRenderStates.fill(nullptr);
		drawer->nativeBorderShader.reset();drawer->borderShader=nullptr;
		drawer->nativeCompositeShader.reset();drawer->compositeShader=nullptr;
		spring::SafeDelete(drawer->groundTextures);
		spring::SafeDelete(drawer->meshDrawer);
	});
#endif
	alwaysDispatchEvents = configHandler->GetBool("AlwaysSendDrawGroundEvents");
	combinedAllowed = configHandler->GetBool("AllowCombinedMapRendering");
	drawerMode = (configHandler->GetInt("ROAM") != 0)? SMF_MESHDRAWER_ROAM: SMF_MESHDRAWER_BASIC;
	groundDetail = configHandler->GetInt("GroundDetail");

	groundTextures = new CSMFGroundTextures(smfMap);
	meshDrawer = SwitchMeshDrawer(drawerMode);

	smfRenderStates[RENDER_STATE_SSP] = ISMFRenderState::GetInstance(false, false);
	smfRenderStates[RENDER_STATE_LUA] = ISMFRenderState::GetInstance( true, false);
	smfRenderStates[RENDER_STATE_NOP] = ISMFRenderState::GetInstance(false,  true);

#ifdef RECOIL_METAL4_LIVE_SCENE
	nativeBorderShader=rendering::CreateNativeTerrainProgramSource(rendering::NativeTerrainProgramSelection::Border,"Border","");
	borderShader=nativeBorderShader.get();
#else
	borderShader = shaderHandler->CreateProgramObject("[SMFGroundDrawer]", "Border");
	borderShader->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFBorderVertProg.glsl", "", GL_VERTEX_SHADER));
	borderShader->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFBorderFragProg.glsl", "", GL_FRAGMENT_SHADER));
#endif
	// Both owners need the original input bindings before their first link.
	borderShader->BindAttribLocation("vertexPos", 0);
	borderShader->BindAttribLocation("vertexCol", 1);
	borderShader->Link();

	borderShader->Enable();
	borderShader->SetUniform("diffuseTex"  , 0);
	borderShader->SetUniform("heightMapTex", 1);
	borderShader->SetUniform("detailsTex"  , 2);
	borderShader->SetUniform("mapSize",
		static_cast<float>(mapDims.mapx * SQUARE_SIZE), static_cast<float>(mapDims.mapy * SQUARE_SIZE),
				   1.0f / (mapDims.mapx * SQUARE_SIZE),            1.0f / (mapDims.mapy * SQUARE_SIZE)
	);
	borderShader->Disable();

	borderShader->Validate();

	if (combinedAllowed) {
#ifdef RECOIL_METAL4_LIVE_SCENE
		nativeCompositeShader=rendering::CreateNativeTerrainProgramSource(rendering::NativeTerrainProgramSelection::Composite,"Composite","");
		compositeShader=nativeCompositeShader.get();
#else
		compositeShader = shaderHandler->CreateProgramObject("[SMFGroundDrawer]", "Composite");
		compositeShader->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFCompositeVertProg.glsl", "", GL_VERTEX_SHADER));
		compositeShader->AttachShaderObject(shaderHandler->CreateShaderObject("GLSL/SMFCompositeFragProg.glsl", "", GL_FRAGMENT_SHADER));
#endif
		compositeShader->Link();

		compositeShader->Enable();
		compositeShader->SetUniform("colorTex", 0);
		compositeShader->SetUniform("depthTex", 1);
		compositeShader->Disable();

		compositeShader->Validate();
	}

	drawForward = true;
	drawDeferred = geomBuffer.Valid();
	drawMapEdges = configHandler->GetBool("MapBorder");
	postDeferredEvents = configHandler->GetBool("AllowDrawMapPostDeferredEvents");
	deferredEvents = configHandler->GetBool("AllowDrawMapDeferredEvents");


	if (smfRenderStates[RENDER_STATE_SSP]->Init(this))
		smfRenderStates[RENDER_STATE_SSP]->Update(this, nullptr);

	// always initialize this state; defer Update (allows re-use)
	smfRenderStates[RENDER_STATE_LUA]->Init(this);

	// note: state must be pre-selected before the first drawn frame
	// Sun*Changed can be called first, e.g. if DynamicSun is enabled
	smfRenderStates[RENDER_STATE_SEL] = SelectRenderState(DrawPass::Normal);

	if (drawDeferred) {
		drawDeferred &= UpdateGeometryBuffer(true);
	}
#ifdef RECOIL_METAL4_LIVE_SCENE
	rollback.Release();
#endif
}

CSMFGroundDrawer::~CSMFGroundDrawer()
{
	RECOIL_DETAILED_TRACY_ZONE;
	// remember which ROAM-mode was enabled (if any)
	configHandler->Set("ROAM", (dynamic_cast<CRoamMeshDrawer*>(meshDrawer) != nullptr)? 1: 0);

	smfRenderStates[RENDER_STATE_SSP]->Kill(); ISMFRenderState::FreeInstance(smfRenderStates[RENDER_STATE_SSP]);
	smfRenderStates[RENDER_STATE_LUA]->Kill(); ISMFRenderState::FreeInstance(smfRenderStates[RENDER_STATE_LUA]);
	smfRenderStates[RENDER_STATE_NOP]->Kill(); ISMFRenderState::FreeInstance(smfRenderStates[RENDER_STATE_NOP]);

	smfRenderStates = { nullptr };

#ifdef RECOIL_METAL4_LIVE_SCENE
	nativeBorderShader.reset();borderShader=nullptr;
#else
	shaderHandler->ReleaseProgramObject("[SMFGroundDrawer]", "Border");
#endif

#ifdef RECOIL_METAL4_LIVE_SCENE
	nativeCompositeShader.reset();compositeShader=nullptr;
#else
	if (compositeShader != nullptr)
		shaderHandler->ReleaseProgramObject("[SMFGroundDrawer]", "Composite");
#endif

	spring::SafeDelete(groundTextures);
	spring::SafeDelete(meshDrawer);
}



IMeshDrawer* CSMFGroundDrawer::SwitchMeshDrawer(int wantedMode)
{
	RECOIL_DETAILED_TRACY_ZONE;
	// toggle
	if (wantedMode <= -1) {
		wantedMode = drawerMode + 1;
		wantedMode %= SMF_MESHDRAWER_LAST;
	}

	if ((wantedMode == drawerMode) && (meshDrawer != nullptr))
		return meshDrawer;

	spring::SafeDelete(meshDrawer);

	switch ((drawerMode = wantedMode)) {
		case SMF_MESHDRAWER_LEGACY: {
			LOG("Legacy Mesh Renderer is no longer available");
		} [[fallthrough]];
		case SMF_MESHDRAWER_BASIC: {
			LOG("Switching to Basic Mesh Rendering");
			meshDrawer = new CBasicMeshDrawer(this);
		} break;
		default: {
			LOG("Switching to ROAM Mesh Rendering");
			meshDrawer = new CRoamMeshDrawer(this);
		} break;
	}

	return meshDrawer;
}

ISMFRenderState* CSMFGroundDrawer::SelectRenderState(const DrawPass::e& drawPass)
{
	RECOIL_DETAILED_TRACY_ZONE;
	// [0] := Lua GLSL, must have a valid shader for this pass
	// [1] := default ARB *or* GLSL, same condition
	const unsigned int stateEnums[2] = {RENDER_STATE_LUA, RENDER_STATE_SSP};

	for (unsigned int n = 0; n < 2; n++) {
		ISMFRenderState* state = smfRenderStates[ stateEnums[n] ];

		if (!state->HasValidShader(drawPass))
			continue;

		return (smfRenderStates[RENDER_STATE_SEL] = state);
	}

	// fallback
	return (smfRenderStates[RENDER_STATE_SEL] = smfRenderStates[RENDER_STATE_NOP]);
}

bool CSMFGroundDrawer::HaveLuaRenderState() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return (smfRenderStates[RENDER_STATE_SEL] == smfRenderStates[RENDER_STATE_LUA]);
}



void CSMFGroundDrawer::DrawDeferredPass(const DrawPass::e& drawPass, bool alphaTest, bool combined)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!geomBuffer.Valid())
		return;

	// some water renderers use FBO's for the reflection pass
	if (drawPass == DrawPass::WaterReflection)
		return;
	// some water renderers use FBO's for the refraction pass
	if (drawPass == DrawPass::WaterRefraction)
		return;
	// CubeMapHandler also uses an FBO for this pass
	if (drawPass == DrawPass::TerrainReflection)
		return;

#ifdef RECOIL_METAL4_LIVE_SCENE
 if(auto* source=rendering::CurrentNativeWaterGroundSourceCapture()) {
  // Keep the original selection. A standard/no-shader state still clears
  // the actual named Map GBuffer, then returns before callbacks/mesh.
  const bool selected=SelectRenderState(DrawPass::TerrainDeferred)->CanDrawDeferred(this);
  const auto shaderPass=combined?DrawPass::TerrainCombined:DrawPass::TerrainDeferred;
  source->BeginDeferred(!selected,combined);source->RecordDeferredClear();
  if(!selected){source->EndDeferred();return;}
  auto& recorder=rendering::ui::GetRecorder();
  smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this,shaderPass);
  smfRenderStates[RENDER_STATE_SEL]->Enable(this,shaderPass);
  if(alphaTest)recorder.SetAlphaTest({true,rendering::ScreenAlphaCompare::Greater,mapInfo->map.voidAlphaMin});
  if(alwaysDispatchEvents||HaveLuaRenderState())
   source->Callback(rendering::NativeGroundCallback::PreDeferred,[]{eventHandler.DrawGroundPreDeferred();});
  meshDrawer->DrawMesh(drawPass);
  if(alphaTest)recorder.SetAlphaTest({false,rendering::ScreenAlphaCompare::Greater,mapInfo->map.voidAlphaMin});
  smfRenderStates[RENDER_STATE_SEL]->Disable(this,drawPass);
  smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this,DrawPass::Normal);
  if(deferredEvents)source->Callback(rendering::NativeGroundCallback::Deferred,[]{eventHandler.DrawGroundDeferred();});
  source->EndDeferred();
  if(!drawForward||postDeferredEvents)
   source->Callback(rendering::NativeGroundCallback::PostDeferred,[]{eventHandler.DrawGroundPostDeferred();});
  if(combined){DrawCompositePass();if(alwaysDispatchEvents||HaveLuaRenderState())
   source->Callback(rendering::NativeGroundCallback::PostForward,[]{eventHandler.DrawGroundPostForward();});}
  return;
 }
#endif

	// deferred pass must be executed with GLSL shaders
	// if the FFP or ARB state was selected, bail early
	if (!SelectRenderState(DrawPass::TerrainDeferred)->CanDrawDeferred(this)) {
		geomBuffer.Bind();
		geomBuffer.SetDepthRange(1.0f, 0.0f);
		geomBuffer.Clear();
		geomBuffer.SetDepthRange(0.0f, 1.0f);
		geomBuffer.UnBind();
		return;
	}

	// combined shaders also write the forward color, the composite then replaces the forward pass
	const DrawPass::e shaderPass = combined ? DrawPass::TerrainCombined : DrawPass::TerrainDeferred;

	GL::GeometryBuffer::LoadViewport();

	{
		geomBuffer.Bind();
		geomBuffer.SetDepthRange(1.0f, 0.0f);
		geomBuffer.Clear();

		smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, shaderPass);
		smfRenderStates[RENDER_STATE_SEL]->Enable(this, shaderPass);

		if (alphaTest) {
			// combined shaders carry the forward alpha in output 0
			glEnable(GL_ALPHA_TEST);
			glAlphaFunc(GL_GREATER, mapInfo->map.voidAlphaMin);
		}

		if (alwaysDispatchEvents || HaveLuaRenderState())
			eventHandler.DrawGroundPreDeferred();

		meshDrawer->DrawMesh(drawPass);

		if (alphaTest) {
			glDisable(GL_ALPHA_TEST);
		}

		smfRenderStates[RENDER_STATE_SEL]->Disable(this, drawPass);
		smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, DrawPass::Normal);

		if (deferredEvents) {
			// Lua G-buffer geometry (e.g. map edge extensions) does not write the
			// color attachment; keep it clear so the composite skips those pixels
			if (combined)
				glColorMaski(GL::GeometryBuffer::ATTACHMENT_COLORTEX, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

			eventHandler.DrawGroundDeferred();

			if (combined)
				glColorMaski(GL::GeometryBuffer::ATTACHMENT_COLORTEX, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		}

		geomBuffer.SetDepthRange(0.0f, 1.0f);
		geomBuffer.UnBind();
	}

	globalRendering->LoadViewport();

	#if 0
	geomBuffer.DrawDebug(geomBuffer.GetBufferTexture(GL::GeometryBuffer::ATTACHMENT_NORMTEX));
	#endif

	// send event if no forward pass will follow; must be done after the unbind
	if (!drawForward || postDeferredEvents)
		eventHandler.DrawGroundPostDeferred();

	if (!combined)
		return;

	// the composite replaces the forward mesh pass, so DrawGroundPreForward is skipped
	DrawCompositePass();

	if (alwaysDispatchEvents || HaveLuaRenderState())
		eventHandler.DrawGroundPostForward();
}

void CSMFGroundDrawer::DrawForwardPass(const DrawPass::e& drawPass, bool alphaTest)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!SelectRenderState(drawPass)->CanDrawForward(this))
		return;
#ifdef RECOIL_METAL4_LIVE_SCENE
	auto* nativeGround=rendering::CurrentNativeWaterGroundSourceCapture();
	if (nativeGround)nativeGround->EnterForward();
#endif

	smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, drawPass);
	smfRenderStates[RENDER_STATE_SEL]->Enable(this, drawPass);

	glPushAttrib((GL_ENABLE_BIT * alphaTest) | (GL_POLYGON_BIT * wireframe));
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (nativeGround)rendering::ui::GetRecorder().PushAttrib(
		(rendering::ui::Enable * alphaTest) | (rendering::ui::Polygon * wireframe));
#endif

	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (nativeGround && wireframe)rendering::ui::GetRecorder().UpdateWorldState([](rendering::NativeWorldDrawState& state){
		state.frontPolygonMode=state.backPolygonMode=rendering::NativePolygonMode::Line;
	},false);
#endif

	if (alphaTest) {
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, mapInfo->map.voidAlphaMin);
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (nativeGround)rendering::ui::GetRecorder().SetAlphaTest({true,rendering::ScreenAlphaCompare::Greater,mapInfo->map.voidAlphaMin});
#endif
	}

	if (alwaysDispatchEvents || HaveLuaRenderState()) {
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (nativeGround)nativeGround->Callback(rendering::NativeGroundCallback::PreForward,[]{eventHandler.DrawGroundPreForward();});
		else
#endif
			eventHandler.DrawGroundPreForward();
	}

	meshDrawer->DrawMesh(drawPass);

	glPopAttrib();
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (nativeGround)rendering::ui::GetRecorder().PopAttrib();
#endif

	smfRenderStates[RENDER_STATE_SEL]->Disable(this, drawPass);
	smfRenderStates[RENDER_STATE_SEL]->SetCurrentShader(this, DrawPass::Normal);

	if (alwaysDispatchEvents || HaveLuaRenderState()) {
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (nativeGround)nativeGround->Callback(rendering::NativeGroundCallback::PostForward,[]{eventHandler.DrawGroundPostForward();});
		else
#endif
			eventHandler.DrawGroundPostForward();
	}
}

bool CSMFGroundDrawer::CanDrawCombinedPass(const DrawPass::e& drawPass)
{
	RECOIL_DETAILED_TRACY_ZONE;
	// the composite can not replicate wireframe drawing
	if (!combinedAllowed || wireframe || drawPass != DrawPass::Normal)
		return false;
	// no color attachment exists when the buffer is multi-sampled
	if (!geomBuffer.Valid() || !geomBuffer.HasColorTexture() ||
#ifdef RECOIL_METAL4_LIVE_SCENE
		!rendering::RequireNativeTerrainProgramValid(compositeShader))
#else
		!compositeShader->IsValid())
#endif
		return false;

	// one render state must serve both passes (Lua shaders can mix with the default state)
	ISMFRenderState* fwdState = SelectRenderState(drawPass);
	ISMFRenderState* dfrState = SelectRenderState(DrawPass::TerrainDeferred);

	return (fwdState == dfrState && dfrState->CanDrawCombined(this));
}

void CSMFGroundDrawer::DrawCompositePass()
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	if(auto* source=rendering::CurrentNativeWaterGroundSourceCapture()){
		auto& context=rendering::OriginalNativeTextureContext();auto& history=context.History();
		history.ActiveTexture(1);context.Registry().Bind(1,geomBuffer.GetBufferTexture(GL::GeometryBuffer::ATTACHMENT_ZVALTEX),rendering::NativeCUSTextureTarget::Image2D);
		history.ActiveTexture(0);context.Registry().Bind(0,geomBuffer.GetBufferTexture(GL::GeometryBuffer::ATTACHMENT_COLORTEX),rendering::NativeCUSTextureTarget::Image2D);
		compositeShader->Enable();source->SubmitComposite(rendering::CaptureCurrentNativeTerrainProgram());compositeShader->Disable();
		history.ActiveTexture(1);history.BindDefault(1,rendering::NativeCUSTextureTarget::Image2D);
		history.ActiveTexture(0);history.BindDefault(0,rendering::NativeCUSTextureTarget::Image2D);return;
	}
#endif
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, geomBuffer.GetBufferTexture(GL::GeometryBuffer::ATTACHMENT_ZVALTEX));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, geomBuffer.GetBufferTexture(GL::GeometryBuffer::ATTACHMENT_COLORTEX));

	compositeShader->Enable();

	glBegin(GL_QUADS);
	glVertex2f(-1.0f, -1.0f);
	glVertex2f( 1.0f, -1.0f);
	glVertex2f( 1.0f,  1.0f);
	glVertex2f(-1.0f,  1.0f);
	glEnd();

	compositeShader->Disable();

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void CSMFGroundDrawer::Draw(const DrawPass::e& drawPass)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	auto* nativeGround=rendering::CurrentNativeWaterGroundSourceCapture();
	if (nativeGround)nativeGround->BeginDraw();
#endif
	// must be here because water renderers also call us
	if (!globalRendering->drawGround)
		return;
	// if entire map is under voidwater, no need to draw *ground*
	if (readMap->HasOnlyVoidWater())
		return;
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (nativeGround) {
		nativeGround->EnterDraw();
		auto& recorder=rendering::ui::GetRecorder();
		recorder.UpdateBlend([](rendering::ScreenBlendState& blend){blend.enabled=false;});
		recorder.UpdateWorldState([](rendering::NativeWorldDrawState& state){state.cullEnabled=true;state.cull=rendering::NativeWorldCull::Back;},false);
	}
#endif

	glDisable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	const bool alphaTest = mapRendering->voidGround || (mapRendering->voidWater && drawPass != DrawPass::WaterReflection);
	// rasterize only once if the deferred pass can also produce the forward color
	const bool combinedPass = drawDeferred && drawForward && CanDrawCombinedPass(drawPass);

	if (drawDeferred) {
		// do the deferred pass first, will allow us to re-use
		// its output at some future point and eventually draw
		// the entire map deferred
		DrawDeferredPass(drawPass, alphaTest, combinedPass);
	}

	if (drawForward && !combinedPass) {
		DrawForwardPass(drawPass, alphaTest);
	}

	glDisable(GL_CULL_FACE);
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (nativeGround)rendering::ui::GetRecorder().UpdateWorldState([](rendering::NativeWorldDrawState& state){state.cullEnabled=false;},false);
#endif

	if (drawPass == DrawPass::Normal && drawMapEdges) {
		DrawBorder(drawPass);
	}
}


void CSMFGroundDrawer::DrawBorder(const DrawPass::e drawPass)
{
	RECOIL_DETAILED_TRACY_ZONE;
	ISMFRenderState* prvState = smfRenderStates[RENDER_STATE_SEL];
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (auto* source=rendering::CurrentNativeWaterGroundSourceCapture()) {
		source->EnterBorder();
		smfRenderStates[RENDER_STATE_SEL]=smfRenderStates[RENDER_STATE_NOP];
		auto& recorder=rendering::ui::GetRecorder();auto& textures=rendering::OriginalNativeTextureContext();
		recorder.UpdateBlend([](auto& blend){blend.enabled=true;});
		recorder.UpdateWorldState([&](auto& state){
			state.cullEnabled=true;state.cull=rendering::NativeWorldCull::Back;
			state.frontPolygonMode=state.backPolygonMode=wireframe?rendering::NativePolygonMode::Line:rendering::NativePolygonMode::Fill;
		},false);
		textures.History().ActiveTexture(2);smfMap->BindNativeTerrainTexture(rendering::NativeTerrainImageRole::Detail);
		textures.History().ActiveTexture(1);smfMap->BindNativeTerrainTexture(rendering::NativeTerrainImageRole::Height);
		textures.History().ActiveTexture(0);
		borderShader->Enable();
		borderShader->SetUniform("borderMinHeight",std::min(readMap->GetInitMinHeight(),-500.0f));
		meshDrawer->DrawBorderMesh(drawPass);
		borderShader->Disable();
		if(wireframe)recorder.UpdateWorldState([](auto& state){state.frontPolygonMode=state.backPolygonMode=rendering::NativePolygonMode::Fill;},false);
		for(unsigned unit:{2u,1u,0u}) {
			textures.History().ActiveTexture(unit);textures.Registry().Bind(unit,0,rendering::NativeCUSTextureTarget::Image2D);
			recorder.SyncObservedTextureBinding(unit);
		}
		recorder.UpdateWorldState([](auto& state){state.cullEnabled=false;},false);
		recorder.UpdateBlend([](auto& blend){blend.enabled=false;});
		smfRenderStates[RENDER_STATE_SEL]=prvState;
		return;
	}
#endif

	// no need to enable, does nothing
	smfRenderStates[RENDER_STATE_SEL] = smfRenderStates[RENDER_STATE_NOP];

	glEnable(GL_BLEND);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	glActiveTexture(GL_TEXTURE2); glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, smfMap->GetDetailTexture());

	glActiveTexture(GL_TEXTURE1); glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, smfMap->GetHeightMapTexture());

	//for CSMFGroundTextures::BindSquareTexture()
	glActiveTexture(GL_TEXTURE0); glEnable(GL_TEXTURE_2D);

	glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

	borderShader->Enable();
	borderShader->SetUniform("borderMinHeight", std::min(readMap->GetInitMinHeight(), -500.0f));
	meshDrawer->DrawBorderMesh(drawPass);
	borderShader->Disable();

	if (wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);

	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);

	smfRenderStates[RENDER_STATE_SEL] = prvState;
}


void CSMFGroundDrawer::DrawShadowPass()
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifdef RECOIL_METAL4_LIVE_SCENE
	if (auto* source=rendering::CurrentNativeTerrainShadowCapture())source->BeginTerrainDraw();
#endif
	if (!globalRendering->drawGround)
		return;
	if (readMap->HasOnlyVoidWater())
		return;

#ifdef RECOIL_METAL4_LIVE_SCENE
	if (auto* source=rendering::CurrentNativeTerrainShadowCapture()) {
		auto& recorder=rendering::ui::GetRecorder();auto& textures=rendering::OriginalNativeTextureContext();
		recorder.UpdateWorldState([&](auto& state){state.polygonOffset=true;state.offsetFactor=spPolygonOffsetScale;state.offsetUnits=spPolygonOffsetUnits;},false);
		textures.History().ActiveTexture(1);smfMap->BindNativeTerrainTexture(rendering::NativeTerrainImageRole::Height);
		// Same immutable map-size uniforms as ShadowHandler's original MAP link.
		source->EnableTerrainProgram(std::min(readMap->GetInitMinHeight(),-500.0f),
			{float(mapDims.mapx*SQUARE_SIZE),float(mapDims.mapy*SQUARE_SIZE),1.0f/(mapDims.mapx*SQUARE_SIZE),1.0f/(mapDims.mapy*SQUARE_SIZE)});
		meshDrawer->DrawMesh(DrawPass::Shadow);
		meshDrawer->DrawBorderMesh(DrawPass::Shadow);
		source->DisableTerrainProgram();
		textures.Registry().Bind(1,0,rendering::NativeCUSTextureTarget::Image2D);recorder.SyncObservedTextureBinding(1);
		textures.History().ActiveTexture(0);
		recorder.UpdateWorldState([](auto& state){state.polygonOffset=false;},false);
		return;
	}
#endif
	shadowShader = shadowHandler.GetShadowGenProg(CShadowHandler::SHADOWGEN_PROGRAM_MAP);
	assert(shadowShader);
	glEnable(GL_POLYGON_OFFSET_FILL);

	//#pragma message "REMOVE ME, WHEN NOT NEEDED"
	//glDisable(GL_CULL_FACE);

	glPolygonOffset(spPolygonOffsetScale, spPolygonOffsetUnits); // dz*s + r*u

	glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, smfMap->GetHeightMapTexture());
	shadowShader->Enable();
	shadowShader->SetUniform("borderMinHeight", std::min(readMap->GetInitMinHeight(), -500.0f));
		meshDrawer->DrawMesh(DrawPass::Shadow);
		// also render the border geometry to prevent light-visible backfaces
		meshDrawer->DrawBorderMesh(DrawPass::Shadow);
	shadowShader->Disable();
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	glDisable(GL_POLYGON_OFFSET_FILL);
	//glEnable(GL_CULL_FACE);
}



void CSMFGroundDrawer::SetLuaShader(const LuaMapShaderData* luaMapShaderData)
{
	RECOIL_DETAILED_TRACY_ZONE;
	smfRenderStates[RENDER_STATE_LUA]->Update(this, luaMapShaderData);
}

void CSMFGroundDrawer::SetupBigSquare(const DrawPass::e& drawPass, const int bigSquareX, const int bigSquareY)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (drawPass != DrawPass::Shadow) {
		groundTextures->BindSquareTexture(bigSquareX, bigSquareY);
		smfRenderStates[RENDER_STATE_SEL]->SetSquareTexGen(bigSquareX, bigSquareY);
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (auto* source=rendering::CurrentNativeWaterGroundSourceCapture())
			source->SelectSquare(bigSquareX,bigSquareY);
#endif

		if (borderShader && borderShader->IsBound()) {
			borderShader->SetUniform("texSquare", bigSquareX, bigSquareY);
		}
	}
	else {
#ifdef RECOIL_METAL4_LIVE_SCENE
		if (auto* source=rendering::CurrentNativeTerrainShadowCapture()) {
			source->SelectTerrainSquare(bigSquareX,bigSquareY);
			return;
		}
#endif
		if (shadowShader && shadowShader->IsBound()) {
			shadowShader->SetUniform("texSquare", bigSquareX, bigSquareY);
		}
	}
}



void CSMFGroundDrawer::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (readMap->HasOnlyVoidWater())
		return;

	groundTextures->DrawUpdate();
	// done by DrawMesh; needs to know the actual draw-pass
	// meshDrawer->Update();

	if (drawDeferred) {
		drawDeferred &= UpdateGeometryBuffer(false);
	}
}

void CSMFGroundDrawer::UpdateRenderState()
{
	RECOIL_DETAILED_TRACY_ZONE;
	smfRenderStates[RENDER_STATE_SSP]->Update(this, nullptr);
}

void CSMFGroundDrawer::SunChanged() {
	RECOIL_DETAILED_TRACY_ZONE;
	// Lua has gl.GetSun
	if (HaveLuaRenderState())
		return;

	smfRenderStates[RENDER_STATE_SEL]->UpdateShaderSkyUniforms();
}


bool CSMFGroundDrawer::UpdateGeometryBuffer(bool init)
{
	RECOIL_DETAILED_TRACY_ZONE;
	static const bool drawDeferredAllowed = configHandler->GetBool("AllowDeferredMapRendering");

	if (!drawDeferredAllowed)
		return false;

	return (geomBuffer.Update(init));
}



void CSMFGroundDrawer::SetDetail(int newGroundDetail)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const int minGroundDetail = MIN_GROUND_DETAIL[drawerMode == SMF_MESHDRAWER_ROAM];
	const int maxGroundDetail = MAX_GROUND_DETAIL[drawerMode == SMF_MESHDRAWER_ROAM];

	configHandler->Set("GroundDetail", groundDetail = std::clamp(newGroundDetail, minGroundDetail, maxGroundDetail));
	LOG("GroundDetail%s set to %i", ((drawerMode != SMF_MESHDRAWER_ROAM)? "[Bias]": ""), groundDetail);
}



int CSMFGroundDrawer::GetGroundDetail(const DrawPass::e& drawPass) const
{
	RECOIL_DETAILED_TRACY_ZONE;
	int detail = groundDetail;

	switch (drawPass) {
		case DrawPass::TerrainReflection:
			detail *= LODScaleTerrainReflection;
			break;
		case DrawPass::WaterReflection:
			detail *= LODScaleReflection;
			break;
		case DrawPass::WaterRefraction:
			detail *= LODScaleRefraction;
			break;
		case DrawPass::Shadow:
			// TODO:
			//   render a contour mesh for the SP? z-fighting / p-panning occur
			//   when the regular and shadow-mesh tessellations differ too much,
			//   more visible on larger or hillier maps
			//   detail *= LODScaleShadow;
			break;
		default:
			break;
	}

	return detail;
}
