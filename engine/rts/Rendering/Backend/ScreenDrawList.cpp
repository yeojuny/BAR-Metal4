/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeStartBoxData.h"
#include "NativeWeaponRangeData.h"
#include "NativeBuildSquareData.h"
#include "NativeSensorPreviewData.h"
#include "NativeSensorLOSData.h"
#include "NativeUnitPrimitiveData.h"
#include "NativeHealthBarsData.h"
#include "NativePanelShaderData.h"
#include "NativeUITextureHistory.h"
#include "ScreenDrawList.h"
#include "NativeDeferredVisualizerProgram.h"
#include "NativeDecalCaptureProgram.h"
#include "NativeSplatPainterProgram.h"
#include "NativeDiffusePainterProgram.h"
#include "NativeTerraformCaptureProgram.h"
#include "NativePiPData.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <functional>

namespace rendering {
namespace {
void ValidState(const ScreenDrawState& state, bool shaderOwnsImages = false)
{
	const auto& ds=state.depthStencil;
	if (ds.depthCompare>ScreenAlphaCompare::Always) throw std::invalid_argument("Invalid native screen depth comparison");
	for (const auto* face:{&ds.frontStencil,&ds.backStencil})
		if (face->compare>ScreenAlphaCompare::Always || face->fail>NativeWorldStencilOp::DecrementWrap ||
			face->depthFail>NativeWorldStencilOp::DecrementWrap || face->pass>NativeWorldStencilOp::DecrementWrap)
			throw std::invalid_argument("Invalid native screen stencil state");
	if (!shaderOwnsImages) ValidateScreenSampler(state.sampler);
	if (state.blend.colorEquation>ScreenBlendEquation::Maximum || state.blend.alphaEquation>ScreenBlendEquation::Maximum)
		throw std::invalid_argument("ScreenDrawList unsupported original blend equation");
	if (!shaderOwnsImages) ValidateScreenSampler(state.secondary.sampler);
	if (state.secondary.uvMode > ScreenUVMode::Texel) throw std::invalid_argument("ScreenDrawList unsupported secondary UV mode");
	const auto factor = [](ScreenBlendFactor value) { return value <= ScreenBlendFactor::SourceAlphaSaturate; };
	if (!factor(state.blend.sourceColor) || !factor(state.blend.destinationColor) || !factor(state.blend.sourceAlpha) || !factor(state.blend.destinationAlpha))
		throw std::invalid_argument("ScreenDrawList unsupported blend factor");
	if (state.material > ScreenMaterial::TerraformCapture || state.uvMode > ScreenUVMode::Texel ||
		state.alphaTest.compare > ScreenAlphaCompare::Always || !std::isfinite(state.alphaTest.reference) || state.alphaTest.reference < 0 || state.alphaTest.reference > 1)
		throw std::invalid_argument("ScreenDrawList invalid typed screen state");
	if((state.material==ScreenMaterial::TargetPriorityStroke||state.material==ScreenMaterial::FrameGrapher)&&(!std::isfinite(state.targetPriorityViewGeometry[0])||!std::isfinite(state.targetPriorityViewGeometry[1])||state.targetPriorityViewGeometry[0]<=0||state.targetPriorityViewGeometry[1]<=0))
		throw std::invalid_argument("TargetPriorityLines actual view geometry is invalid");
 if(state.material==ScreenMaterial::TerraformCapture){if(!state.terraformCapture)throw std::invalid_argument("Terraform Capture input missing");ValidateNativeTerraformCapture(*state.terraformCapture);}
 if(state.material==ScreenMaterial::DiffusePainter){if(!state.diffusePainter)throw std::invalid_argument("Diffuse Painter input missing");ValidateNativeDiffusePainter(*state.diffusePainter);}
 if(state.material==ScreenMaterial::SplatPainter){if(!state.splatPainter)throw std::invalid_argument("Splat Painter input missing");ValidateNativeSplatPainter(*state.splatPainter);}
 if(state.material==ScreenMaterial::DecalCapture){if(!state.decalCapture)throw std::invalid_argument("Decal Capture input missing");ValidateNativeDecalCapture(*state.decalCapture);}
 if(state.material==ScreenMaterial::DeferredVisualizer){if(!state.deferredVisualizer)throw std::invalid_argument("Deferred visualizer input missing");ValidateNativeDeferredVisualizer(*state.deferredVisualizer);}
	if (!shaderOwnsImages && state.material == ScreenMaterial::MiniMapBackground && !state.minimapBackground)
		throw std::invalid_argument("ScreenDrawList minimap background requires an owned source snapshot");
	if (!shaderOwnsImages && state.material != ScreenMaterial::Solid && state.material != ScreenMaterial::TargetPriorityStroke && state.material != ScreenMaterial::FrameGrapher && state.material != ScreenMaterial::DeferredVisualizer && state.material != ScreenMaterial::SpectatorHUD && state.material != ScreenMaterial::MiniMapBackground && !state.texture)
		throw std::invalid_argument("ScreenDrawList textured material requires an owned texture snapshot");
}
void FiniteVertex(const ScreenVertex& value)
{
	if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
		!std::all_of(value.color.begin(), value.color.end(), [](float v) { return std::isfinite(v); }))
		throw std::invalid_argument("ScreenDrawList vertex must be finite");
}
void FiniteUV(const ScreenTexCoord& value)
{
	if (!std::all_of(value.begin(),value.end(),[](float v){return std::isfinite(v);})) throw std::invalid_argument("ScreenDrawList texture coordinates must be finite");
}
} // namespace

