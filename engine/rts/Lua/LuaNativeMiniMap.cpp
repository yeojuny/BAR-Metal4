/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "LuaNativeUI.h"
#include "Game/UI/MiniMap.h"
#include "Game/UI/CursorIcons.h"
#include "Game/UI/CommandColors.h"
#include "Game/UI/GuiHandler.h"
#include "Game/SelectedUnitsHandler.h"
#include "Map/ReadMap.h"
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeMiniMapCapture.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/CommandDrawer.h"
#include "Rendering/LineDrawer.h"
#include "System/EventHandler.h"
#include "System/Log/ILog.h"
#include "System/SpringMath.h"
#include <stdexcept>
#include <utility>

namespace LuaNativeUI {
namespace {
using namespace rendering;
using namespace rendering::ui;

struct Cache {
	NativeMiniMapCapture capture;
	std::shared_ptr<const ScreenTexture> image;
	std::uint64_t updateSerial=0;
	std::string failure;
};
Cache cache;

void UnitProjection(NativeUIRecorder& recorder)
{
	recorder.MatrixMode(NativeUIMatrixMode::Projection);
	recorder.LoadMatrix(OrthoMatrix(0,1,0,1,-1,1));
	recorder.MatrixMode(NativeUIMatrixMode::Texture);
	recorder.LoadMatrix(IdentityMatrix());
	recorder.MatrixMode(NativeUIMatrixMode::ModelView);
	recorder.LoadMatrix(IdentityMatrix());
}

using MiniMapPoint = std::array<float,2>;

void Quad(NativeUIRecorder& recorder,const std::array<MiniMapPoint,4>& positions,const std::array<MiniMapPoint,4>& uv)
{
	recorder.Begin(ScreenPrimitive::Quads,"original minimap quad");
	for (unsigned n=0;n<4;++n) {
		recorder.TexCoord(uv[n][0],uv[n][1]);
		recorder.Vertex(positions[n][0],positions[n][1]);
	}
	recorder.End();
}

constexpr std::array<MiniMapPoint,4> unitQuad{{{0,0},{1,0},{1,1},{0,1}}};

MiniMapPoint MapPosition(const NativeMiniMapState& state,const std::array<float,2>& p)
{
	const float x=p[0]/float(mapDims.mapx*SQUARE_SIZE),z=p[1]/float(mapDims.mapy*SQUARE_SIZE);
	switch(state.rotation) {
		case 0:return {x,1-z};
		case 1:return {z,x};
		case 2:return {1-x,z};
		case 3:return {1-z,1-x};
		default:throw std::runtime_error("Invalid original minimap quarter turn");
	}
}

void Background(NativeUIRecorder& recorder,const NativeMiniMapState& state,std::shared_ptr<const NativeMiniMapBackground> background)
{
	// Same four source UVs and quarter-turn permutations as DrawBackground.
	constexpr std::array<MiniMapPoint,4> sourceUV{{{0,1},{1,1},{1,0},{0,0}}};
	std::array<MiniMapPoint,4> positions;
	for (unsigned n=0;n<4;++n) positions[n]=unitQuad[(n+state.rotation)%4];
	// The native cache replaces DrawBackground at this actual refresh event.
	// Its original raw binds must survive this adapter's local Texture scope.
	readMap->BindMiniMapTextures();
	recorder.PushAttrib(Current|rendering::ui::Blend|Alpha|Texture);
	recorder.Color({1,1,1,1});
	ScreenBlendState blend;blend.enabled=false;recorder.SetBlend(blend);
	recorder.SetAlphaTest({});
	recorder.SetMiniMapBackground(std::move(background));
	Quad(recorder,positions,sourceUV);
	recorder.PopAttrib();
}

void Icons(NativeUIRecorder& recorder)
{
	// The original UnitDrawer supplies the real filtered/sorted atlas quads,
	// including rotation, LOS/radar error positions, selection and team colors.
	const auto icons=minimap->GetNativeIcons();
	recorder.PushAttrib(Current|Texture|Alpha);
	recorder.SetAlphaTest({true,ScreenAlphaCompare::Greater,0});
	ScreenSampler sampler{ScreenFilter::Linear,ScreenWrap::ClampEdge,ScreenWrap::ClampEdge,ScreenMipFilter::Linear};
	for (const auto& icon: icons) {
		recorder.SetTexture(icon.atlas,sampler);
		recorder.Color({icon.color[0]/255.0f,icon.color[1]/255.0f,icon.color[2]/255.0f,icon.color[3]/255.0f});
		std::array<MiniMapPoint,4> positions;
		for (unsigned n=0;n<4;++n) positions[n]={icon.positions[n][0]/float(mapDims.mapx*SQUARE_SIZE),1-icon.positions[n][1]/float(mapDims.mapy*SQUARE_SIZE)};
		Quad(recorder,positions,icon.uv);
	}
	recorder.PopAttrib();
	recorder.SetTextureEnabled(false);
}

void Lines(NativeUIRecorder& recorder,const NativeMiniMapState& state,const std::vector<NativeMiniMapLine>& source)
{
	std::vector<ScreenLine> lines;lines.reserve(source.size());
	for (const auto& line:source) {
		const auto a=MapPosition(state,line.start),b=MapPosition(state,line.end);
		lines.push_back({{a[0],a[1],0,line.color},{b[0],b[1],0,line.endColor.value_or(line.color)},line.width});
	}
	recorder.DrawLines(std::move(lines));
}

void Projectiles(NativeUIRecorder& recorder,const NativeMiniMapState& state)
{
	if (!state.drawProjectiles) return;
	std::vector<NativeMiniMapLine> lines;std::vector<NativeMiniMapPoint> points;
	minimap->GetNativeProjectiles(lines,points);
	Lines(recorder,state,lines);
	std::vector<ScreenVertex> vertices;vertices.reserve(points.size());
	for (const auto& point:points) {
		if (point.sizePixels!=1) throw std::runtime_error("Native minimap projectile point size is not implemented");
		const auto p=MapPosition(state,point.position);
		vertices.push_back({p[0],p[1],0,point.color});
	}
	recorder.DrawPixelPoints(std::move(vertices));
}

ScreenBlendFactor BlendFactor(int factor)
{
	switch(factor) {
		case GL_ZERO:return ScreenBlendFactor::Zero;
		case GL_ONE:return ScreenBlendFactor::One;
		case GL_SRC_ALPHA:return ScreenBlendFactor::SourceAlpha;
		case GL_ONE_MINUS_SRC_ALPHA:return ScreenBlendFactor::OneMinusSourceAlpha;
		case GL_DST_ALPHA:return ScreenBlendFactor::DestinationAlpha;
		case GL_ONE_MINUS_DST_ALPHA:return ScreenBlendFactor::OneMinusDestinationAlpha;
		case GL_SRC_COLOR:return ScreenBlendFactor::SourceColor;
		case GL_ONE_MINUS_SRC_COLOR:return ScreenBlendFactor::OneMinusSourceColor;
		case GL_DST_COLOR:return ScreenBlendFactor::DestinationColor;
		case GL_ONE_MINUS_DST_COLOR:return ScreenBlendFactor::OneMinusDestinationColor;
		case GL_SRC_ALPHA_SATURATE:return ScreenBlendFactor::SourceAlphaSaturate;
		default:throw std::runtime_error("Original minimap command blend is not implemented");
	}
}

void Commands(NativeUIRecorder& recorder,const NativeMiniMapState& state)
{
	commandDrawer->DrawLuaQueuedUnitSetCommands();
	if (state.drawCommands>0 && guihandler->GetQueueKeystate()) selectedUnitsHandler.DrawCommands();
	const auto paths=lineDrawer.ConsumeNativePaths();
	if (paths.empty()) return;
	ScreenBlendState blend;
	blend.sourceColor=blend.sourceAlpha=BlendFactor(cmdColors.QueuedBlendSrc());
	blend.destinationColor=blend.destinationAlpha=BlendFactor(cmdColors.QueuedBlendDst());
	recorder.SetBlend(blend);
	for (const auto& path:paths) {
		std::vector<ScreenLine> lines;
		for (std::size_t n=1;n<path.positions.size();n+=path.lineStrip?1:2) {
			const auto& previous=path.positions[n-1];const auto& next=path.positions[n];
			const auto a=MapPosition(state,{previous[0],previous[2]}),b=MapPosition(state,{next[0],next[2]});
			ScreenLine line{{a[0],a[1],0,path.colors[n-1]},{b[0],b[1],0,path.colors[n]},2.5f};
			line.stippled=path.stippled;line.pattern=path.pattern;line.factor=path.factor;
			line.resetStipple=!path.lineStrip || n==1;lines.push_back(line);
		}
		recorder.DrawLines(std::move(lines));
	}
}

void AreaCommands(NativeUIRecorder& recorder,const NativeMiniMapState& state)
{
	if (state.drawCommands<=0) return;
	const auto preview=guihandler->GetNativeAreaCommandPreview();
	if (!preview) return;
	// Original DrawMapStuff(true) follows queued lines in the cached pass.
	// The input owner supplies the original selection and clamped world radius.
	recorder.PushAttrib(Current|Texture|Scissor);
	try {
		recorder.SetScissor(ScreenScissor{0,0,state.size[0],state.size[1]});
		recorder.SetTexture({});
		recorder.SetTexture({}, {}, 1);
		recorder.Color({preview->color[0],preview->color[1],preview->color[2],0.5f});
		recorder.BeginLegacyPrimitive(NativeDrawPrimitive::TriangleFan,"original minimap area command");
		constexpr int divs=256;
		for (int i=0;i<=divs;++i) {
			const float radians=math::TWOPI*(float)i/(float)divs;
			const auto p=MapPosition(state,{preview->center.x+fastmath::sin(radians)*preview->radius,
				preview->center.z+fastmath::cos(radians)*preview->radius});
			recorder.Vertex(p[0],p[1]);
		}
		recorder.EndLegacyPrimitive();
	} catch (...) {
		recorder.AbortPrimitive();recorder.PopAttrib();throw;
	}
	recorder.PopAttrib();
}
}

void ResetMiniMap() {cache={};}

void UpdateMiniMap()
{
	if (!minimap) return;
	const auto state=minimap->GetNativeState();
	if (state.minimized || !state.size[0] || !state.size[1] || !state.updateSerial) return;
	if (cache.image && cache.updateSerial==state.updateSerial) return;
	auto& recorder=GetRecorder();
	bool targetOpen=false,cacheDraw=false;
	recorder.BeginResourceStage();
	try {
		auto background=cache.capture.Capture(state);
		auto next=(cache.image && cache.image->width==state.size[0] && cache.image->height==state.size[1])?
			std::make_shared<ScreenTexture>(*cache.image):MakeRenderTargetTexture(state.size[0],state.size[1],"original minimap cache");
		++next->generation;
		recorder.BeginTarget(next);targetOpen=true;
		recorder.SetScissor({});
		recorder.ClearTarget(ScreenColor{0,0,0,1});
		UnitProjection(recorder);
		recorder.SetBlend({});recorder.SetTexture({});
		minimap->BeginNativeCacheDraw();cacheDraw=true;
		cursorIcons.Enable(false);
		Background(recorder,state,std::move(background));
		eventHandler.DrawInMiniMapBackground();
		recorder.SetScissor(ScreenScissor{0,0,state.size[0],state.size[1]});
		Icons(recorder);
		recorder.SetScissor({});
		Projectiles(recorder,state);
		Commands(recorder,state);
		AreaCommands(recorder,state);
		Lines(recorder,state,minimap->GetNativeRangeLines());
		eventHandler.DrawInMiniMap();
		cursorIcons.Enable(true);
		minimap->EndNativeCacheDraw();cacheDraw=false;
		recorder.EndTarget();targetOpen=false;
		cache.image=std::move(next);cache.updateSerial=state.updateSerial;cache.failure.clear();
	} catch (const std::exception& error) {
		cursorIcons.Enable(true);
		if (cacheDraw) minimap->EndNativeCacheDraw();
		if (targetOpen) recorder.AbortTarget();
		if (cache.failure!=error.what()) LOG_L(L_ERROR,"[NativeMiniMap] Original cache update failed: %s",error.what());
		cache.failure=error.what();
	}
	recorder.EndResourceStage();
}

void DrawMiniMap(bool defaultTransform)
{
	if (!minimap || minimap->GetMinimized()) return;
	if (!cache.failure.empty()) throw std::runtime_error(cache.failure);
	// Update precedes screen draw; an initial refresh can legitimately be due
	// only on the next frame, just as with the original minimap cache.
	if (!cache.image) return;
	const auto state=minimap->GetNativeState();
	const bool dualViewport=state.dualScreen && defaultTransform;
	if (dualViewport && (state.mainViewport[2]<=0 || state.mainViewport[3]<=0 ||
		state.dualViewport[2]<=0 || state.dualViewport[3]<=0 || !state.size[0] || !state.size[1]))
		throw std::runtime_error("Native dual minimap has no actual window geometry");
	auto& recorder=GetRecorder();
	recorder.MatrixMode(NativeUIMatrixMode::ModelView);recorder.PushMatrix();
	if (defaultTransform) {
		if (dualViewport) {
			// Original LuaOpenGL::DrawMiniMap first scales by the MAIN view
			// dimensions. LoadDualViewport changes raster placement, not that
			// caller transform or its projection. Native C++ also uses pixels.
			recorder.MultMatrix(ScaleMatrix(float(state.mainViewport[2]),float(state.mainViewport[3]),1));
		} else {
			recorder.MultMatrix(TranslationMatrix(float(state.position[0]),float(state.position[1]),0));
			recorder.MultMatrix(ScaleMatrix(float(state.size[0]),float(state.size[1]),1));
		}
	}
	recorder.MatrixMode(NativeUIMatrixMode::Texture);recorder.PushMatrix();recorder.LoadMatrix(IdentityMatrix());
	recorder.MatrixMode(NativeUIMatrixMode::ModelView);
	recorder.PushAttrib(Current|rendering::ui::Blend|Alpha);
	if (dualViewport) recorder.PushAttrib(Viewport|Scissor);
	try {
		if (dualViewport) {
			// Original LoadDualViewport clears the FULL second view, including
			// aspect bars, independently of the incoming viewport/depth/blend.
			const auto& v=state.dualViewport;
			recorder.SetScissor(ScreenScissor{v[0],v[1],std::uint32_t(v[2]),std::uint32_t(v[3]),true});
			recorder.ClearAttachments(ScreenAttachmentColor,ScreenColor{0,0,0,1});
			recorder.SetScissor({});
			recorder.SetViewport({state.position[0],state.position[1],state.size[0],state.size[1]});
		}
		recorder.Color({1,1,1,1});
		ScreenBlendState blend;blend.enabled=false;recorder.SetBlend(blend);
		recorder.SetTexture(cache.image,{ScreenFilter::Nearest,ScreenWrap::TransparentBorder,ScreenWrap::TransparentBorder});
		Quad(recorder,unitQuad,unitQuad);
		recorder.SetBlend({});
		recorder.SetTextureEnabled(false);
		recorder.SetScissor(ScreenScissor{state.position[0],state.position[1],state.size[0],state.size[1],state.dualScreen});
		Lines(recorder,state,minimap->GetNativeDynamicLines());
		recorder.SetScissor({});
	} catch (...) {
		recorder.AbortPrimitive();
		if (dualViewport) recorder.PopAttrib();
		recorder.PopAttrib();
		recorder.MatrixMode(NativeUIMatrixMode::Texture);recorder.PopMatrix();
		recorder.MatrixMode(NativeUIMatrixMode::ModelView);recorder.PopMatrix();
		throw;
	}
	if (dualViewport) {
		recorder.PopAttrib();
		recorder.SetScissor({}); // Original successful return leaves scissor disabled.
		// Original RenderCachedTexture calls LoadViewport, restoring MAIN,
		// even if its caller supplied a different viewport on entry.
		const auto& v=state.mainViewport;
		recorder.SetViewport({v[0],v[1],std::uint32_t(v[2]),std::uint32_t(v[3])});
	}
	recorder.PopAttrib();
	recorder.MatrixMode(NativeUIMatrixMode::Texture);recorder.PopMatrix();
	recorder.MatrixMode(NativeUIMatrixMode::ModelView);recorder.PopMatrix();
}
}