void ValidateScreenTextureMipGeneration(const ScreenTexture& texture)
{
	if (!texture.generateMipmaps) return;
	if (texture.incomplete || texture.renderTarget || texture.postprocessVersion || texture.sourceTexture)
		throw std::invalid_argument("Native generated image mips require an original image base, not incomplete/RTT/shared-copy storage");
	if (texture.mips.size()>1)
		throw std::invalid_argument("Original embedded image mip levels must not be overwritten");
	// The native uploader validates the original base layout and decodes BC
	// into legal RGBA8 storage before the shared GPU mip producer runs.
}

std::shared_ptr<const ScreenRenderTargetIdentity> ScreenTextureTargetIdentity(const ScreenTexture& texture)
{
	if(texture.renderTarget)return texture.renderTarget;
	return texture.postprocessVersion && texture.postprocessVersion->image?
		texture.postprocessVersion->image->originalScreenTarget:nullptr;
}

void ValidateScreenSampler(const ScreenSampler& value)
{
	if (value.filter>ScreenFilter::Linear || value.mipFilter>ScreenMipFilter::Linear ||
		value.wrapS>ScreenWrap::LegacyClamp || value.wrapT>ScreenWrap::LegacyClamp ||
		value.magnificationFilter.value_or(value.filter)>ScreenFilter::Linear || value.anisotropy<1 || value.anisotropy>16)
		throw std::invalid_argument("ScreenDrawList unsupported sampler state");
	if ((value.wrapS==ScreenWrap::LegacyClamp || value.wrapT==ScreenWrap::LegacyClamp) &&
		(value.mipFilter!=ScreenMipFilter::None || value.magnificationFilter.value_or(value.filter)!=value.filter))
		throw std::invalid_argument("Original atlas GL_CLAMP currently requires equal min/mag filters and no mip filtering");
}

void ScreenDrawList::Color(ScreenColor value)
{
	if (!std::all_of(value.begin(), value.end(), [](float v) { return std::isfinite(v); }))
		throw std::invalid_argument("ScreenDrawList color must be finite");
	currentColor = value;
}
void ScreenDrawList::TexCoord(float u, float v) { MultiTexCoord(0,u,v); }
void ScreenDrawList::MultiTexCoord(unsigned unit, float u, float v)
{
	if (unit>1) throw std::invalid_argument("ScreenDrawList texture unit outside implemented 0/1 contract");
	FiniteUV({u,v}); currentTexCoord[unit*2]=u;currentTexCoord[unit*2+1]=v;
}
void ScreenDrawList::SetBlend(ScreenBlendState value)
{
	if (recording) throw std::logic_error("ScreenDrawList blend change inside Begin");
	auto state = currentState; state.blend = value; ValidState(state); currentState = std::move(state);
}
void ScreenDrawList::SetScissor(std::optional<ScreenScissor> value)
{
	if (recording) throw std::logic_error("ScreenDrawList scissor change inside Begin");
	currentState.scissor = value;
}
void ScreenDrawList::SetTexture(std::shared_ptr<const ScreenTexture> texture, ScreenMaterial material, ScreenUVMode uvMode, ScreenSampler sampler)
{
	if (recording) throw std::logic_error("ScreenDrawList texture change inside Begin");
	auto state = currentState; state.texture = std::move(texture); state.material = material; state.uvMode = uvMode; state.sampler = sampler;
	state.minimapBackground.reset();
	ValidState(state); currentState = std::move(state);
}
void ScreenDrawList::SetSecondaryTexture(ScreenSecondaryTexture value)
{
	if (recording) throw std::logic_error("ScreenDrawList texture change inside Begin");
	auto state=currentState;state.secondary=std::move(value);ValidState(state);currentState=std::move(state);
}
void ScreenDrawList::SetMiniMapBackground(std::shared_ptr<const NativeMiniMapBackground> background)
{
	if (recording) throw std::logic_error("ScreenDrawList minimap background change inside Begin");
	auto state = currentState;
	state.texture.reset(); state.minimapBackground = std::move(background);
	state.material = ScreenMaterial::MiniMapBackground; state.uvMode = ScreenUVMode::Normalized;
	ValidState(state); currentState = std::move(state);
}
void ScreenDrawList::SetAlphaTest(ScreenAlphaTest value)
{
	if (recording) throw std::logic_error("ScreenDrawList alpha-test change inside Begin");
	if (!std::isfinite(value.reference)) throw std::invalid_argument("ScreenDrawList alpha reference must be finite");
	value.reference = std::clamp(value.reference, 0.0f, 1.0f);
	auto state = currentState; state.alphaTest = value; ValidState(state); currentState = std::move(state);
}
void ScreenDrawList::SetSampler(ScreenSampler value)
{
	if (recording) throw std::logic_error("ScreenDrawList sampler change inside Begin");
	auto state = currentState; state.sampler = value; ValidState(state); currentState = std::move(state);
}
void ScreenDrawList::SetUVMode(ScreenUVMode value)
{
	if (recording) throw std::logic_error("ScreenDrawList UV mode change inside Begin");
	auto state = currentState; state.uvMode = value; ValidState(state); currentState = std::move(state);
}
void ScreenDrawList::RestoreState(const ScreenDrawListState& value)
{
	if (recording) throw std::logic_error("ScreenDrawList state restore inside Begin");
	FiniteVertex({0,0,0,value.color}); FiniteUV(value.texCoord); ValidState(value.draw);
	currentColor = value.color; currentTexCoord = value.texCoord; currentState = value.draw;
}
void ScreenDrawList::Begin(ScreenPrimitive value)
{
	if (recording) throw std::logic_error("ScreenDrawList nested Begin");
	if (value > ScreenPrimitive::Quads) throw std::invalid_argument("ScreenDrawList unsupported primitive");
	pendingVertices.clear(); pendingTexCoords.clear(); batchState = currentState; primitive = value; recording = true;
}
void ScreenDrawList::BeginQuads() { Begin(ScreenPrimitive::Quads); }
void ScreenDrawList::Vertex(float x, float y, float z)
{
	if (!recording) throw std::logic_error("ScreenDrawList Vertex outside Begin");
	const ScreenVertex value{x,y,z,currentColor}; FiniteVertex(value);
	pendingVertices.reserve(pendingVertices.size()+1); pendingTexCoords.reserve(pendingTexCoords.size()+1);
	pendingVertices.push_back(value); pendingTexCoords.push_back(currentTexCoord);
}
void ScreenDrawList::End()
{
	if (!recording) throw std::logic_error("ScreenDrawList End outside Begin");
	const size_t count = pendingVertices.size();
	if ((primitive == ScreenPrimitive::Quads && count % 4) || (primitive == ScreenPrimitive::Triangles && count % 3))
		throw std::invalid_argument("ScreenDrawList incomplete primitive; Abort required after failure");
	if (count > std::numeric_limits<uint32_t>::max()) throw std::length_error("ScreenDrawList exceeds uint32 vertex range");
	std::vector<uint32_t> batchIndices;
	if (primitive == ScreenPrimitive::Quads) {
		for (uint32_t i = 0; i < count; i += 4) batchIndices.insert(batchIndices.end(), {i,i+1,i+2,i,i+2,i+3});
	} else if (primitive == ScreenPrimitive::Triangles) {
		for (uint32_t i = 0; i < count; ++i) batchIndices.push_back(i);
	} else if (count >= 3) {
		for (uint32_t i = 2; i < count; ++i) {
			if (primitive == ScreenPrimitive::TriangleFan) batchIndices.insert(batchIndices.end(), {0,i-1,i});
			else if (i % 2) batchIndices.insert(batchIndices.end(), {i-1,i-2,i});
			else batchIndices.insert(batchIndices.end(), {i-2,i-1,i});
		}
	}
	recording = false;
	try { AppendTriangles(pendingVertices, pendingTexCoords, batchIndices, batchState); }
	catch (...) { recording = true; throw; }
	Abort();
}
void ScreenDrawList::AppendTriangles(std::span<const ScreenVertex> sourceVertices, std::span<const ScreenTexCoord> sourceUVs,
	std::span<const uint32_t> sourceIndices, const ScreenDrawState& state)
{
	if (recording) throw std::logic_error("ScreenDrawList AppendTriangles during Begin");
	ValidState(state);
	if (sourceVertices.size() != sourceUVs.size() || sourceIndices.size() % 3)
		throw std::invalid_argument("ScreenDrawList triangle/UV extent mismatch");
	for (const auto& value: sourceVertices) FiniteVertex(value);
	for (const auto& value: sourceUVs) FiniteUV(value);
	for (uint32_t index: sourceIndices) if (index >= sourceVertices.size()) throw std::invalid_argument("ScreenDrawList local index exceeds vertices");
	if (sourceIndices.empty()) return;
	const auto maximum = std::numeric_limits<uint32_t>::max();
	if (sourceVertices.size() > maximum || vertices.size() > maximum-sourceVertices.size() ||
		sourceIndices.size() > maximum || indices.size() > maximum-sourceIndices.size())
		throw std::length_error("ScreenDrawList exceeds uint32 index range");
	vertices.reserve(vertices.size()+sourceVertices.size()); texCoords.reserve(texCoords.size()+sourceUVs.size());
	indices.reserve(indices.size()+sourceIndices.size()); commands.reserve(commands.size()+1);
	const uint32_t base = static_cast<uint32_t>(vertices.size()), first = static_cast<uint32_t>(indices.size());
	vertices.insert(vertices.end(), sourceVertices.begin(), sourceVertices.end()); texCoords.insert(texCoords.end(), sourceUVs.begin(), sourceUVs.end());
	for (uint32_t index: sourceIndices) indices.push_back(base+index);
	commands.push_back({first,static_cast<uint32_t>(sourceIndices.size()),state});
}
bool ScreenCommandRequiresMapSource(const ScreenDrawCommand& command)
{
	if(command.panelShader)for(const auto& image:command.panelShader->images)
		if(image && image->sourceTexture && image->sourceDependent)return true;
	if(command.sensorPreview)for(const auto& input:command.sensorPreview->images)
  if(input.dependency&&input.dependency->sourceTexture&&input.dependency->sourceDependent)return true;
 return (command.buildSquare && command.buildSquare->mapSourceDependent) || (command.startBox && command.startBox->mapSourceDependent) || (command.weaponRange && command.weaponRange->mapSourceDependent);
}
std::array<std::shared_ptr<const ScreenTexture>,13> ScreenCommandImages(const ScreenDrawCommand& command)
{
	if(command.panelShader)return {command.panelShader->images[0],command.panelShader->images[1],command.panelShader->images[2]};
 // Only final Minimap programs are accepted as Preview markers: their
 // captured sampler-name slots are 0/1/2, never recaptured at GPU preparation.
 if(command.sensorPreview)return {command.sensorPreview->images[0].dependency,
  command.sensorPreview->images[1].dependency,command.sensorPreview->images[2].dependency};
	if(command.unitIcons)return {command.unitIcons->draws.front().atlas};
	if(command.sensorLOS)return {command.sensorLOS->heightDependency};
	if(command.healthBars)return {command.healthBars->atlas};
	if(command.startBox){std::array<std::shared_ptr<const ScreenTexture>,13> images{};std::copy(command.startBox->dependencyImages.begin(),command.startBox->dependencyImages.end(),images.begin());return images;}
	if(command.buildSquare)return {command.buildSquare->dependencyImage};
	if(command.weaponRange)return {command.weaponRange->dependencyImage,command.weaponRange->coverageDependencyImage};
	if(command.pip)return {command.pip->sampledImages[0].image,command.pip->sampledImages[1].image,command.pip->sampledImages[2].image};
	const auto& state=command.state;
 if(state.terraformCapture){std::array<std::shared_ptr<const ScreenTexture>,13> images{};for(unsigned n=0;n<(state.terraformCapture->program==NativeTerraformCaptureProgram::Reproject?2u:1u);++n)images[n]=state.terraformCapture->images[n].texture;return images;}
 if(state.diffusePainter){std::array<std::shared_ptr<const ScreenTexture>,13> images{};const unsigned count=state.diffusePainter->program==NativeDiffusePainterProgram::Copy?1:4;for(unsigned n=0;n<count;++n)images[n]=state.diffusePainter->images[n].texture;return images;}
 if(state.deferredVisualizer){std::array<std::shared_ptr<const ScreenTexture>,13> images{};for(unsigned i=0;i<13;++i)images[i]=state.deferredVisualizer->images[i].texture;return images;}
	return {state.material==ScreenMaterial::Solid?std::shared_ptr<const ScreenTexture>{}:state.texture,
		(state.material==ScreenMaterial::FontCoverage||state.material==ScreenMaterial::MiniMapBackground)?std::shared_ptr<const ScreenTexture>{}:state.secondary.texture,{}};
}
void ScreenDrawList::AppendPiP(std::shared_ptr<const NativePiPDraw> draw)
{
	if(recording)throw std::logic_error("Native PiP marker during Begin");
	if(!draw)throw std::invalid_argument("Missing immutable native PiP packet");
	ValidateNativePiPDraw(*draw);ValidState(draw->surface);
	if(draw->surface.attachmentClear)throw std::invalid_argument("Native PiP packet cannot be an attachment clear");
	if(indices.size()>UINT32_MAX || commands.size()>=UINT32_MAX)throw std::length_error("Native PiP screen command overflow");
	commands.push_back({std::uint32_t(indices.size()),0,draw->surface,std::move(draw)});
}
void ScreenDrawList::AppendStartBox(std::shared_ptr<const NativeStartBoxDraw> draw)
{
	if(recording || !draw)throw std::logic_error("Original StartBox marker lacks a complete draw outside Begin");
	// GLSL samples its captured image bindings independently of legacy
	// texture enables. Validate those through the typed packet, not the
	// fixed-function ScreenMaterial snapshot, which may be unresolved.
	ValidateNativeStartBoxDraw(*draw);ValidState(draw->surface,true);
	if((draw->stage!=NativeStartBoxStage::MiniMap && draw->stage!=NativeStartBoxStage::TextureBake) || draw->surface.attachmentClear)throw std::logic_error("Original StartBox screen marker is not its minimap callin");
	if(indices.size()>UINT32_MAX || commands.size()>=UINT32_MAX)throw std::length_error("Original StartBox command overflow");
	commands.push_back({std::uint32_t(indices.size()),0,draw->surface,{},std::move(draw)});
}
void ScreenDrawList::AppendWeaponRange(std::shared_ptr<const NativeWeaponRangeDraw> draw)
{
	if(recording || !draw)throw std::logic_error("Original range marker lacks a complete draw outside Begin");
	ValidateNativeWeaponRangeDraw(*draw);ValidState(draw->surface,true);
	// The recorder has already bound the actual World R8+D16 target. Keep
	// its typed mask disc in that target list, alongside the original Attack
	// minimap/PiP marker; other World programs remain outside this list.
	const bool worldMask=draw->stage==NativeWorldDrawStage::World && draw->program.inMiniMap<0.5f &&
		draw->program.defines->maskPass && !draw->program.defines->composite;
	const bool attackMiniMap=draw->program.inMiniMap>0.5f && draw->program.defines->family==NativeWeaponRangeFamily::Attack;
	if((!worldMask && !attackMiniMap) || draw->surface.attachmentClear)
		throw std::logic_error("Original range screen marker is not its Attack minimap or actual World mask callback");
	if(indices.size()>UINT32_MAX || commands.size()>=UINT32_MAX)throw std::length_error("Original range command overflow");
	// The typed packet owns its vertices/instances; the fixed-function index range is empty.
	ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw->surface;command.weaponRange=std::move(draw);commands.push_back(std::move(command));
}
void ScreenDrawList::AppendBuildSquare(std::shared_ptr<const NativeBuildSquareDraw> draw)
{
	if(recording||!draw)throw std::logic_error("Original BuildSquare marker requires its complete draw outside Begin");
	ValidateNativeBuildSquareDraw(*draw);ValidState(draw->surface,true);
	if(!draw->program.isMiniMap||draw->surface.attachmentClear)throw std::logic_error("Original BuildSquare screen marker requires actual minimap callback");
	if(indices.size()>UINT32_MAX||commands.size()>=UINT32_MAX)throw std::length_error("Original BuildSquare command overflow");
	ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw->surface;command.buildSquare=std::move(draw);commands.push_back(std::move(command));
}
void ScreenDrawList::AppendHealthBars(std::shared_ptr<const NativeHealthBarsDraw> draw)
{
	if(recording || !draw)throw std::logic_error("Original Healthbars marker needs a complete draw outside Begin");
	ValidateNativeHealthBarsDraw(*draw);ValidState(draw->surface,true);
	if(indices.size()>UINT32_MAX || commands.size()>=UINT32_MAX)throw std::length_error("Original Healthbars command overflow");
	ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw->surface;
	command.healthBars=std::move(draw);commands.push_back(std::move(command));
}
void ScreenDrawList::AppendUnitIcons(std::shared_ptr<const NativeUnitPrimitiveFrame> frame)
{
 if(recording || !frame)throw std::logic_error("Original unit icon marker requires its complete draw outside Begin");
 ValidateNativeUnitIconFrame(*frame);const auto& draw=frame->draws.front();ValidState(draw.surface,true);
 if(draw.surface.attachmentClear)throw std::logic_error("Original unit icon is not an attachment clear");
 if(indices.size()>UINT32_MAX || commands.size()>=UINT32_MAX)throw std::length_error("Original unit icon command overflow");
 ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw.surface;command.unitIcons=std::move(frame);commands.push_back(std::move(command));
}
void ScreenDrawList::AppendSensorLOS(std::shared_ptr<const NativeSensorLOSDraw> draw)
{
 if(recording||!draw)throw std::logic_error("LOS marker requires complete original fan outside Begin");
 ValidateNativeSensorLOSDraw(*draw);ValidState(draw->surface,true);
 if(!draw->program.defines->stencilPass||draw->surface.attachmentClear)throw std::logic_error("LOS target marker is not its original stencil fan");
 if(indices.size()>UINT32_MAX||commands.size()>=UINT32_MAX)throw std::length_error("LOS target command overflow");
 ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw->surface;command.sensorLOS=std::move(draw);commands.push_back(std::move(command));
}
void ScreenDrawList::AppendSensorPreview(std::shared_ptr<const NativeSensorPreviewDraw> draw)
{
 if(recording||!draw)throw std::logic_error("Sensor Preview marker requires a complete draw outside Begin");
 ValidateNativeSensorPreviewDraw(*draw);ValidState(draw->surface,true);
 if(draw->callin!=NativeSensorPreviewCallin::Minimap||draw->program.source->kind!=NativeSensorPreviewProgramKind::Minimap||draw->surface.attachmentClear)
  throw std::logic_error("Sensor Preview UI marker is not the actual final Minimap program/callin");
 if(indices.size()>UINT32_MAX||commands.size()>=UINT32_MAX)throw std::length_error("Sensor Preview minimap command overflow");
 ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw->surface;
 command.sensorPreview=std::move(draw);commands.push_back(std::move(command));
}
void ScreenDrawList::AppendPanelShader(std::shared_ptr<const NativePanelShaderDraw> draw)
{
	if(recording || !draw)throw std::logic_error("Panel shader marker requires a complete draw outside Begin");
	ValidateNativePanelShaderDraw(*draw);ValidState(draw->surface,true);
	if(indices.size()>UINT32_MAX || commands.size()>=UINT32_MAX)throw std::length_error("Panel shader command overflow");
	ScreenDrawCommand command{};command.firstIndex=std::uint32_t(indices.size());command.state=draw->surface;
	command.panelShader=std::move(draw);commands.push_back(std::move(command));
}
void ScreenDrawList::Abort() noexcept { pendingVertices.clear(); pendingTexCoords.clear(); recording = false; }
void ScreenDrawList::ClearGeometry()
{
	if (recording) throw std::logic_error("ScreenDrawList ClearGeometry during active batch");
	vertices.clear(); texCoords.clear(); indices.clear(); commands.clear();
}

void ScreenDrawList::ClearColorAttachment(ScreenColor color, uint32_t width, uint32_t height,
	std::optional<ScreenScissor> scissor, std::array<bool,4> colorWrite)
{
	ScreenClearValues values; values.color=color;
	ClearAttachments(ScreenAttachmentColor,values,width,height,scissor,colorWrite,false,0);
}

void ScreenDrawList::ClearAttachments(unsigned mask, const ScreenClearValues& values, uint32_t width, uint32_t height,
	std::optional<ScreenScissor> scissor, std::array<bool,4> colorWrite, bool depthWrite, uint32_t stencilWriteMask)
{
	if (mask & ~ScreenAttachmentAll) throw std::invalid_argument("Unsupported native attachment clear mask");
	if (!width || !height) throw std::invalid_argument("Screen attachment clear requires explicit nonzero extent");
	for (float channel:values.color) if (!std::isfinite(channel)) throw std::invalid_argument("Nonfinite native attachment clear color");
	if (!std::isfinite(values.depth)) throw std::invalid_argument("Nonfinite native attachment clear depth");
	if (!mask) return;
	ScreenDrawState state;
	state.blend.enabled=false; state.attachmentClear=true; state.scissor=scissor;
	state.colorWrite=(mask & ScreenAttachmentColor)? colorWrite : std::array<bool,4>{false,false,false,false};
	auto& ds=state.depthStencil;
	ds.depthTest=ds.depthWrite=(mask & ScreenAttachmentDepth) && depthWrite;
	ds.depthCompare=ScreenAlphaCompare::Always;
	ds.stencilTest=(mask & ScreenAttachmentStencil) != 0;
	for (auto* face:{&ds.frontStencil,&ds.backStencil}) {
		face->compare=ScreenAlphaCompare::Always;
		face->fail=face->depthFail=face->pass=NativeWorldStencilOp::Replace;
		// glClearStencil uses the low attachment bits; glStencilFunc reference
		// clamping is a distinct operation and must not be used for a clear.
		face->reference=std::uint32_t(values.stencil)&255u;
		face->writeMask=stencilWriteMask;
	}
	auto color=values.color; for (auto& channel:color) channel=std::clamp(channel,0.0f,1.0f);
	const float z=2*std::clamp(values.depth,0.0f,1.0f)-1;
	const std::array<ScreenVertex,4> quad{{{0,0,z,color},{float(width),0,z,color},
		{float(width),float(height),z,color},{0,float(height),z,color}}};
	const std::array<ScreenTexCoord,4> uv{};
	const std::array<uint32_t,6> order{0,1,2,0,2,3};
	AppendTriangles(quad,uv,order,state);
}

std::uint32_t ScreenRenderTargetPixelBytes(ScreenTextureFormat format) noexcept
{
	switch (format) {
		case ScreenTextureFormat::R8Unorm: return 1;
		case ScreenTextureFormat::RG8Unorm:
		case ScreenTextureFormat::R16Float: return 2;
		case ScreenTextureFormat::RGBA16Float: return 8;
		case ScreenTextureFormat::RG16Float:
		case ScreenTextureFormat::RG11B10Float:
		case ScreenTextureFormat::R32Float:
		case ScreenTextureFormat::RGBA8Unorm: return 4;
		default: return 0;
	}
}

std::vector<size_t> OrderScreenRenderTargetPasses(const ScreenFrameView& frame)
{
	const auto passes = frame.targetPasses;
	if (passes.size() > 4096) throw std::length_error("Native UI RTT pass count exceeds bounded contract");
	const auto validTarget = [](const std::shared_ptr<const ScreenTexture>& texture,bool depth=false) {
		if (!texture || !texture->renderTarget || !texture->renderTarget->resourceId ||
			!texture->width || !texture->height || texture->width > 16384 || texture->height > 16384 ||
			texture->width != texture->renderTarget->width || texture->height != texture->renderTarget->height ||
			(!ScreenRenderTargetPixelBytes(texture->format) && !(depth && texture->format==ScreenTextureFormat::Depth16Unorm)) || !texture->flipV ||
			!texture->bytes.empty() || texture->bytesPerRow || !texture->mips.empty())
			throw std::invalid_argument("Native UI RTT requires an owned color target version without CPU payload and with original GL UV origin");
	};
	std::unordered_map<const ScreenTexture*, size_t> producers;
	for (size_t i = 0; i < passes.size(); ++i) {
		const auto& pass = passes[i]; validTarget(pass.destination);
		if (!producers.emplace(pass.destination.get(), i).second) throw std::invalid_argument("Duplicate native UI RTT generation producer");
		if (!pass.drawList || pass.drawList->IsRecording() || pass.coordinateWidth != pass.destination->width ||
			pass.coordinateHeight != pass.destination->height || pass.load > ScreenTargetLoad::DontCare || pass.store > ScreenTargetStore::DontCare)
			throw std::invalid_argument("Native UI RTT requires completed geometry and exact attachment coordinate extent");
		if (!std::all_of(pass.clearColor.begin(), pass.clearColor.end(), [](float v) { return std::isfinite(v); }))
			throw std::invalid_argument("Nonfinite native UI RTT attachment clear");
		if(pass.sourceStage!=NativePostprocessStage::Screen &&
			((pass.sourceStage!=NativePostprocessStage::PreUnit && pass.sourceStage!=NativePostprocessStage::World) || !pass.commandSequence || !pass.sourceDrawFrame || !pass.destination->generation))
			throw std::invalid_argument("World target lacks the actual PreUnit frame/ordinal/output version");
		if(pass.sourceDependent && pass.sourceStage==NativePostprocessStage::Screen && (!pass.commandSequence || !frame.drawList || pass.screenCommandOffset>frame.drawList->Commands().size()))
			throw std::invalid_argument("Source-dependent target has no actual root screen insertion");
		if(pass.depthDestination){
			const auto& d=pass.depthDestination;validTarget(d,true);
			if(!producers.emplace(d.get(),i).second)throw std::invalid_argument("Duplicate coverage depth version producer");
			if(pass.sourceStage!=NativePostprocessStage::World || pass.destination->format!=ScreenTextureFormat::R8Unorm ||
				d->format!=ScreenTextureFormat::Depth16Unorm || !d->generation || !d->renderTarget || d->width!=pass.destination->width || d->height!=pass.destination->height || !d->flipV)
				throw std::invalid_argument("Coverage target lacks actual R8+D16 attachment versions");
			if(pass.previousDepth && (pass.previousDepth->renderTarget!=d->renderTarget || pass.previousDepth->generation>=d->generation || pass.previousDepth->format!=d->format))
				throw std::invalid_argument("Coverage target prior depth identity differs");
		}else if(pass.previousDepth)throw std::invalid_argument("Prior depth supplied without actual depth target");
		if (pass.load == ScreenTargetLoad::Load) {
			if(pass.previous && pass.previous->postprocessVersion) {
				if(pass.previous->renderTarget || !pass.previous->flipV || !pass.previous->bytes.empty() || pass.previous->bytesPerRow ||
					!pass.previous->mips.empty() || pass.previous->width!=pass.destination->width || pass.previous->height!=pass.destination->height)
					throw std::invalid_argument("Copied RTT Load has competing storage or changed extent");
			} else validTarget(pass.previous);
			if (ScreenTextureTargetIdentity(*pass.previous) != pass.destination->renderTarget || pass.previous->format != pass.destination->format ||
				pass.previous->generation >= pass.destination->generation)
				throw std::invalid_argument("Native UI RTT Load requires an earlier version of the same target");
		} else if (pass.previous) throw std::invalid_argument("Native UI RTT previous version supplied without Load");
	}
	const auto sourceDependency=[&](const std::shared_ptr<const ScreenTexture>& texture,const ScreenRenderTargetPass* consumer,std::size_t rootOffset){
		if(!texture)return;const auto found=producers.find(texture.get());if(found==producers.end() || (!passes[found->second].sourceDependent && passes[found->second].sourceStage==NativePostprocessStage::Screen))return;
		const auto& producer=passes[found->second];
		if(producer.sourceStage!=NativePostprocessStage::Screen){
			if(consumer && consumer->sourceStage!=NativePostprocessStage::Screen &&
				(consumer->sourceStage<producer.sourceStage || consumer->sourceDrawFrame!=producer.sourceDrawFrame || (consumer->sourceStage==producer.sourceStage && consumer->commandSequence<=producer.commandSequence)))
				throw std::invalid_argument("World RTT consumer precedes its actual same-frame producer");
			return; // Actual PreUnit target precedes the root screen and screen caches.
		}
		if(consumer && consumer->sourceStage!=NativePostprocessStage::Screen)
			throw std::invalid_argument("World RTT cannot consume a later screen producer");
		if(consumer){if(!consumer->sourceDependent || consumer->screenCommandOffset<producer.screenCommandOffset ||
			(consumer->screenCommandOffset==producer.screenCommandOffset && consumer->commandSequence<=producer.commandSequence))
			throw std::invalid_argument("Cached target precedes its actual source-dependent child/previous generation");}
		else if(rootOffset<producer.screenCommandOffset)throw std::invalid_argument("Screen draw precedes its actual source-dependent target");
	};
	for(const auto& pass:passes){sourceDependency(pass.previous,&pass,0);sourceDependency(pass.previousDepth,&pass,0);for(const auto& command:pass.drawList->Commands())for(const auto& image:ScreenCommandImages(command))sourceDependency(image,&pass,0);}
	if(frame.drawList)for(std::size_t i=0;i<frame.drawList->Commands().size();++i)for(const auto& image:ScreenCommandImages(frame.drawList->Commands()[i]))sourceDependency(image,nullptr,i);
	std::vector<uint8_t> visited(passes.size(), 0);
	std::vector<size_t> result;
	std::function<void(size_t, size_t)> visit;
	const auto dependency = [&](const std::shared_ptr<const ScreenTexture>& texture, size_t depth) {
		if (!texture || !texture->renderTarget) return;
		validTarget(texture,texture->format==ScreenTextureFormat::Depth16Unorm);
		const auto found = producers.find(texture.get());
		if (found == producers.end()) return; // retired earlier-frame version; GPU cache verifies it
		if (passes[found->second].store != ScreenTargetStore::Store) throw std::invalid_argument("Native UI RTT sampled output was discarded");
		visit(found->second, depth + 1);
	};
	visit = [&](size_t index, size_t depth) {
		if (depth > 64) throw std::length_error("Native UI RTT dependency nesting exceeds bounded contract");
		if (visited[index] == 2) return;
		if (visited[index] == 1) throw std::invalid_argument("Cyclic native UI RTT generation dependency");
		visited[index] = 1;
		const auto& pass = passes[index];
		if (pass.load == ScreenTargetLoad::Load) dependency(pass.previous, depth);
		if(pass.previousDepth)dependency(pass.previousDepth,depth);
		for (const auto& command: pass.drawList->Commands()) {
			for (const auto& texture: ScreenCommandImages(command)) {
				if (texture && ScreenTextureTargetIdentity(*texture) == pass.destination->renderTarget)
					throw std::invalid_argument("Native UI RTT self-target sampling is unsupported");
				dependency(texture, depth);
			}
		}
		visited[index] = 2; result.push_back(index);
	};
	for (size_t i = 0; i < passes.size(); ++i) visit(i, 0);
	if (frame.drawList) for (const auto& command: frame.drawList->Commands()) {
		for(const auto& texture:ScreenCommandImages(command))dependency(texture,0);
	}
	return result;
}
} // namespace rendering
