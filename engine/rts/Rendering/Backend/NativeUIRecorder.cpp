/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDeferredVisualizerProgram.h"
#include "NativeStartBoxData.h"
#include "NativeWeaponRangeData.h"
#include "NativeBuildSquareData.h"
#include "NativeSensorPreviewData.h"
#include "NativeSensorLOSData.h"
#include "NativeUnitPrimitiveData.h"
#include "NativeHealthBarsData.h"
#include "NativePanelShaderData.h"
#include "NativeUIRecorder.h"
#include "NativeDecalCaptureProgram.h"
#include "NativeSplatPainterProgram.h"
#include "NativeDiffusePainterProgram.h"
#include "NativeTerraformCaptureProgram.h"
#include "NativeEasyFacingProgram.h"
#include "NativeSquadHullProgram.h"
#include "NativeTerraformOverlayProgram.h"
#include "NativeLOSViewProgram.h"
#include "NativeWaterOverlayProgram.h"
#include "NativeUITextureStorage.h"
#include "NativeOriginalModelContext.h"
#include "NativePiPData.h"
#include "NativeMiniMapData.h"
#include "NativeTextureContext.h"
#include "NativeInheritedClipPlane.h"
#include "NativeWorldDrawGeometry.h"
#include "NativeWorldDrawSequence.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <exception>

namespace rendering::ui {

struct NativeUICommandList {
	std::vector<std::function<void(NativeUIRecorder::Impl&)>> commands;
};

static void Require(bool value, const char* reason)
{
	if (!value) throw std::runtime_error(reason);
}

static const NativeUITextureBinding* TextureObject(const NativeUITextureBinding* binding)
{
	std::array<const NativeUITextureBinding*,64> visited{};
	for (unsigned depth=0;depth<visited.size();++depth) {
		Require(std::find(visited.begin(),visited.begin()+depth,binding)==visited.begin()+depth,"Native UI texture delegation cycle");
		visited[depth]=binding;
		if (binding->source) { binding=binding->source.get();continue; }
		return binding;
	}
	throw std::runtime_error("Native UI texture delegation depth exceeds bound");
}
NativeUITextureBinding::Resolved NativeUITextureBinding::Resolve() const
{
	const auto* object=TextureObject(this);
	if (object->postprocess) {
		const auto& binding = *object->postprocess;
		Require(!binding.deleted && binding.current && binding.current->image == binding.image,
			"Native copied image is deleted or has no actual source contents");
		const auto& version = binding.current; const auto& image = *version->image;
		if (HasNativePostprocessCPUContents(*version))
			return {SelectNativeUITextureSample(version->sourceImage,object->sampler),object->sampler};
		auto snapshot = std::make_shared<ScreenTexture>();
		snapshot->sourceName = "shared-copy-" + std::to_string(image.object);
		snapshot->width = image.width; snapshot->height = image.height; snapshot->generation = version->generation;
		snapshot->flipV = true; snapshot->postprocessVersion = version;
		switch (image.format) {
			case NativePostprocessFormat::RGBA8: snapshot->format=ScreenTextureFormat::RGBA8Unorm; break;
			case NativePostprocessFormat::RG8: snapshot->format=ScreenTextureFormat::RG8Unorm; break;
			case NativePostprocessFormat::R8Unorm: snapshot->format=ScreenTextureFormat::R8Unorm; break;
			case NativePostprocessFormat::RGBA16Float: snapshot->format=ScreenTextureFormat::RGBA16Float; break;
			case NativePostprocessFormat::Depth32: snapshot->format=ScreenTextureFormat::Depth32Float; break;
			case NativePostprocessFormat::Depth24Unorm: snapshot->format=ScreenTextureFormat::Depth24Unorm; break;
			case NativePostprocessFormat::BC1RGBA: throw std::runtime_error("Compressed shared image lacks its original CPU TexImage contents");
			case NativePostprocessFormat::R16Float: snapshot->format=ScreenTextureFormat::R16Float; break;
			case NativePostprocessFormat::R32Float: snapshot->format=ScreenTextureFormat::R32Float; break;
		}
		return {std::move(snapshot),object->sampler};
	}
	Require(object->current!=nullptr,"Native UI texture resource was deleted or has no initialized image");
	return {SelectNativeUITextureSample(object->current,object->sampler),object->sampler};
}

std::shared_ptr<ScreenTexture> MakeRenderTargetTexture(std::uint32_t width,std::uint32_t height,const std::string& sourceName)
{
	return MakeRenderTargetTexture(width,height,sourceName,ScreenTextureFormat::RGBA8Unorm);
}
std::shared_ptr<ScreenTexture> MakeRenderTargetTexture(std::uint32_t width,std::uint32_t height,const std::string& sourceName,ScreenTextureFormat format)
{
	Require(width>0 && height>0 && width<=16384 && height<=16384,"Native UI render target extent outside bounds");
	Require(ScreenRenderTargetPixelBytes(format)!=0 || format==ScreenTextureFormat::Depth16Unorm,"Native UI render target format has no actual attachment producer");
	// Lua-owned targets and engine-owned caches share the same identity space.
	static std::atomic<std::uint64_t> resourceIds{0};
	auto identity=std::make_shared<ScreenRenderTargetIdentity>();
	identity->resourceId=++resourceIds;identity->width=width;identity->height=height;
	auto image=std::make_shared<ScreenTexture>();image->width=width;image->height=height;image->format=format;
	image->sourceName=sourceName;image->renderTarget=std::move(identity);image->flipV=true;
	return image;
}

NativeUIMatrix IdentityMatrix() { return {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; }
NativeUIMatrix TranslationMatrix(float x, float y, float z)
{
	auto result = IdentityMatrix(); result[12] = x; result[13] = y; result[14] = z; return result;
}
NativeUIMatrix ScaleMatrix(float x, float y, float z)
{
	auto result = IdentityMatrix(); result[0] = x; result[5] = y; result[10] = z; return result;
}
NativeUIMatrix RotationMatrix(float degrees, float x, float y, float z)
{
	const float length = std::sqrt(x*x + y*y + z*z);
	Require(std::isfinite(length) && length > 0 && std::isfinite(degrees), "Native UI invalid rotation");
	x /= length; y /= length; z /= length;
	const float angle = degrees * 0.017453292519943295f;
	const float c = std::cos(angle), s = std::sin(angle), t = 1-c;
	return {t*x*x+c, t*x*y+s*z, t*x*z-s*y, 0,
		t*x*y-s*z, t*y*y+c, t*y*z+s*x, 0,
		t*x*z+s*y, t*y*z-s*x, t*z*z+c, 0, 0,0,0,1};
}
NativeUIMatrix OrthoMatrix(float l, float r, float b, float t, float n, float f)
{
	Require(r != l && t != b && f != n, "Native UI singular orthographic matrix");
	return {2/(r-l),0,0,0, 0,2/(t-b),0,0, 0,0,-2/(f-n),0,
		-(r+l)/(r-l),-(t+b)/(t-b),-(f+n)/(f-n),1};
}
static NativeUIMatrix Multiply(const NativeUIMatrix& a, const NativeUIMatrix& b)
{
	NativeUIMatrix result{};
	for (int col = 0; col < 4; ++col)
		for (int row = 0; row < 4; ++row)
			for (int k = 0; k < 4; ++k) result[col*4+row] += a[k*4+row] * b[col*4+k];
	return result;
}
static std::array<float,4> Transform(const NativeUIMatrix& m, std::array<float,4> p)
{
	std::array<float,4> result{};
	for (int row = 0; row < 4; ++row)
		for (int k = 0; k < 4; ++k) result[row] += m[k*4+row] * p[k];
	return result;
}

struct NativeUIRecorder::Impl {
	NativeUIRecorder* owner = nullptr;
	struct State {
		ScreenDrawListState draw;
		NativeWorldDrawState world;
		std::array<float,3> normal{0,0,1};
		// GL binds a texture name, whose storage and sampler can change before
		// the next draw. Only emitted commands retain an immutable image version.
		std::shared_ptr<NativeUITextureBinding> textureBinding;
		bool textureEnabled = false;
		std::shared_ptr<NativeUITextureBinding> textureBinding1;
		bool textureEnabled1 = false;
		bool textureObserved = false, textureObserved1 = false;
		bool scissorEnabled = false;
		NativeUIMatrixMode mode = NativeUIMatrixMode::ModelView;
		std::array<std::vector<NativeUIMatrix>,3> matrices{{{IdentityMatrix()}, {IdentityMatrix()}, {IdentityMatrix()}}};
		struct Attribute {
			unsigned mask;
			ScreenDrawListState draw;
			NativeWorldDrawState world;
			std::array<float,3> normal;
			std::shared_ptr<NativeUITextureBinding> textureBinding;
			bool textureEnabled;
			bool scissorEnabled;
			// GL_TEXTURE_BIT saves the parameters of the object bound at Push,
			// independently of its image storage or later delegation changes.
			std::shared_ptr<NativeUITextureBinding> textureObject;
			ScreenSampler textureParameters;
			std::shared_ptr<NativeUITextureBinding> textureBinding1;
			bool textureEnabled1 = false;
			std::shared_ptr<NativeUITextureBinding> textureObject1;
			ScreenSampler textureParameters1;
			ScreenClearValues clear;
			bool textureHistoryPushed=false;
			bool textureObserved=false,textureObserved1=false;
			uint64_t modelClipContext=0;
			std::array<bool,3> modelClipEnable{};
   bool modelFogEnabled=false;
		};
		std::vector<Attribute> attributes;
	};
	struct Compilation { State before; std::shared_ptr<NativeUICommandList> list; ScreenClearValues beforeClear; };
	State state;
	bool genesisSafe=false,genesisSource=false;
	std::vector<Compilation> compiling;
	struct SamplerUndo {
		std::shared_ptr<NativeUITextureBinding> object;
		ScreenSampler sampler;
	};
	std::vector<SamplerUndo> samplerUndo;
	ScreenDrawList output;
	ScreenDrawList* activeOutput = &output;
	std::vector<ScreenRenderTargetPass> passes;
	std::function<bool(const std::shared_ptr<const NativePostprocessVersion>&)> sourceDependency;
	struct TargetChange {std::shared_ptr<NativeUITextureBinding> binding;std::shared_ptr<const ScreenTexture> previous,published;bool sourceDependent;NativeUITexturePublication texturePublication;std::uint64_t actualSubmission=0;};
	std::vector<TargetChange> targetChanges;
	bool DependsOnSource(const std::shared_ptr<const ScreenTexture>& image) const {
		if(!image)return false;
		if(image->sourceTexture)return image->sourceDependent;
		if(image->postprocessVersion){Require(bool(sourceDependency),"Copied-image target requires the actual source-frame ownership query");return sourceDependency(image->postprocessVersion);}
		for(const auto& pass:passes)if(pass.destination==image || pass.depthDestination==image)return pass.sourceDependent || pass.sourceStage!=NativePostprocessStage::Screen;
		return false;
	}
	void RollbackSourceTargets() noexcept {
		for(auto i=targetChanges.rbegin();i!=targetChanges.rend();++i)if(i->sourceDependent && !i->actualSubmission && i->binding->current==i->published){if(textureObserver)textureObserver->CancelPublication(i->texturePublication);i->binding->current=i->previous;}
		std::erase_if(targetChanges,[](const auto& change){return change.sourceDependent && !change.actualSubmission;});
		std::erase_if(passes,[this](const auto& pass){
			if(pass.actualSubmission || (!pass.sourceDependent && pass.sourceStage==NativePostprocessStage::Screen))return false;
			return std::none_of(targetChanges.begin(),targetChanges.end(),[&](const auto& change){return (change.published==pass.destination || change.published==pass.depthDestination) && change.actualSubmission;});
		});
	}
	ScreenClearValues clear;
	WorldClearHandler worldClearHandler;
	ShadowVertexSource shadowVertexSource;
	NativeUITextureObserver* textureObserver=nullptr;
	std::vector<bool> originalTextureScopes;
	struct Target {
		State before;
		ScreenClearValues beforeClear;
		std::uint32_t width,height;
		ScreenDrawList* beforeOutput;
		std::shared_ptr<ScreenDrawList> geometry;
		std::shared_ptr<const ScreenTexture> destination,previous;
		std::size_t miniMapStageCount;
		std::shared_ptr<const ScreenTexture> depthDestination,previousDepth;
		bool identities=true;std::uint64_t actualSourceDrawFrame=0;
		std::shared_ptr<NativeUITextureBinding> colorBinding,depthBinding;
		std::shared_ptr<const ScreenTexture> publishedColor,publishedDepth;
		bool editorTarget=false,editorWorld=false,worldStageBefore=false;
	};
	std::vector<Target> targets;
	ScreenRenderTargetPass MakeTargetPass(const Target& target) const
	{
	ScreenRenderTargetPass pass;
	pass.destination=target.destination; pass.previous=target.previous; pass.drawList=target.geometry;
	pass.depthDestination=target.depthDestination;pass.previousDepth=target.previousDepth;
	pass.load=pass.previous? ScreenTargetLoad::Load: ScreenTargetLoad::DontCare; pass.store=ScreenTargetStore::Store;
	pass.coordinateWidth=width; pass.coordinateHeight=height;
	pass.editorTarget=target.editorTarget;
	if(target.editorWorld){
		Require(target.editorTarget && !pass.depthDestination && target.actualSourceDrawFrame,
			"Editor RTT lost its original world callin or color attachment owner");
		pass.sourceStage=NativePostprocessStage::World;pass.sourceDrawFrame=target.actualSourceDrawFrame;
	}else if(worldStage){
		const bool coverage=bool(pass.depthDestination);
		Require(miniMapStages.empty() && !pass.drawList->Commands().empty() &&
			((coverage && worldStageId==NativeWorldDrawStage::World && pass.destination->format==ScreenTextureFormat::R8Unorm) ||
			(!coverage && worldStageId==NativeWorldDrawStage::PreUnit && pass.destination->format==ScreenTextureFormat::RGBA16Float)),
			"World target is outside its actual StartBox bake or range coverage callin");
		pass.sourceStage=coverage?NativePostprocessStage::World:NativePostprocessStage::PreUnit;
		pass.sourceDrawFrame=target.actualSourceDrawFrame;
		for(const auto& command:pass.drawList->Commands()){
			if(coverage && command.state.attachmentClear)continue;
			const auto frame=coverage?(command.weaponRange?command.weaponRange->sourceDrawFrame:0):(command.startBox?command.startBox->sourceDrawFrame:0);
			Require(frame && (coverage?(command.weaponRange && command.weaponRange->program.defines && command.weaponRange->program.defines->maskPass):
				(command.startBox && command.startBox->stage==NativeStartBoxStage::TextureBake && command.startBox->program.program==NativeStartBoxProgram::SDFBake)),
				"World target contains a command outside its actual typed producer");
			if(!pass.sourceDrawFrame)pass.sourceDrawFrame=frame;
			Require(pass.sourceDrawFrame==frame,"World target mixes original draw frames");
		}
	}
	pass.sourceDependent=DependsOnSource(pass.previous)||DependsOnSource(pass.previousDepth);
	bool containsSensorPreview=false;
	for(const auto& command:pass.drawList->Commands()){
		containsSensorPreview=containsSensorPreview||bool(command.sensorPreview)||bool(command.sensorLOS);
		pass.sourceDependent=pass.sourceDependent||ScreenCommandRequiresMapSource(command);
		for(const auto& image:ScreenCommandImages(command))pass.sourceDependent=pass.sourceDependent||DependsOnSource(image);
	}
	if(pass.sourceDependent)Require(screen || pass.sourceStage!=NativePostprocessStage::Screen,"Copy-dependent target requires its actual source-frame screen stage");
	if(pass.sourceDependent||containsSensorPreview)pass.screenCommandOffset=std::uint32_t(output.Commands().size());
	pass.commandSequence=NextNativeWorldDrawSequence();
		return pass;
	}
	unsigned typedTargetDepth=0;
	ScreenDrawList& Output() { return *activeOutput; }
	void CheckSample(const std::shared_ptr<const ScreenTexture>& texture) const {
		if (!texture) return;
		const auto identity=ScreenTextureTargetIdentity(*texture);if(!identity)return;
		for (const auto& target: targets)
			Require(target.destination->renderTarget->resourceId != identity->resourceId, "Native UI cannot sample an active parent/self render target");
	}
	ScreenDrawListState DrawState() const
	{
		auto result=state.draw;
		result.draw.colorWrite=state.world.colorWrite;
		result.draw.alphaToCoverage=state.world.alphaToCoverage;
		result.draw.cull=!state.world.cullEnabled?ScreenCull::None:state.world.cull==NativeWorldCull::Front?ScreenCull::Front:state.world.cull==NativeWorldCull::Back?ScreenCull::Back:ScreenCull::FrontAndBack;
		auto& ds=result.draw.depthStencil;
		ds.depthTest=state.world.depthTest; ds.depthWrite=state.world.depthWrite; ds.depthCompare=state.world.depthCompare;
		ds.stencilTest=state.world.stencilTest; ds.frontStencil=state.world.frontStencil; ds.backStencil=state.world.backStencil;
		if (!state.scissorEnabled) result.draw.scissor.reset();
		if (!state.textureEnabled && result.draw.material!=ScreenMaterial::MiniMapBackground) {result.draw.texture.reset();result.draw.material=ScreenMaterial::Solid;}
		if (!state.textureEnabled1) result.draw.secondary.texture.reset();
		return result;
	}
	ScreenDrawListState ResolveDrawState() const
	{
		auto result=DrawState();
		if (result.draw.material!=ScreenMaterial::MiniMapBackground && state.textureEnabled && state.textureBinding) {
			const auto resolved=state.textureBinding->Resolve();
			result.draw.texture=resolved.image; result.draw.sampler=resolved.sampler;
		}
		if (result.draw.material==ScreenMaterial::MiniMapBackground) result.draw.secondary={};
		else if (state.textureEnabled1 && state.textureBinding1) {
			const auto resolved=state.textureBinding1->Resolve();result.draw.secondary.texture=resolved.image;result.draw.secondary.sampler=resolved.sampler;
		}
		if(textureObserver&&state.textureObserved&&result.draw.material!=ScreenMaterial::MiniMapBackground){
			if(textureObserver->Enabled(0)){const auto actual=textureObserver->BoundImage(0);result.draw.texture=actual.image;result.draw.sampler=actual.sampler;result.draw.material=ScreenMaterial::RGBA;result.draw.uvMode=ScreenUVMode::Normalized;}
			else{result.draw.texture.reset();result.draw.material=ScreenMaterial::Solid;}
		}
		if(textureObserver&&state.textureObserved1&&result.draw.material!=ScreenMaterial::MiniMapBackground){
			if(textureObserver->Enabled(1)){const auto actual=textureObserver->BoundImage(1);result.draw.secondary={actual.image,ScreenUVMode::Normalized,actual.sampler};}
			else result.draw.secondary.texture.reset();
		}
		CheckSample(result.draw.texture);CheckSample(result.draw.secondary.texture);
		return result;
	}
	void RestoreTextureParameters(const std::shared_ptr<NativeUITextureBinding>& object, ScreenSampler sampler)
	{
		// GL_COMPILE records the Pop; its compile-time state simulation must
		// never modify the shared texture object before list execution.
		if (!object || !compiling.empty()) return;
		if (replayDepth) samplerUndo.push_back({object,object->sampler});
		object->sampler=sampler;
	}
	bool frame = false, screen = false, genesis = false, primitive = false;
	bool worldStage=false,worldSafe=false,worldZeroToOne=false;
	NativeWorldDrawStage worldStageId=NativeWorldDrawStage::World;
	NativeUIMatrix worldView=IdentityMatrix(),worldProjection=IdentityMatrix(),worldBillboard=IdentityMatrix();
	std::vector<NativeWorldDrawVertex> worldVertices;
	std::vector<NativeGenericShadowVertex> rawShadowVertices;
	bool HasRawShadowSource()const{return worldStage&&worldStageId==NativeWorldDrawStage::ShadowOpaque&&bool(shadowVertexSource);}
	bool screenRaster=false;
	ScreenDrawState worldSurface;
	std::string primitiveOrigin;
	bool legacyPrimitive=false,legacyInvalidOperation=false;
	std::uint64_t legacyInvalidOperationCount=0;
	NativeDrawPrimitive primitiveMode=NativeDrawPrimitive::Quads;
	std::array<ScreenVertex,4> legacyTail;
	std::array<ScreenTexCoord,4> legacyTailUV;
	unsigned legacyTailCount=0;
	std::uint32_t width = 1, height = 1;
	NativeUIFrameRasterMapping rasterMapping;
	NativeUIFrameRasterMapping CurrentRasterMapping() const {
		if(!targets.empty()||typedTargetDepth)return {width,height,width,height,{0,0,width,height}};
		return rasterMapping;
	}
	ScreenViewport CurrentViewport() const { return state.draw.draw.viewport.value_or(CurrentRasterMapping().defaultViewport); }
	std::array<double,4> PhysicalViewport() const {
		const auto mapping=CurrentRasterMapping();const auto vp=CurrentViewport();
		const double sx=double(mapping.renderWidth)/mapping.windowLogicalWidth,sy=double(mapping.renderHeight)/mapping.windowLogicalHeight;
		return {double(vp.x)*sx,double(mapping.renderHeight)-(double(vp.y)+vp.height)*sy,double(vp.width)*sx,double(vp.height)*sy};
	}
	void AppendPhysicalPixels(const ScreenDrawList& geometry,ScreenDrawState drawState) {
		const auto mapping=CurrentRasterMapping();auto vertices=std::vector<ScreenVertex>(geometry.Vertices().begin(),geometry.Vertices().end());
		for(auto& v:vertices){v.x=v.x*float(width)/mapping.renderWidth;v.y=v.y*float(height)/mapping.renderHeight;}
		drawState.viewport=ScreenViewport{0,0,mapping.windowLogicalWidth,mapping.windowLogicalHeight};
		// Original points and lines are not faces. Their pixel-coverage triangles
		// must not acquire polygon culling from the surrounding Lua state.
		drawState.cull=ScreenCull::None;
		Output().AppendTriangles(vertices,geometry.TexCoords(),geometry.Indices(),drawState);
	}
	unsigned replayDepth = 0;
	std::optional<State> stageBefore;
	struct MiniMapStage {State before;bool safeMode;std::uint32_t width,height;};
	std::vector<MiniMapStage> miniMapStages;
	std::vector<ScreenDrawList*> pipMiniMapOutputs;

	bool CanDraw() const { return frame && !typedTargetDepth && (screen || worldStage || !targets.empty()) && (!genesis || !targets.empty()) && compiling.empty(); }
	void ResetPrimitive() noexcept {primitive=false;primitiveOrigin.clear();legacyPrimitive=false;legacyTailCount=0;worldVertices.clear();rawShadowVertices.clear();screenRaster=false;}
	void InvalidLegacyOperation() {legacyInvalidOperation=true;++legacyInvalidOperationCount;}
	void AppendScreenRaster(const NativeWorldDrawCommand& command)
	{
		if(command.indices.empty())return;
		const auto mapping=CurrentRasterMapping();const auto viewport=PhysicalViewport();
		if(viewport[2]==0 || viewport[3]==0)return;
		auto mesh=BuildNativeWorldDrawGeometry(command,viewport,mapping.renderWidth,mapping.renderHeight,mapping.samples);
		std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;vertices.reserve(mesh.vertices.size());uv.reserve(mesh.vertices.size());
		for(const auto& v:mesh.vertices){const auto& p=v.position;Require(p[3]==1,"Native screen raster requires its orthographic basis");vertices.push_back({(p[0]+1)*float(width)*.5f,(p[1]+1)*float(height)*.5f,p[2],v.color});uv.push_back(v.uv);}
		auto surface=command.surface;
		if(mesh.fullViewport)surface.viewport=ScreenViewport{0,0,mapping.windowLogicalWidth,mapping.windowLogicalHeight};
		// Non-fill polygon faces were already culled before coverage expansion.
		// Filled source polygons retain their native GPU cull state.
		if(command.primitive!=NativeDrawPrimitive::Triangles || command.world.frontPolygonMode!=NativePolygonMode::Fill || command.world.backPolygonMode!=NativePolygonMode::Fill)surface.cull=ScreenCull::None;
		if(!mesh.indices.empty())Output().AppendTriangles(vertices,uv,mesh.indices,surface);
	}
	void StartPrimitive(NativeDrawPrimitive mode,const std::string& origin,bool legacy)
	{
		if (primitive) {
			if (legacy) {InvalidLegacyOperation();return;}
			throw std::runtime_error("Native UI nested primitive Begin; active="+primitiveOrigin+"; requested="+origin);
		}
		if (!CanDraw() && compiling.empty())
			throw std::runtime_error("Native UI geometry outside supported screen/list stage (frame="+std::to_string(frame)+
				", screen="+std::to_string(screen)+", genesis="+std::to_string(genesis)+", targets="+std::to_string(targets.size())+
				", replay="+std::to_string(replayDepth)+")");
		// Resolve before opening the primitive so a deleted/feedback resource
		// cannot leave the recorder with a half-open batch after rejection.
		if (CanDraw()) {
			screenRaster=!worldStage && (unsigned(mode)>unsigned(NativeDrawPrimitive::Quads) || state.world.frontPolygonMode!=NativePolygonMode::Fill || state.world.backPolygonMode!=NativePolygonMode::Fill);
			if(worldStage || screenRaster) {if(worldStage)Require(GetWorldDrawRecorder().IsActive(),"Native world frame not begun by host");worldSurface=(HasRawShadowSource()?DrawState():ResolveDrawState()).draw;worldVertices.clear();rawShadowVertices.clear();}
			else {Output().RestoreState(ResolveDrawState());Output().Begin(ScreenPrimitive(mode));}
		}
		primitive=true;primitiveOrigin=origin;legacyPrimitive=legacy;primitiveMode=mode;legacyTailCount=0;
	}
	void FinishPrimitive(bool legacy)
	{
		if (!primitive && legacy) {InvalidLegacyOperation();return;}
		Require(primitive,"Native UI End without Begin");
		if (CanDraw()) {
			if(worldStage){
				auto world=state.world;
				world.inheritedClipPlane=NativeInheritedWaterClipPlane(worldStageId,state.matrices[1].back(),worldZeroToOne);
				std::function<std::shared_ptr<const NativeGenericShadowInput>()> snapshot;
				if(HasRawShadowSource())snapshot=[this]{return shadowVertexSource(rawShadowVertices,state.matrices[0].back(),state.matrices[1].back(),worldZeroToOne);};
				GetWorldDrawRecorder().Record(worldStageId,primitiveMode,worldVertices,worldSurface,world,snapshot);
			}
			else if(screenRaster) {
				auto command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,primitiveMode,worldVertices,worldSurface,state.world);
				AppendScreenRaster(command);
			} else Output().End();
		}
		ResetPrimitive();
	}
	void RecordVertex(float x,float y,float z,float w)
	{
		Require(primitive,"Native UI vertex outside primitive");
		if (!CanDraw()) return;
		// The selected original shader owns texture-coordinate transforms; its
		// raw input cannot use the fixed-function texture-matrix result.
		const auto uv=HasRawShadowSource()?state.draw.texCoord:ProjectUV(state.draw.texCoord);
		if(worldStage) {
			auto clip=Transform(state.matrices[1].back(),Transform(state.matrices[0].back(),{x,y,z,w}));
			if(worldZeroToOne)clip[2]=2*clip[2]-clip[3];
			for(float value:clip)Require(std::isfinite(value),"Native world non-finite clip vertex");
			if(HasRawShadowSource())rawShadowVertices.push_back({{x,y,z,w},{state.normal[0],state.normal[1],state.normal[2],0},state.draw.color,state.draw.texCoord});
			worldVertices.push_back({clip,state.draw.color,uv});return;
		}
		const auto p=Project(x,y,z,w);
		if(screenRaster){worldVertices.push_back({{p[0]*2/float(width)-1,p[1]*2/float(height)-1,p[2],1},state.draw.color,uv});return;}
		const auto emit=[&](const ScreenVertex& vertex,ScreenTexCoord coordinate) {
			Output().Color(vertex.color);Output().TexCoord(coordinate[0],coordinate[1]);Output().MultiTexCoord(1,coordinate[2],coordinate[3]);Output().Vertex(vertex.x,vertex.y,vertex.z);
		};
		const ScreenVertex vertex{p[0],p[1],p[2],state.draw.color};
		if (legacyPrimitive && (primitiveMode==NativeDrawPrimitive::Quads || primitiveMode==NativeDrawPrimitive::Triangles)) {
			// GL ignores an incomplete final primitive. Keep only that bounded
			// tail here, so ScreenDrawList's strict End invariants stay intact.
			legacyTail[legacyTailCount]=vertex;legacyTailUV[legacyTailCount++]=uv;
			const unsigned group=primitiveMode==NativeDrawPrimitive::Quads?4:3;
			if (legacyTailCount==group) {for(unsigned n=0;n<group;++n)emit(legacyTail[n],legacyTailUV[n]);legacyTailCount=0;}
		} else emit(vertex,uv);
	}
	void OutsidePrimitive() const { Require(!primitive, "Native UI state change within primitive"); }
	void Emit(std::function<void(Impl&)> command)
	{
		if (!compiling.empty()) compiling.back().list->commands.push_back(command);
		try { command(*this); }
		catch (...) {
			if (!compiling.empty()) compiling.back().list->commands.pop_back();
			throw;
		}
	}
	void ResetScreen()
	{
		Require(!primitive, "Native UI reset with unfinished primitive");
		// ResetGLState disables stippling, but does not change its parameters.
		// C++ command drawing can update them before the next Lua callback/frame.
		const auto pattern=state.world.stipplePattern,factor=state.world.stippleFactor;
		state = {};
		state.world.stipplePattern=pattern;state.world.stippleFactor=factor;
		state.matrices[1][0] = OrthoMatrix(0, float(width), 0, float(height), -1, 1);
	}
	void ApplyTexture(std::shared_ptr<const ScreenTexture> texture,ScreenSampler sampler,unsigned unit) {
		if(unit>=2)return;CheckSample(texture);
		(unit==0?state.textureObserved:state.textureObserved1)=false;
		if(unit==1){state.draw.draw.secondary={texture,ScreenUVMode::Normalized,sampler};state.textureBinding1.reset();state.textureEnabled1=texture!=nullptr;return;}
		auto& s=state.draw.draw;s.texture=texture;s.material=texture?ScreenMaterial::RGBA:ScreenMaterial::Solid;s.minimapBackground.reset();state.textureBinding.reset();state.textureEnabled=texture!=nullptr;s.uvMode=ScreenUVMode::Normalized;s.sampler=sampler;
	}
	void ApplyResource(const std::shared_ptr<NativeUITextureBinding>&resource,unsigned unit) {
		if(unit>=2)return;const auto resolved=(!compiling.empty()&&resource->postprocess&&!resource->postprocess->current)?NativeUITextureBinding::Resolved{resource->current,resource->sampler}:resource->Resolve();CheckSample(resolved.image);
		(unit==0?state.textureObserved:state.textureObserved1)=false;
		if(unit==1){state.textureBinding1=resource;state.textureEnabled1=true;state.draw.draw.secondary={resolved.image,ScreenUVMode::Normalized,resolved.sampler};return;}
		state.textureBinding=resource;state.textureEnabled=true;auto&s=state.draw.draw;s.texture=resolved.image;s.sampler=resolved.sampler;s.minimapBackground.reset();s.material=s.texture?ScreenMaterial::RGBA:ScreenMaterial::Solid;s.uvMode=ScreenUVMode::Normalized;
	}
	void ApplyEnabled(bool enabled,unsigned unit) {
		if(unit>=2)return;
		if(unit==1){Require(!enabled||state.textureObserved1||state.textureBinding1||state.draw.draw.secondary.texture||!compiling.empty(),"Native UI enabling default secondary texture needs its original fixed function consumer");state.textureEnabled1=enabled;return;}
		Require(!enabled||state.textureObserved||state.textureBinding||state.draw.draw.texture||!compiling.empty(),"Native UI enabling default texture needs its original fixed function consumer");state.textureEnabled=enabled;
	}
	void SyncObservedTexture(unsigned unit){
		if(unit>=2)return;Require(textureObserver!=nullptr,"Typed texture mirror lacks shared source observer");
		(unit==0?state.textureObserved:state.textureObserved1)=true;
		(unit==0?state.textureEnabled:state.textureEnabled1)=textureObserver->Enabled(unit);
		if(unit==0){state.textureBinding.reset();state.draw.draw.minimapBackground.reset();state.draw.draw.material=state.textureEnabled?ScreenMaterial::RGBA:ScreenMaterial::Solid;}
		else state.textureBinding1.reset();
	}
	void ResetDrawingAttributes()
	{
		OutsidePrimitive();
		// LuaOpenGL::ResetGLState does not reset current normal/UV, bound
		// texture objects, scissor rectangles, or either kind of stack.
		state.draw.color={1,1,1,1};
		state.draw.draw.blend={};
		state.draw.draw.alphaTest={false,ScreenAlphaCompare::Greater,0.5f};
		state.textureEnabled=false;state.scissorEnabled=false;
		// Original ResetGLState leaves SAMPLE_ALPHA_TO_COVERAGE unchanged.
		state.world.frontPolygonMode=state.world.backPolygonMode=NativePolygonMode::Fill;
		state.world.lineStipple=false;state.world.lineWidth=1;state.world.pointSize=1;
		{
			auto& s=state.world;
			s.depthTest=false;s.depthCompare=ScreenAlphaCompare::LessEqual;s.depthWrite=false;s.depthClamp=false;
			s.cullEnabled=false;s.cull=NativeWorldCull::Back;s.polygonOffset=false;
			s.stencilTest=false;s.frontStencil.writeMask=s.backStencil.writeMask=~0u;
			s.colorWrite={true,true,true,true};s.combinedColorWrite={true,true,true,true};s.lineStipple=false;s.lineWidth=1;s.pointSize=1;
		}
		if(textureObserver&&compiling.empty()){textureObserver->Reset2D();SyncObservedTexture(0);SyncObservedTexture(1);}
	}
	void ResetMiniMap(bool resetAttributes)
	{
		OutsidePrimitive();
		if (resetAttributes) ResetDrawingAttributes();
		const auto callbackWidth=miniMapStages.empty()?width:miniMapStages.back().width;
		const auto callbackHeight=miniMapStages.empty()?height:miniMapStages.back().height;
		state.matrices[0].back()=ScaleMatrix(1.0f/float(callbackWidth),1.0f/float(callbackHeight),1);
		state.matrices[1].back()=OrthoMatrix(0,1,0,1,0,-1);
		state.matrices[2].back()=IdentityMatrix();
		state.mode=NativeUIMatrixMode::ModelView;
	}
	std::array<float,4> Project(float x, float y, float z, float w) const
	{
		const auto p = Transform(state.matrices[1].back(), Transform(state.matrices[0].back(), {x,y,z,w}));
		for (const auto value: p) Require(std::isfinite(value), "Native UI non-finite projected vertex");
		Require(p[3] != 0, "Native UI vertex has zero homogeneous w");
		Require(p[3] == 1, "Native UI screen slice does not support perspective/homogeneous geometry (projected w must equal 1)");
		// Keep orthographic GL clip depth, including vertices outside [-1,1].
		// The GPU clips whole primitives and interpolates their attributes;
		// flattening or clamping here would incorrectly retain rejected pixels.
		return {(p[0]+1)*float(width)*0.5f, (p[1]+1)*float(height)*0.5f, p[2], 1};
	}
	ScreenTexCoord ProjectUV(const ScreenTexCoord& coordinate) const
	{
		const auto p = Transform(state.matrices[2].back(), {coordinate[0],coordinate[1],0,1});
		Require(std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[3]) && p[3] != 0, "Native UI invalid texture coordinate");
		Require(!worldStage || p[3]==1,"Native world projective texture coordinates require their original q interpolation");
		return {p[0]/p[3],p[1]/p[3],coordinate[2],coordinate[3]};
	}
	void Replay(const std::shared_ptr<const NativeUICommandList>& list)
	{
		Require(list != nullptr && replayDepth < 64, "Native UI invalid or recursively deep list");
		State before = state;
		const auto beforeClear=clear;
		const auto undoBegin=samplerUndo.size();
		++replayDepth;
		try { for (const auto& command: list->commands) command(*this); }
		catch (...) {
			for (auto n=samplerUndo.size();n>undoBegin;--n) samplerUndo[n-1].object->sampler=samplerUndo[n-1].sampler;
			samplerUndo.resize(undoBegin);
			--replayDepth; ResetPrimitive(); Output().Abort(); state=std::move(before); clear=beforeClear; throw;
		}
		--replayDepth;
		if (!replayDepth) samplerUndo.clear();
	}
};

NativeUIRecorder::NativeUIRecorder(): impl(std::make_unique<Impl>()) { impl->owner=this; }
NativeUIRecorder::~NativeUIRecorder() = default;
NativeUIRecorder& GetRecorder() { static NativeUIRecorder recorder; return recorder; }

void NativeUIRecorder::BeginFrame(std::uint32_t w, std::uint32_t h,std::optional<NativeUIFrameRasterMapping> mapping)
{
	Require(std::none_of(impl->targetChanges.begin(),impl->targetChanges.end(),[](const auto& change){return change.actualSubmission!=0;}),
		"Native UI target publications from an actual submission require real retirement or poisoned-context teardown");
	Require(w > 0 && h > 0 && !impl->frame && !impl->genesis && impl->compiling.empty() && impl->targets.empty() && impl->originalTextureScopes.empty() && impl->pipMiniMapOutputs.empty(), "Native UI invalid frame begin");
	impl->RollbackSourceTargets();
	const auto raster=mapping.value_or(NativeUIFrameRasterMapping{w,h,w,h,{0,0,w,h}});
	Require(raster.windowLogicalWidth && raster.windowLogicalHeight && raster.renderWidth && raster.renderHeight && raster.renderWidth<=16384 && raster.renderHeight<=16384,"Native UI actual raster mapping is invalid");
	impl->rasterMapping=raster;
	impl->width = w; impl->height = h; impl->activeOutput = &impl->output; impl->output.ClearGeometry(); impl->ResetScreen(); impl->frame = true;
}
std::uint32_t NativeUIRecorder::RootCommandCount() const
{
	Require(impl->frame && impl->screen && !impl->worldStage && impl->compiling.empty() && impl->targets.empty() && !impl->typedTargetDepth && !impl->primitive, "Postprocess requires the actual root screen stage");
	Require(impl->output.Commands().size() <= UINT32_MAX, "Root screen command offset overflow");
	return std::uint32_t(impl->output.Commands().size());
}

ScreenFrameView NativeUIRecorder::EndFrame()
{
	Require(impl->frame && !impl->primitive && impl->compiling.empty() && impl->targets.empty() && !impl->typedTargetDepth && impl->miniMapStages.empty() && impl->pipMiniMapOutputs.empty() && !impl->worldStage && impl->originalTextureScopes.empty(), "Native UI unbalanced frame end");
	EndScreenStage(); if(impl->genesisSource)EndGenesisStage();else EndResourceStage(); impl->frame = false;
	return {&impl->output, impl->width, impl->height, {}, impl->passes};
}
void NativeUIRecorder::RetireCompletedFrame(bool sourceFrameCompleted)
{
	Require(!impl->frame && impl->targets.empty() && impl->compiling.empty(),"Native UI producer retirement requires a closed, GPU-completed frame");
	if(sourceFrameCompleted){impl->passes.clear();impl->targetChanges.clear();}
	else {std::erase_if(impl->passes,[](const auto& pass){return !pass.sourceDependent && pass.sourceStage==NativePostprocessStage::Screen;});std::erase_if(impl->targetChanges,[](const auto& change){return !change.sourceDependent;});}
}
void NativeUIRecorder::SetSourceDependencyQuery(std::function<bool(const std::shared_ptr<const NativePostprocessVersion>&)> query){impl->sourceDependency=std::move(query);}
void NativeUIRecorder::CommitTargetBinding(std::shared_ptr<NativeUITextureBinding> binding,std::shared_ptr<const ScreenTexture> previous,std::shared_ptr<const ScreenTexture> published)
{
	Require(binding && binding->current==previous,"Native target binding was replaced during its callback");
	auto found=std::find_if(impl->passes.rbegin(),impl->passes.rend(),[&](const auto& pass){return pass.destination==published || pass.depthDestination==published;});
	Require(found!=impl->passes.rend(),"Native target publication has no completed original callback producer");
	impl->targetChanges.push_back({binding,previous,published,found->sourceDependent || found->sourceStage!=NativePostprocessStage::Screen});binding->current=std::move(published);
	try {if(impl->textureObserver)impl->targetChanges.back().texturePublication=impl->textureObserver->Publish(binding);}
	catch(...) {binding->current=std::move(previous);impl->targetChanges.pop_back();throw;}
}
void NativeUIRecorder::CommitCopiedTargetBinding(std::shared_ptr<NativeUITextureBinding> binding,
	std::shared_ptr<const ScreenTexture> previous,std::shared_ptr<const ScreenTexture> published)
{
	Require(binding && binding->current==previous && previous && published && published->postprocessVersion &&
		!published->renderTarget && ScreenTextureTargetIdentity(*previous)==ScreenTextureTargetIdentity(*published),
		"Panel copy must publish back to its original unchanged FBO cell");
	impl->targetChanges.push_back({binding,previous,published,true});
	binding->current=std::move(published);
	try {if(impl->textureObserver)impl->targetChanges.back().texturePublication=impl->textureObserver->Publish(binding);}
	catch(...) {binding->current=std::move(previous);impl->targetChanges.pop_back();throw;}
}
void NativeUIRecorder::CommitTargetPair(std::shared_ptr<NativeUITextureBinding> color,std::shared_ptr<const ScreenTexture> previousColor,
 std::shared_ptr<const ScreenTexture> nextColor,std::shared_ptr<NativeUITextureBinding> depth,
 std::shared_ptr<const ScreenTexture> previousDepth,std::shared_ptr<const ScreenTexture> nextDepth)
{
 const auto first=impl->targetChanges.size();
 try{CommitTargetBinding(depth,previousDepth,nextDepth);CommitTargetBinding(color,previousColor,nextColor);}
 catch(...){
  while(impl->targetChanges.size()>first){auto& change=impl->targetChanges.back();
   if(change.binding->current==change.published){if(impl->textureObserver)impl->textureObserver->CancelPublication(change.texturePublication);change.binding->current=change.previous;}
   impl->targetChanges.pop_back();
  }
  // The callback ended, but neither attachment publication was accepted.
  // Remove only that never-submitted paired producer; successful nested
  // callbacks retain their own independent publications and producer passes.
  std::erase_if(impl->passes,[&](const auto& pass){return pass.destination==nextColor && pass.depthDestination==nextDepth;});
  throw;
 }
}
void NativeUIRecorder::RecordTargetSubmission(std::span<const ScreenRenderTargetPass> passes,std::uint64_t actualSubmission,
	std::shared_ptr<const NativePostprocessFrame> actualSourceFrame) noexcept
{
 if(!actualSubmission)std::terminate(); // Actual common submission identities are nonzero.
 for(const auto& pass:passes)for(auto& owned:impl->passes)if(owned.destination==pass.destination){
  if(owned.actualSubmission && owned.actualSubmission!=actualSubmission)std::terminate();
  owned.actualSubmission=actualSubmission;
 }
 for(const auto& pass:passes)for(auto& change:impl->targetChanges)
  if(change.published==pass.destination || change.published==pass.depthDestination)change.actualSubmission=actualSubmission;
	if(actualSourceFrame)for(const auto& operation:actualSourceFrame->operations)for(const auto& output:operation.outputs)
		for(auto& change:impl->targetChanges)if(change.published->postprocessVersion==output)change.actualSubmission=actualSubmission;
}
void NativeUIRecorder::RetireTargetSubmission(std::uint64_t actualSubmission)
{
	Require(actualSubmission && impl->compiling.empty() && !impl->primitive,
		"Native target prefix retirement requires its actual completed submission");
	std::erase_if(impl->passes,[&](const auto& pass){return pass.actualSubmission==actualSubmission;});
	std::erase_if(impl->targetChanges,[&](const auto& change){return change.actualSubmission==actualSubmission;});
}
NativeUIReadbackPrefix NativeUIRecorder::CaptureReadbackPrefix()
{
	impl->OutsidePrimitive();
	Require(impl->frame && impl->compiling.empty() && !impl->typedTargetDepth && impl->miniMapStages.empty() &&
		impl->pipMiniMapOutputs.empty(),"Native SaveImage requires its actual current framebuffer outside unfinished typed scopes");
	NativeUIReadbackPrefix prefix;
	prefix.screenStage=impl->screen && !impl->worldStage;
	if(!impl->targets.empty() && impl->targets.back().editorWorld){
		prefix.screenStage=false;prefix.readSourceStage=NativePostprocessStage::World;
	}
	prefix.coordinateWidth=impl->targets.empty()?impl->width:impl->targets.front().width;
	prefix.coordinateHeight=impl->targets.empty()?impl->height:impl->targets.front().height;
	// Allocate all prefix/continuation storage before changing any live target.
	prefix.root=std::make_shared<const ScreenDrawList>(impl->output);
	prefix.targets=impl->passes;
	if(!impl->targets.empty()) {
		auto& target=impl->targets.back();
		Require(target.colorBinding && (!target.depthDestination || target.depthBinding),
			"Native target SaveImage requires its actual publication owner");
		Require(target.colorBinding->current==target.publishedColor &&
			(!target.depthBinding || target.depthBinding->current==target.publishedDepth),
			"Native target was deleted or replaced before SaveImage");
		Require(target.destination->generation!=UINT64_MAX &&
			(!target.depthDestination || target.depthDestination->generation!=UINT64_MAX),
			"Native target continuation generation exhausted");
		auto next=std::make_shared<ScreenTexture>(*target.destination);++next->generation;
		std::shared_ptr<ScreenTexture> nextDepth;
		if(target.depthDestination){nextDepth=std::make_shared<ScreenTexture>(*target.depthDestination);++nextDepth->generation;}
		auto geometry=std::make_shared<ScreenDrawList>();
		auto pass=impl->MakeTargetPass(target);
		prefix.targets.push_back(pass);
		impl->passes.push_back(std::move(pass));
		try {
			if(target.depthBinding)CommitTargetPair(target.colorBinding,target.publishedColor,target.destination,
				target.depthBinding,target.publishedDepth,target.depthDestination);
			else CommitTargetBinding(target.colorBinding,target.publishedColor,target.destination);
		} catch(...) {
			std::erase_if(impl->passes,[&](const auto& p){return p.destination==target.destination;});
			throw;
		}
		prefix.readColor=target.destination;prefix.readDepth=target.depthDestination;
		target.publishedColor=target.destination;target.publishedDepth=target.depthDestination;
		target.previous=target.destination;target.previousDepth=target.depthDestination;
		target.destination=std::move(next);target.depthDestination=std::move(nextDepth);
		target.geometry=std::move(geometry);impl->activeOutput=target.geometry.get();
	}
	prefix.cutoff=NextNativeWorldDrawSequence();
	return prefix;
}
void NativeUIRecorder::AbortFrame() noexcept
{
	// Only never-submitted source publications may be reversed below. Exact
	// versions recorded after real Begin survive timeout/error/completion-unwind;
	// runtime poison and the common retained lifetime forbid their reuse.
	impl->output.Abort(); impl->ResetPrimitive(); impl->compiling.clear(); impl->targets.clear(); impl->activeOutput = &impl->output;
	impl->frame = impl->screen = impl->worldStage = false; impl->stageBefore.reset();
	if(!impl->genesisSource){impl->genesis=false;impl->genesisSafe=false;}
	impl->miniMapStages.clear();
	impl->pipMiniMapOutputs.clear();
	impl->RollbackSourceTargets();
	impl->output.ClearGeometry();
	// Keep an acquired real Genesis attribute scope until its actual
	// DisableDrawGenesis owner runs. Transport abort is not a source PopAttrib.
	if(!impl->genesisSource)impl->state = {};
	// The original DisableDraw* owner must close any acquired EnableCommon
	// scope. A failed transport submission is not an extra source glPopAttrib.
	// BeginFrame rejects reuse until that real owner has completed its cleanup.
}
void NativeUIRecorder::BeginScreenStage()
{
	Require(impl->frame && impl->compiling.empty() && !impl->genesis, "Native UI screen stage outside frame");
	if (impl->screen) { ResetScreenStage(); return; }
	impl->stageBefore = impl->state; impl->ResetScreen(); impl->screen = true;
}
void NativeUIRecorder::ResetScreenStage() { Require(impl->screen, "Native UI screen reset outside stage"); impl->ResetScreen(); }
void NativeUIRecorder::EndScreenStage() noexcept
{
	if (!impl->screen) return;
	impl->output.Abort(); impl->ResetPrimitive();
	if (impl->stageBefore) impl->state = std::move(*impl->stageBefore);
	impl->stageBefore.reset(); impl->screen = false;
}
void NativeUIRecorder::BeginWorldStage(NativeWorldDrawStage stage,NativeUIMatrix view,NativeUIMatrix projection,NativeUIMatrix billboard,bool safeMode,bool zeroToOneDepth)
{
	impl->OutsidePrimitive();
	Require(impl->frame && !impl->screen && !impl->genesis && !impl->worldStage && impl->targets.empty() && impl->compiling.empty(),"Native world stage requires its host frame, outside screen/targets");
	Require(GetWorldDrawRecorder().IsActive(),"Native world frame host is not active");
	for(const auto& matrix:{view,projection,billboard})for(float value:matrix)Require(std::isfinite(value),"Native world camera matrix non-finite");
	impl->worldStageId=stage;impl->worldView=view;impl->worldProjection=projection;impl->worldBillboard=billboard;impl->worldSafe=safeMode;impl->worldZeroToOne=zeroToOneDepth;
	if(safeMode)PushAttrib(Blend|Alpha|Enable|Depth|Polygon|Line|Point|ColorMask|Viewport);
	impl->worldStage=true;
	ResetWorldStage();
	if(impl->textureObserver){impl->SyncObservedTexture(0);impl->SyncObservedTexture(1);}
}
void NativeUIRecorder::ResetWorldStage()
{
	Require(impl->worldStage,"Native world reset outside stage");impl->OutsidePrimitive();
	impl->state.matrices[0].back()=impl->worldView;impl->state.matrices[1].back()=impl->worldProjection;impl->state.matrices[2].back()=IdentityMatrix();
	impl->state.mode=NativeUIMatrixMode::ModelView;
	if(impl->worldSafe) {
		impl->ResetDrawingAttributes();
		auto& s=impl->state.world;
		s.depthTest=false;s.depthCompare=ScreenAlphaCompare::LessEqual;s.depthWrite=false;s.depthClamp=false;
		s.cullEnabled=false;s.cull=NativeWorldCull::Back;s.polygonOffset=false;
		s.stencilTest=false;s.frontStencil.writeMask=s.backStencil.writeMask=~0u;
		s.colorWrite={true,true,true,true};s.combinedColorWrite={true,true,true,true};s.lineStipple=false;s.lineWidth=1;s.pointSize=1;
	}
}
void NativeUIRecorder::EndWorldStage()
{
	if(!impl->worldStage)return;
	AbortPrimitive();
	if(impl->worldSafe) {
		impl->state.matrices[0].back()=impl->worldView;impl->state.matrices[1].back()=impl->worldProjection;impl->state.matrices[2].back()=IdentityMatrix();
		impl->state.mode=NativeUIMatrixMode::ModelView;
		PopAttrib();
	}
	impl->worldStage=false;
}
bool NativeUIRecorder::IsWorldStage() const noexcept {return impl->worldStage;}
bool NativeUIRecorder::IsScreenStage() const noexcept {return impl->screen;}
bool NativeUIRecorder::IsGenesisStage() const noexcept {return impl->genesis && impl->genesisSource;}
bool NativeUIRecorder::HasActiveTarget() const noexcept {return !impl->targets.empty();}
void NativeUIRecorder::RecordOperation(std::function<void(NativeUIRecorder&)> operation)
{
	if(!impl->compiling.empty() && impl->primitive){impl->InvalidLegacyOperation();return;}
	impl->Emit([operation=std::move(operation)](Impl& i) {
		if(i.primitive){i.InvalidLegacyOperation();return;}
		i.OutsidePrimitive(); Require(i.owner!=nullptr,"Native recorder owner missing"); operation(*i.owner);
	});
}
std::optional<NativeWorldDrawStage> NativeUIRecorder::CurrentWorldStage() const noexcept
{
	return impl->worldStage ? std::optional(impl->worldStageId) : std::nullopt;
}
NativeWorldDrawState NativeUIRecorder::CurrentWorldState() const {return impl->state.world;}
void NativeUIRecorder::UpdateWorldState(std::function<void(NativeWorldDrawState&)> operation,bool requireWorld,bool legacyPrimitiveRules)
{
	if(legacyPrimitiveRules && !impl->compiling.empty() && impl->primitive){impl->InvalidLegacyOperation();return;}
	impl->Emit([operation=std::move(operation),requireWorld,legacyPrimitiveRules](Impl& i) {
		if(legacyPrimitiveRules && i.primitive){i.InvalidLegacyOperation();return;}
		i.OutsidePrimitive();Require(!requireWorld || i.worldStage || !i.compiling.empty(),"Native world state outside world/list stage");
		auto next=i.state.world;operation(next);
		Require(std::isfinite(next.lineWidth)&&next.lineWidth>0&&std::isfinite(next.pointSize)&&next.pointSize>0,"Native world raster size invalid");
		Require(std::isfinite(next.offsetFactor)&&std::isfinite(next.offsetUnits),"Native world offset non-finite");
		i.state.world=next;
	});
}
void NativeUIRecorder::BeginMiniMapStage(bool safeMode)
{
	impl->OutsidePrimitive();
	Require(impl->frame && !impl->targets.empty() && impl->compiling.empty(),"Native minimap callback requires its actual target");
	Require(impl->miniMapStages.size()<64,"Native minimap callback nesting exceeds bound");
	impl->miniMapStages.push_back({impl->state,safeMode,impl->width,impl->height});
	try {
		for (auto& stack:impl->state.matrices) {
			Require(stack.size()<256,"Native minimap matrix stack overflow");
			stack.push_back(stack.back());
		}
		// Original EnableCommon saves AttribBits, which deliberately excludes
		// CURRENT_BIT, TEXTURE_BIT and SCISSOR_BIT.
		if (safeMode) PushAttrib(NativeUIAttribute::Blend|Alpha|Enable|Polygon|Line|Point|Viewport);
		impl->ResetMiniMap(safeMode);
	} catch (...) {
		impl->state=std::move(impl->miniMapStages.back().before);
		impl->miniMapStages.pop_back();
		throw;
	}
}
void NativeUIRecorder::ResetMiniMapStage()
{
	Require(!impl->miniMapStages.empty(),"Native minimap reset outside callback");
	impl->ResetMiniMap(true);
}
void NativeUIRecorder::EndMiniMapStage(bool safeMode,bool returnToScreen)
{
	Require(!impl->miniMapStages.empty() && impl->compiling.empty(),"Native minimap callback end without begin");
	const auto& stage=impl->miniMapStages.back();
	Require(stage.safeMode==safeMode,"Native minimap callback changed safety mode");
	impl->ResetPrimitive();impl->Output().Abort();
	for (unsigned mode=0;mode<3;++mode)
		Require(impl->state.matrices[mode].size()==stage.before.matrices[mode].size()+1,"Native minimap callback has unbalanced matrices");
	Require(impl->state.attributes.size()==stage.before.attributes.size()+unsigned(safeMode),"Native minimap callback has unbalanced attributes");
	if (safeMode) PopAttrib();
	else if (returnToScreen) impl->ResetDrawingAttributes();
	impl->state.matrices=stage.before.matrices;
	impl->state.mode=NativeUIMatrixMode::ModelView;
	impl->miniMapStages.pop_back();
}
void NativeUIRecorder::RunPiPMiniMapCallback(const std::function<void()>& callback)
{
	impl->OutsidePrimitive();
	Require(callback && impl->CanDraw() && impl->screen && !impl->worldStage &&
		impl->targets.empty() && impl->miniMapStages.empty() && impl->activeOutput==&impl->output,
		"PiP minimap forwarding requires its actual root screen output outside targets/lists");
	const auto depth=impl->pipMiniMapOutputs.size();
	Require(depth<64,"PiP minimap callback nesting exceeds bound");
	auto* const output=impl->activeOutput;
	impl->pipMiniMapOutputs.push_back(output);
	try {
		// PiP already established its original projection, viewport and scissor.
		// Do not run engine minimap setup/reset or change any drawing state here.
		callback();
		Require(impl->pipMiniMapOutputs.size()==depth+1 && impl->pipMiniMapOutputs.back()==output &&
			impl->activeOutput==output && impl->CanDraw() && impl->screen && !impl->worldStage &&
			impl->targets.empty() && impl->miniMapStages.empty() && !impl->primitive,
			"PiP minimap callback did not return to its actual screen output");
		impl->pipMiniMapOutputs.pop_back();
	} catch (...) {
		// The private Lua bridge converts protected-call failure into an
		// exception here before the original outer pcall handles it. Nested
		// invocations retire only their own scope; AbortFrame may retire all.
		if(impl->pipMiniMapOutputs.size()>depth)impl->pipMiniMapOutputs.resize(depth);
		throw;
	}
}
void NativeUIRecorder::ResetDrawingState()
{
	impl->Emit([](Impl& i) {i.ResetDrawingAttributes();});
}
void NativeUIRecorder::ResetDrawingMatrices()
{
	Require(!impl->miniMapStages.empty() || impl->screen || impl->worldStage || !impl->compiling.empty(),"Native matrices reset outside a converted stage/list");
	NativeUIMatrix modelView=IdentityMatrix();
	NativeUIMatrix projection=OrthoMatrix(0,float(impl->width),0,float(impl->height),-1,1);
	if(impl->worldStage){modelView=impl->worldView;projection=impl->worldProjection;}
	if (!impl->miniMapStages.empty()) {
		const auto& stage=impl->miniMapStages.back();
		modelView=ScaleMatrix(1.0f/float(stage.width),1.0f/float(stage.height),1);
		projection=OrthoMatrix(0,1,0,1,0,-1);
	}
	// GL_COMPILE records the matrix literals chosen at the ResetMatrices
	// call, including that minimap's extent; replay must not choose a new stage.
	impl->Emit([modelView,projection](Impl& i) {
		i.OutsidePrimitive();
		i.state.matrices[0].back()=modelView;
		i.state.matrices[1].back()=projection;
		i.state.matrices[2].back()=IdentityMatrix();
		i.state.mode=NativeUIMatrixMode::ModelView;
	});
}
void NativeUIRecorder::BeginResourceStage()
{
	impl->OutsidePrimitive();Require(impl->frame && !impl->screen && !impl->worldStage && !impl->genesis && impl->targets.empty() && impl->compiling.empty(),"Native resource stage overlaps a source draw stage");
	impl->genesis=true;impl->genesisSource=false;impl->genesisSafe=false;
}
void NativeUIRecorder::EndResourceStage()
{
	if(!impl->genesis)return;
	Require(!impl->genesisSource,"Native resource scope cannot end an original Genesis call-in");impl->genesis=false;
}
void NativeUIRecorder::BeginGenesisStage(bool safeMode)
{
	impl->OutsidePrimitive();
	Require(impl->frame && !impl->screen && !impl->worldStage && !impl->genesis && impl->targets.empty() && impl->compiling.empty(),"Native Genesis requires its original non-overlapping source stage");
	// Exact EnableCommon AttribBits: CURRENT/TEXTURE/SCISSOR values and
	// matrix contents are deliberately not restored by glPopAttrib.
	if(safeMode)PushAttrib(Blend|Alpha|Enable|Depth|Polygon|Line|Point|ColorMask|Viewport);
	impl->genesisSafe=safeMode;impl->genesis=true;impl->genesisSource=true;
	if(safeMode)impl->ResetDrawingAttributes();
	for(auto& stack:impl->state.matrices)stack.back()=IdentityMatrix();
	impl->state.mode=NativeUIMatrixMode::ModelView;
}
void NativeUIRecorder::ResetGenesisStage()
{
	Require(impl->genesis && impl->genesisSource,"Native Genesis reset outside its source stage");impl->OutsidePrimitive();
	if(!impl->genesisSafe)return;
	for(auto& stack:impl->state.matrices)stack.back()=IdentityMatrix();
	impl->state.mode=NativeUIMatrixMode::ModelView;
	impl->ResetDrawingAttributes();
}
void NativeUIRecorder::EndGenesisStage()
{
	if(!impl->genesis)return;
	Require(impl->genesisSource,"Native Genesis end requires its actual source call-in");
	AbortPrimitive();
	if(impl->genesisSafe){
		// Original DisableDrawGenesis resets the top matrices to identity;
		// it does not restore the matrices that preceded EnableDrawGenesis.
		for(auto& stack:impl->state.matrices)stack.back()=IdentityMatrix();
		impl->state.mode=NativeUIMatrixMode::ModelView;
		PopAttrib();
	}
	impl->genesis=false;impl->genesisSafe=false;impl->genesisSource=false;
}
void NativeUIRecorder::RunFramebufferState(std::uint32_t width,std::uint32_t height,bool identities,const std::function<void()>& callback)
{
	impl->OutsidePrimitive();Require(IsActive() && impl->compiling.empty() && width && height && callback,"Original private FBO requires its actual callback and extent");
	PushAttrib(Viewport);SetViewport({0,0,width,height});
	if(identities){
		MatrixMode(NativeUIMatrixMode::Projection);PushMatrix();LoadMatrix(IdentityMatrix());
		MatrixMode(NativeUIMatrixMode::ModelView);PushMatrix();LoadMatrix(IdentityMatrix());
	}
	const auto restore=[&]{
		if(identities){MatrixMode(NativeUIMatrixMode::Projection);PopMatrix();MatrixMode(NativeUIMatrixMode::ModelView);PopMatrix();}
		PopAttrib();
	};
	try{callback();}catch(...){restore();throw;}
	restore();
}
bool NativeUIRecorder::HasActiveTypedTarget() const noexcept {return impl->typedTargetDepth!=0;}
void NativeUIRecorder::RunTypedTargetState(std::uint32_t width,std::uint32_t height,const std::function<void()>& callback)
{
 impl->OutsidePrimitive();
 Require(impl->frame&&impl->worldStage&&impl->worldStageId==NativeWorldDrawStage::World&&
  impl->targets.empty()&&impl->compiling.empty()&&width&&height&&callback&&impl->typedTargetDepth<64,
  "Typed target requires its actual DrawWorld callback, extent and bounded source scope");
 Require(impl->state.matrices[0].size()<256&&impl->state.matrices[1].size()<256,
  "Typed target original matrix stack overflow");
 // Only original GL_VIEWPORT_BIT plus the two pushed matrix stacks belong to
 // this scope. In particular glScissor/ColorMask/Blend writes are NOT rolled back.
 auto modelView=impl->state.matrices[0],projection=impl->state.matrices[1];
 const auto viewport=impl->state.draw.draw.viewport;
 const auto oldWidth=impl->width,oldHeight=impl->height;
 const auto depth=impl->typedTargetDepth;
 const auto attributes=impl->state.attributes.size();
 const auto restore=[&]()noexcept{
  impl->state.matrices[0].swap(modelView);impl->state.matrices[1].swap(projection);
  impl->state.draw.draw.viewport=viewport;impl->state.mode=NativeUIMatrixMode::ModelView;
  impl->width=oldWidth;impl->height=oldHeight;impl->typedTargetDepth=depth;
 };
 try{
  ++impl->typedTargetDepth;impl->width=width;impl->height=height;
  impl->state.matrices[1].push_back(IdentityMatrix());
  impl->state.matrices[0].push_back(IdentityMatrix());
  impl->state.mode=NativeUIMatrixMode::ModelView;
  impl->state.draw.draw.viewport=ScreenViewport{0,0,width,height};
  callback();
  Require(!impl->primitive&&impl->compiling.empty()&&impl->frame&&impl->worldStage&&
   impl->worldStageId==NativeWorldDrawStage::World&&impl->typedTargetDepth==depth+1&&
   impl->state.matrices[0].size()==modelView.size()+1&&impl->state.matrices[1].size()==projection.size()+1&&
   impl->state.attributes.size()==attributes,
   "Typed target callback left its actual matrix/attribute/source scope unbalanced");
 }catch(...){restore();throw;}
 restore();
}
bool NativeUIRecorder::IsActive() const noexcept { return impl->screen || impl->genesis || impl->worldStage || !impl->compiling.empty() || !impl->targets.empty(); }
bool NativeUIRecorder::IsCompiling() const noexcept { return !impl->compiling.empty(); }

void NativeUIRecorder::BeginList()
{
	impl->OutsidePrimitive(); Require(impl->compiling.size() < 64, "Native UI list nesting overflow");
	impl->compiling.push_back({impl->state,std::make_shared<NativeUICommandList>(),impl->clear});
}
std::shared_ptr<const NativeUICommandList> NativeUIRecorder::EndList()
{
	Require(!impl->compiling.empty() && !impl->primitive, "Native UI unfinished list");
	auto result = impl->compiling.back().list; impl->clear=impl->compiling.back().beforeClear; impl->state = std::move(impl->compiling.back().before); impl->compiling.pop_back(); return result;
}
void NativeUIRecorder::AbortList() noexcept
{
	if (impl->compiling.empty()) return;
	impl->ResetPrimitive(); impl->Output().Abort(); impl->clear=impl->compiling.back().beforeClear; impl->state = std::move(impl->compiling.back().before); impl->compiling.pop_back();
}
void NativeUIRecorder::CallList(std::shared_ptr<const NativeUICommandList> list)
{
	impl->Emit([list=std::move(list)](Impl& i) { i.Replay(list); });
}
void NativeUIRecorder::CallList(NativeDisplayListReference sourceName)
{
	if (!sourceName.Identity()) return;
	impl->Emit([sourceName=std::move(sourceName)](Impl& i) {
		// Resolve at the actual call, including a compiled parent's replay.
		// A deleted original name is a no-op, never a recycled Lua slot.
		if (auto list=sourceName.Resolve()) i.Replay(list);
	});
}
void NativeUIRecorder::Begin(ScreenPrimitive mode,std::string origin)
{
	impl->Emit([mode,origin=std::move(origin)](Impl& i) {
		i.StartPrimitive(NativeDrawPrimitive(mode),origin,false);
	});
}
void NativeUIRecorder::Vertex(float x, float y, float z, float w)
{
	Require(std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && std::isfinite(w), "Native UI non-finite vertex");
	impl->Emit([x,y,z,w](Impl& i) {
		i.RecordVertex(x,y,z,w);
	});
}
void NativeUIRecorder::End()
{
	impl->Emit([](Impl& i) {i.FinishPrimitive(false);});
}
void NativeUIRecorder::BeginLegacyPrimitive(ScreenPrimitive mode,std::string origin)
{BeginLegacyPrimitive(NativeDrawPrimitive(mode),std::move(origin));}
void NativeUIRecorder::BeginLegacyPrimitive(NativeDrawPrimitive mode,std::string origin)
{
	if (!impl->compiling.empty() && impl->primitive) {impl->InvalidLegacyOperation();return;}
	impl->Emit([mode,origin=std::move(origin)](Impl& i) {i.StartPrimitive(mode,origin,true);});
}
void NativeUIRecorder::LegacyVertex(float x,float y,float z,float w)
{
	Require(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z)&&std::isfinite(w),"Native UI non-finite vertex");
	if (!impl->compiling.empty() && !impl->primitive) {impl->InvalidLegacyOperation();return;}
	impl->Emit([x,y,z,w](Impl& i) {if(!i.primitive){i.InvalidLegacyOperation();return;}i.RecordVertex(x,y,z,w);});
}
void NativeUIRecorder::EndLegacyPrimitive()
{
	if (!impl->compiling.empty() && !impl->primitive) {impl->InvalidLegacyOperation();return;}
	impl->Emit([](Impl& i) {i.FinishPrimitive(true);});
}
bool NativeUIRecorder::TakeLegacyInvalidOperation() noexcept {const bool result=impl->legacyInvalidOperation;impl->legacyInvalidOperation=false;return result;}
std::uint64_t NativeUIRecorder::LegacyInvalidOperationCount() const noexcept {return impl->legacyInvalidOperationCount;}
void NativeUIRecorder::AbortPrimitive() noexcept { impl->ResetPrimitive(); impl->Output().Abort(); }
void NativeUIRecorder::Color(ScreenColor value)
{
	for (const auto x: value) Require(std::isfinite(x), "Native UI non-finite color");
	impl->Emit([value](Impl& i) { i.state.draw.color = value; });
}
void NativeUIRecorder::Normal(float x,float y,float z)
{
	Require(std::isfinite(x) && std::isfinite(y) && std::isfinite(z),"Native UI non-finite normal");
	impl->Emit([x,y,z](Impl& i) {i.state.normal={x,y,z};});
}
std::array<float,3> NativeUIRecorder::CurrentNormal() const {return impl->state.normal;}
void NativeUIRecorder::TexCoord(float u, float v) { MultiTexCoord(0,u,v); }
void NativeUIRecorder::MultiTexCoord(unsigned unit, float u, float v)
{
	Require(unit<2,"Native UI texture unit outside implemented 0/1 contract");
	Require(std::isfinite(u) && std::isfinite(v), "Native UI non-finite UV");
	impl->Emit([unit,u,v](Impl& i) { i.state.draw.texCoord[unit*2]=u;i.state.draw.texCoord[unit*2+1]=v; });
}
void NativeUIRecorder::SetBlend(ScreenBlendState value) { UpdateBlend([value](ScreenBlendState& state){state=value;}); }
void NativeUIRecorder::UpdateBlend(std::function<void(ScreenBlendState&)> operation)
{
	if(!impl->compiling.empty() && impl->primitive && impl->legacyPrimitive){impl->InvalidLegacyOperation();return;}
	impl->Emit([operation=std::move(operation)](Impl& i){if(i.primitive && i.legacyPrimitive){i.InvalidLegacyOperation();return;}i.OutsidePrimitive();auto value=i.state.draw.draw.blend;operation(value);
		Require(value.colorEquation<=ScreenBlendEquation::Maximum && value.alphaEquation<=ScreenBlendEquation::Maximum,"Native UI unsupported blend equation");
		i.state.draw.draw.blend=value;
	});
}
void NativeUIRecorder::SetScissor(std::optional<ScreenScissor> value)
{
	impl->Emit([value](Impl& i) {
		if(i.primitive && i.legacyPrimitive){i.InvalidLegacyOperation();return;}
		i.OutsidePrimitive(); i.state.scissorEnabled=value.has_value();
		if (value) i.state.draw.draw.scissor=value;
	});
}
void NativeUIRecorder::SetScissorEnabled(bool enabled)
{
	impl->Emit([enabled](Impl& i) {if(i.primitive && i.legacyPrimitive){i.InvalidLegacyOperation();return;}i.OutsidePrimitive();i.state.scissorEnabled=enabled;});
}
void NativeUIRecorder::SetViewport(ScreenViewport value)
{
	impl->Emit([value](Impl& i) {
		if(i.primitive && i.legacyPrimitive){i.InvalidLegacyOperation();return;}
		i.OutsidePrimitive(); i.state.draw.draw.viewport=value;
	});
}
void NativeUIRecorder::SetAlphaTest(ScreenAlphaTest value)
{
	Require(std::isfinite(value.reference), "Native UI non-finite alpha reference");
	value.reference = std::clamp(value.reference, 0.0f, 1.0f);
	impl->Emit([value](Impl& i) { i.OutsidePrimitive(); i.state.draw.draw.alphaTest = value; });
}
void NativeUIRecorder::SetTextureObserver(NativeUITextureObserver* observer)
{
 Require(!impl->primitive&&impl->compiling.empty()&&impl->state.attributes.empty()&&impl->originalTextureScopes.empty(),"Cannot change shared texture observer inside an original state/list scope");impl->textureObserver=observer;
}
NativeUITextureObserver* NativeUIRecorder::TextureObserver()const{return impl->textureObserver;}
void NativeUIRecorder::LuaActiveTexture(unsigned actual)
{
	Require(actual<32&&impl->textureObserver,"ActiveTexture requires the actual shared original texture context");
	impl->Emit([actual](Impl&i){i.OutsidePrimitive();if(i.compiling.empty())i.textureObserver->ActiveTexture(actual);});
}
void NativeUIRecorder::LuaTextureEnabled(bool enabled,unsigned requested)
{
 Require(requested<32,"Original texture unit outside actual engine range");
 impl->Emit([enabled,requested](Impl&i){i.OutsidePrimitive();if(i.textureObserver&&i.compiling.empty()){const auto actual=i.textureObserver->LuaEnable(requested,enabled);i.SyncObservedTexture(actual);}else i.ApplyEnabled(enabled,requested);});
}
void NativeUIRecorder::LuaTextureImage(std::shared_ptr<const ScreenTexture> image,ScreenSampler sampler,unsigned requested)
{
 Require(image&&requested<32,"Original Lua image bind lacks actual storage/unit");
 auto recorded=impl->textureObserver?impl->textureObserver->CaptureBind({},image):nullptr;
 impl->Emit([image=std::move(image),sampler,requested,recorded](Impl&i){i.OutsidePrimitive();if(i.textureObserver&&i.compiling.empty()){const auto actual=i.textureObserver->LuaBind(requested,recorded);i.SyncObservedTexture(actual);}else i.ApplyTexture(image,sampler,requested);});
}
void NativeUIRecorder::LuaTextureResource(std::shared_ptr<NativeUITextureBinding> binding,unsigned requested)
{
 Require(binding&&requested<32,"Original Lua image bind lacks actual owner/unit");
 auto recorded=impl->textureObserver?impl->textureObserver->CaptureBind(binding,{}):nullptr;
 impl->Emit([binding=std::move(binding),requested,recorded](Impl&i){i.OutsidePrimitive();if(i.textureObserver&&i.compiling.empty()){const auto actual=i.textureObserver->LuaBind(requested,recorded);i.SyncObservedTexture(actual);}else i.ApplyResource(binding,requested);});
}
void NativeUIRecorder::LuaTextureNoBind(unsigned requested,bool parsed)
{
 Require(requested<32,"Original texture unit outside actual engine range");
 impl->Emit([requested,parsed](Impl&i){i.OutsidePrimitive();if(i.textureObserver&&i.compiling.empty())i.textureObserver->LuaNoBind(requested,parsed);});
}
bool NativeUIRecorder::LuaTextureLookup(unsigned requested,const std::function<NativeUILuaTextureLookup()>&parse)
{
 Require(requested<32&&bool(parse),"Original Lua texture parser/unit is absent");impl->OutsidePrimitive();NativeUILuaTextureLookup result;
 if(impl->textureObserver&&impl->compiling.empty()){
  const auto actual=impl->textureObserver->LuaLookup(requested,[&]{result=parse();return NativeTextureLookup{result.parsed,(result.binding||result.image)?impl->textureObserver->CaptureBind(result.binding,result.image):nullptr,false};});
  // Failed lookup and named ID0 preserve the binding and enable, but loader
  // side effects (if any) are still actual source state for the next draw.
  impl->SyncObservedTexture(actual);return result.parsed;
 }
 result=parse();
 if(result.binding)LuaTextureResource(result.binding,requested);
 else if(result.image)LuaTextureImage(result.image,result.sampler,requested);
 else LuaTextureNoBind(requested,result.parsed);
 return result.parsed;
}
void NativeUIRecorder::SyncObservedTextureBinding(unsigned actual)
{
 Require(actual<32,"Typed texture mirror has invalid original unit");
 impl->Emit([actual](Impl&i){i.OutsidePrimitive();if(i.textureObserver&&i.compiling.empty())i.SyncObservedTexture(actual);});
}
void NativeUIRecorder::BeginOriginalTextureScope(bool safeMode,bool screenCallin)
{
 impl->OutsidePrimitive();Require(impl->compiling.empty(),"Original call-in texture scope inside display-list compile");
 impl->originalTextureScopes.push_back(false);
 if(!impl->textureObserver)return;
 if(safeMode){
  try{impl->textureObserver->Push(false,true);}catch(...){impl->originalTextureScopes.pop_back();throw;}
  impl->originalTextureScopes.back()=true;impl->textureObserver->Reset2D();
 }
 if(screenCallin)impl->textureObserver->Reset2D(); // Screen calls ResetGLState again after Setup.
 impl->SyncObservedTexture(0);impl->SyncObservedTexture(1);
}
void NativeUIRecorder::ResetOriginalTextureState()
{
 impl->Emit([](Impl&i){i.OutsidePrimitive();if(!i.textureObserver||!i.compiling.empty())return;i.textureObserver->Reset2D();i.SyncObservedTexture(0);i.SyncObservedTexture(1);});
}
void NativeUIRecorder::EndOriginalTextureScope()
{
 impl->OutsidePrimitive();Require(!impl->originalTextureScopes.empty(),"Original call-in texture scope underflow");
 const bool pushed=impl->originalTextureScopes.back();
 if(pushed)impl->textureObserver->Pop();
 impl->originalTextureScopes.pop_back();
 if(!impl->textureObserver)return;
 impl->SyncObservedTexture(0);impl->SyncObservedTexture(1);
}
void NativeUIRecorder::SetTexture(std::shared_ptr<const ScreenTexture> texture,ScreenSampler sampler,unsigned unit)
{
 Require(unit<2,"Native UI typed sync texture unit outside implemented 0/1 contract");
 impl->Emit([texture=std::move(texture),sampler,unit](Impl&i){i.OutsidePrimitive();i.ApplyTexture(texture,sampler,unit);});
}
void NativeUIRecorder::SetMiniMapBackground(std::shared_ptr<const NativeMiniMapBackground> background)
{
	Require(background!=nullptr,"Native minimap background requires original resources");
	impl->Emit([background=std::move(background)](Impl& i) {
		i.OutsidePrimitive();
		auto& state=i.state.draw.draw;
		state.material=ScreenMaterial::MiniMapBackground;
		state.minimapBackground=background;
		state.texture.reset();state.uvMode=ScreenUVMode::Normalized;
		i.state.textureBinding.reset();i.state.textureEnabled=false;
	});
}
ScreenDrawListState NativeUIRecorder::CurrentState() const { return impl->DrawState(); }
void NativeUIRecorder::SetTextureResource(std::shared_ptr<NativeUITextureBinding> resource,unsigned unit)
{
 Require(unit<2&&resource,"Native UI typed sync texture owner/unit differs");impl->Emit([resource=std::move(resource),unit](Impl&i){i.OutsidePrimitive();i.ApplyResource(resource,unit);});
}
void NativeUIRecorder::SetTextureEnabled(bool enabled,unsigned unit)
{
 Require(unit<2,"Native UI typed sync texture unit outside implemented 0/1 contract");impl->Emit([enabled,unit](Impl&i){i.OutsidePrimitive();i.ApplyEnabled(enabled,unit);});
}
void NativeUIRecorder::DrawLines(std::vector<ScreenLine> lines)
{
	impl->Emit([lines=std::move(lines)](Impl& i) {
		i.OutsidePrimitive();Require(i.CanDraw() || !i.compiling.empty(),"Native line geometry outside a converted stage");if(!i.CanDraw())return;
		const auto mapping=i.CurrentRasterMapping();const auto vp=i.CurrentViewport();if(!vp.width || !vp.height)return;
		const double sx=double(mapping.renderWidth)/mapping.windowLogicalWidth,sy=double(mapping.renderHeight)/mapping.windowLogicalHeight;
		std::vector<ScreenLine> projected;
		for(auto line:lines) {
			for(auto* vertex:{&line.start,&line.end}){const auto p=i.Project(vertex->x,vertex->y,vertex->z,1);vertex->x=p[0];vertex->y=p[1];vertex->z=p[2];}
			const auto original=line;double enter=0,leave=1;
			const auto clip=[&](double a,double b,double low,double high){if(a==b)return a>=low&&a<=high;double lo=(low-a)/(b-a),hi=(high-a)/(b-a);if(lo>hi)std::swap(lo,hi);enter=std::max(enter,lo);leave=std::min(leave,hi);return enter<=leave;};
			if(!clip(line.start.x,line.end.x,0,i.width)||!clip(line.start.y,line.end.y,0,i.height)||!clip(line.start.z,line.end.z,-1,1))continue;
			const auto at=[&](double t){ScreenVertex vertex;vertex.x=float((vp.x+(original.start.x+(double(original.end.x)-original.start.x)*t)*vp.width/i.width)*sx);vertex.y=float((vp.y+(original.start.y+(double(original.end.y)-original.start.y)*t)*vp.height/i.height)*sy);vertex.z=float(original.start.z+(double(original.end.z)-original.start.z)*t);for(unsigned c=0;c<4;++c)vertex.color[c]=float(original.start.color[c]+(double(original.end.color[c])-original.start.color[c])*t);return vertex;};
			line.start=at(enter);line.end=at(leave);projected.push_back(line);
		}
		auto state=i.DrawState().draw;state.texture.reset();state.minimapBackground.reset();state.material=ScreenMaterial::Solid;state.secondary={};
		auto rasterState=state;rasterState.scissor.reset();ScreenDrawList geometry;
		AppendNativeScreenLines(geometry,projected,rasterState,mapping.renderWidth,mapping.renderHeight,mapping.samples);
		i.AppendPhysicalPixels(geometry,state);
	});
}
void NativeUIRecorder::DrawPixelPoints(std::vector<ScreenVertex> points)
{
	impl->Emit([points=std::move(points)](Impl& i) {
		i.OutsidePrimitive();Require(i.CanDraw() || !i.compiling.empty(),"Native point geometry outside a converted stage");if(!i.CanDraw())return;
		const auto mapping=i.CurrentRasterMapping();const auto vp=i.CurrentViewport();if(!vp.width || !vp.height)return;
		const double sx=double(mapping.renderWidth)/mapping.windowLogicalWidth,sy=double(mapping.renderHeight)/mapping.windowLogicalHeight;
		auto state=i.DrawState().draw;state.texture.reset();state.minimapBackground.reset();state.material=ScreenMaterial::Solid;state.secondary={};
		std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;std::vector<std::uint32_t> indices;
		for(const auto& vertex:points) {
			const auto p=i.Project(vertex.x,vertex.y,vertex.z,1);
			if(p[0]<0 || p[0]>i.width || p[1]<0 || p[1]>i.height || p[2]<-1 || p[2]>1)continue;
			const float x=float(std::floor((vp.x+double(p[0])*vp.width/i.width)*sx)),y=float(std::floor((vp.y+double(p[1])*vp.height/i.height)*sy));
			const auto first=std::uint32_t(vertices.size());for(const auto& corner:std::array<std::array<float,2>,4>{{{x,y},{x+1,y},{x+1,y+1},{x,y+1}}}){vertices.push_back({corner[0],corner[1],p[2],vertex.color});uv.push_back({0,0});}
			for(auto offset:{0u,1u,2u,0u,2u,3u})indices.push_back(first+offset);
		}
		ScreenDrawList geometry;geometry.AppendTriangles(vertices,uv,indices,state);i.AppendPhysicalPixels(geometry,state);
	});
}
void NativeUIRecorder::BeginTarget(std::shared_ptr<const ScreenTexture> destination, std::shared_ptr<const ScreenTexture> previous,
	std::shared_ptr<const ScreenTexture> depthDestination,std::shared_ptr<const ScreenTexture> previousDepth,bool identities,std::uint64_t actualSourceDrawFrame,
	std::shared_ptr<NativeUITextureBinding> colorBinding,std::shared_ptr<NativeUITextureBinding> depthBinding,bool editorTarget)
{
	impl->OutsidePrimitive();
	const bool parentEditor=!impl->targets.empty() && impl->targets.back().editorTarget;
	editorTarget=editorTarget||parentEditor;
	const bool editorWorld=editorTarget && (impl->worldStage || (parentEditor && impl->targets.back().editorWorld));
	if(editorTarget){
		Require(!depthDestination && !previousDepth && identities && impl->miniMapStages.empty() && !impl->typedTargetDepth &&
			(!impl->worldStage || impl->worldStageId==NativeWorldDrawStage::World),
			"Original painter RTT requires its color-only world/screen target scope");
		if(parentEditor)actualSourceDrawFrame=impl->targets.back().actualSourceDrawFrame;
		Require(actualSourceDrawFrame!=0,"Original editor target lacks its draw frame");
	}
	const bool worldBake=!editorTarget && impl->worldStage && impl->worldStageId==NativeWorldDrawStage::PreUnit && impl->miniMapStages.empty();
	const bool coverage=impl->worldStage && impl->worldStageId==NativeWorldDrawStage::World && impl->miniMapStages.empty() && bool(depthDestination);
	Require(impl->frame && (impl->screen || impl->genesis || worldBake || coverage || editorTarget) && impl->compiling.empty(),"Native UI RenderToTexture requires a converted draw stage, outside list compilation");
	Require(destination && destination->renderTarget && ScreenRenderTargetPixelBytes(destination->format)!=0 &&
		destination->width>0 && destination->height>0 && destination->width<=16384 && destination->height<=16384,"Native UI invalid color render target");
	if(worldBake)Require(!depthDestination && identities && destination->format==ScreenTextureFormat::RGBA16Float && destination->generation,
		"Original world RTT currently requires the actual StartBox RGBA16F output version");
	if(depthDestination){
		Require(coverage && actualSourceDrawFrame && destination->format==ScreenTextureFormat::R8Unorm &&
			depthDestination->renderTarget && depthDestination->format==ScreenTextureFormat::Depth16Unorm && depthDestination->generation &&
			depthDestination->width==destination->width && depthDestination->height==destination->height,
			"Coverage FBO requires its actual same-size R8+D16 world attachments");
		if(previousDepth)Require(previousDepth->renderTarget==depthDestination->renderTarget && previousDepth->format==depthDestination->format &&
			previousDepth->generation<depthDestination->generation,"Coverage depth version/load identity differs");
		impl->CheckSample(depthDestination);
	}else Require(!previousDepth,"Previous depth supplied without a depth destination");
	Require(impl->targets.size()<64,"Native UI render target nesting overflow");
	impl->CheckSample(destination);
	if (previous) Require(ScreenTextureTargetIdentity(*previous)==destination->renderTarget && previous->format==destination->format &&
		previous->generation<destination->generation,"Native UI target format/version/load mismatch");
	Require(!depthBinding || (colorBinding && depthDestination),"Native depth binding lacks its paired color target");
	if(colorBinding)Require(colorBinding->current && ScreenTextureTargetIdentity(*colorBinding->current)==destination->renderTarget &&
		((previous && colorBinding->current==previous) || (!previous && !colorBinding->current->generation)),
		"Native target binding does not own the opening color version");
	if(depthBinding)Require(depthBinding->current && ScreenTextureTargetIdentity(*depthBinding->current)==depthDestination->renderTarget &&
		((previousDepth && depthBinding->current==previousDepth) || (!previousDepth && !depthBinding->current->generation)),
		"Native target binding does not own the opening depth version");
	auto geometry=std::make_shared<ScreenDrawList>();
	impl->targets.push_back({impl->state,impl->clear,impl->width,impl->height,impl->activeOutput,geometry,std::move(destination),std::move(previous),impl->miniMapStages.size(),std::move(depthDestination),std::move(previousDepth),identities,actualSourceDrawFrame});
	auto& opened=impl->targets.back();
	opened.editorTarget=editorTarget;opened.editorWorld=editorWorld;opened.worldStageBefore=impl->worldStage;
	// Only recording coordinates change: preserve the actual callin on the
	// target, while its local FBO geometry uses the original identity matrices.
	if(editorTarget)impl->worldStage=false;
	opened.colorBinding=std::move(colorBinding);opened.depthBinding=std::move(depthBinding);
	if(opened.colorBinding)opened.publishedColor=opened.colorBinding->current;
	if(opened.depthBinding)opened.publishedDepth=opened.depthBinding->current;
	impl->activeOutput=geometry.get(); impl->width=impl->targets.back().destination->width; impl->height=impl->targets.back().destination->height;
	try {
		for (unsigned mode=0;identities && mode<2;++mode) {
			Require(impl->state.matrices[mode].size()<256,"Native UI target matrix stack overflow");
			impl->state.matrices[mode].push_back(IdentityMatrix());
		}
	} catch (...) { AbortTarget(); throw; }
	if(identities)impl->state.mode=NativeUIMatrixMode::ModelView;
	impl->state.draw.draw.viewport=ScreenViewport{0,0,impl->width,impl->height};
}
void NativeUIRecorder::EndTarget()
{
	Require(!impl->targets.empty() && !impl->primitive && impl->compiling.empty(),"Native UI unfinished RenderToTexture callback");
	const auto& target=impl->targets.back();
	for (unsigned mode=0;target.identities && mode<2;++mode)
		Require(impl->state.matrices[mode].size()==target.before.matrices[mode].size()+1,"Native UI unbalanced target matrix stack");
	auto pass=impl->MakeTargetPass(target);
	impl->passes.push_back(std::move(pass));
	try {
		if(target.depthBinding)CommitTargetPair(target.colorBinding,target.publishedColor,target.destination,
			target.depthBinding,target.publishedDepth,target.depthDestination);
		else if(target.colorBinding)CommitTargetBinding(target.colorBinding,target.publishedColor,target.destination);
	} catch(...) {
		std::erase_if(impl->passes,[&](const auto& p){return p.destination==target.destination;});
		throw; // Keep this target open for the caller's matching AbortTarget.
	}
	for (unsigned mode=0;target.identities && mode<2;++mode) impl->state.matrices[mode]=target.before.matrices[mode];
	impl->state.draw.draw.viewport=target.before.draw.draw.viewport;
	if(target.identities)impl->state.mode=NativeUIMatrixMode::ModelView;
	impl->width=target.width; impl->height=target.height; impl->activeOutput=target.beforeOutput;
	impl->worldStage=target.worldStageBefore;
	impl->targets.pop_back();
}
void NativeUIRecorder::AbortTarget() noexcept
{
	if (impl->targets.empty()) return;
	auto target=std::move(impl->targets.back()); impl->targets.pop_back();
	if(target.identities){impl->state=std::move(target.before);impl->clear=target.beforeClear;}
	else impl->state.draw.draw.viewport=target.before.draw.draw.viewport;
	impl->width=target.width; impl->height=target.height;
	impl->miniMapStages.resize(target.miniMapStageCount);
	impl->activeOutput=target.beforeOutput; impl->worldStage=target.worldStageBefore; impl->ResetPrimitive();
}
void NativeUIRecorder::AppendPiP(NativePiPDraw draw)
{
	impl->Emit([draw=std::move(draw)](Impl& i) {
		i.OutsidePrimitive();Require(!i.worldStage,"Native PiP requires the original screen or target stage");
		Require(i.CanDraw() || !i.compiling.empty(),"Native PiP outside screen/target/list stage");
		if(!i.CanDraw())return;
		auto packet=std::make_shared<NativePiPDraw>(draw);
		packet->viewportWidth=i.width;packet->viewportHeight=i.height;
		packet->surface=i.DrawState().draw;packet->raster=i.state.world;
		packet->program.uniforms.modelViewProjection=Multiply(i.state.matrices[1].back(),i.state.matrices[0].back());
		for(const auto& image:packet->sampledImages)i.CheckSample(image.image);
		i.Output().AppendPiP(std::move(packet));
	});
}
bool NativeUIRecorder::IsMiniMapStage()const{return !impl->miniMapStages.empty();}
void NativeUIRecorder::AppendStartBox(NativeStartBoxDraw draw)
{
	impl->Emit([draw=std::move(draw)](Impl& i){
		i.OutsidePrimitive();
		const bool minimapShader=draw.program.program==NativeStartBoxProgram::Polygon?draw.program.isMiniMap==1:draw.program.coneIsMiniMap>0.5f;
		const bool pipScreen=i.screen && !i.worldStage && i.targets.empty() && minimapShader;
		const bool bake=draw.program.program==NativeStartBoxProgram::SDFBake;
		if(bake)Require(i.worldStage && i.worldStageId==NativeWorldDrawStage::PreUnit && !i.targets.empty() && i.miniMapStages.empty(),
			"Original StartBox SDF bake requires its actual world PreUnit target");
		else Require(!i.miniMapStages.empty() || pipScreen,"Original StartBox screen draw requires the actual minimap callin or PiP screen shader");
		Require(i.CanDraw(),"Original StartBox minimap has no screen/target output");
		auto packet=std::make_shared<NativeStartBoxDraw>(draw);
		packet->viewportWidth=i.width;packet->viewportHeight=i.height;
		packet->surface=i.DrawState().draw;packet->raster=i.state.world;
		ScreenDrawCommand marker;marker.startBox=packet;
		for(const auto& image:ScreenCommandImages(marker))i.CheckSample(image);
		i.Output().AppendStartBox(std::move(packet));
	});
}
void NativeUIRecorder::AppendWeaponRange(NativeWeaponRangeDraw draw)
{
	impl->Emit([draw=std::move(draw)](Impl& i){
		i.OutsidePrimitive();
		const bool pipScreen=i.screen && !i.worldStage && i.targets.empty() && draw.program.inMiniMap>0.5f;
		const bool coverageTarget=i.worldStage && i.worldStageId==NativeWorldDrawStage::World && !i.targets.empty() &&
			i.targets.back().depthDestination && draw.program.defines && draw.program.defines->maskPass;
		Require(!i.miniMapStages.empty() || pipScreen || coverageTarget,"Original range requires actual minimap/PiP or world coverage target");
		Require(i.CanDraw(),"Original range minimap has no screen/target output");
		auto packet=std::make_shared<NativeWeaponRangeDraw>(draw);packet->surface=i.DrawState().draw;packet->raster=i.state.world;
		ScreenDrawCommand marker;marker.weaponRange=packet;
		for(const auto& image:ScreenCommandImages(marker))i.CheckSample(image);
		i.Output().AppendWeaponRange(std::move(packet));
	});
}
void NativeUIRecorder::AppendBuildSquare(NativeBuildSquareDraw draw)
{
	impl->Emit([draw=std::move(draw)](Impl& i){
		i.OutsidePrimitive();
		const bool pipScreen=i.screen&&!i.worldStage&&i.targets.empty()&&draw.program.isMiniMap;
		Require((!i.miniMapStages.empty()||pipScreen)&&i.CanDraw(),"Original BuildSquare needs actual minimap/PiP output");
		auto packet=std::make_shared<NativeBuildSquareDraw>(draw);packet->surface=i.DrawState().draw;packet->raster=i.state.world;
		i.Output().AppendBuildSquare(std::move(packet));
	});
}
void NativeUIRecorder::AppendHealthBars(std::shared_ptr<const NativeHealthBarsDraw> draw)
{
	impl->OutsidePrimitive();
	Require(draw && impl->CanDraw() && impl->screen && !impl->worldStage && impl->targets.empty() &&
		impl->compiling.empty() && impl->miniMapStages.empty() && impl->pipMiniMapOutputs.empty(),
		"Original Healthbars require the actual root DrawScreenEffects output");
	ValidateNativeHealthBarsDraw(*draw);impl->CheckSample(draw->atlas);
	// Retain the exact sampled atlas, UBO/model publication and scalar values
	// from this draw. Ordered postprocess copies see this marker's real offset.
	impl->Output().AppendHealthBars(std::move(draw));
}
void NativeUIRecorder::AppendWaterOverlay(std::span<const std::array<float,2>> positions,std::shared_ptr<const NativeWaterOverlayInput> input)
{
 auto& i=*impl;i.OutsidePrimitive();Require(input&&i.CanDraw()&&i.worldStage&&i.worldStageId==(input->fog?NativeWorldDrawStage::World:NativeWorldDrawStage::PreUnit)&&!IsCompiling()&&!HasActiveTarget()&&!HasActiveTypedTarget(),"Water Overlay requires original immediate world stage");
 auto captured=std::make_shared<NativeWaterOverlayInput>(*input);captured->clipZeroToOne=i.worldZeroToOne;input=std::move(captured);
 ValidateNativeWaterOverlay(*input);Require(positions.size()%3==0&&!positions.empty(),"Water Overlay indexed triangle range missing");
 Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Water Overlay special raster state requires general implementation");
 for(unsigned n=0;n<(input->fog?3u:4u);++n)i.CheckSample(input->images[n].texture);
 auto surface=i.DrawState().draw;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};surface.minimapBackground.reset();surface.sampler={};
 std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(positions.size());
 // Preserve original plane positions for the paired GPU vertex shader. The
 // common fill path passes these through; it must not apply fixed matrices.
 for(const auto& p:positions)vertices.push_back({{p[0],p[1],0,1},{1,1,1,1},{}});
 GetWorldDrawRecorder().RecordWaterOverlay(vertices,surface,i.state.world,std::move(input));
}
void NativeUIRecorder::AppendLOSView(std::span<const std::array<float,4>> positions,std::shared_ptr<const NativeLOSViewInput> input)
{
 auto& i=*impl;i.OutsidePrimitive();Require(i.CanDraw()&&i.worldStage&&i.worldStageId==NativeWorldDrawStage::PreDecals&&!IsCompiling()&&!HasActiveTarget()&&!HasActiveTypedTarget(),"LOS View requires original immediate DrawPreDecals");
 Require(bool(input)&&positions.size()==6,"LOS View original quad/program missing");ValidateNativeLOSView(*input);
 Require(i.textureObserver&&!i.state.world.inheritedClipPlane,"LOS View actual sampler observer/clip output differs");
 auto depth=i.textureObserver->BoundImage(0),image=i.textureObserver->BoundImage(2);
 Require(depth.image&&image.image&&!depth.image->incomplete&&!image.image->incomplete,"LOS View actual bound image missing");
 Require(depth.image->format==ScreenTextureFormat::Depth32Float||depth.image->format==ScreenTextureFormat::Depth24Unorm||depth.image->format==ScreenTextureFormat::Depth16Unorm,"LOS View original map depth binding is not depth storage");
 Require(image.image->format!=ScreenTextureFormat::Depth32Float&&image.image->format!=ScreenTextureFormat::Depth24Unorm&&image.image->format!=ScreenTextureFormat::Depth16Unorm,"LOS View original screen-copy image is not color storage");
 Require(bool(image.image->postprocessVersion),"LOS View actual screen-copy version missing");
 i.CheckSample(depth.image);i.CheckSample(image.image);i.CheckSample(input->los);
 auto surface=i.DrawState().draw;surface.material=ScreenMaterial::RGBA;surface.texture=depth.image;surface.sampler=depth.sampler;surface.uvMode=ScreenUVMode::Normalized;surface.secondary={image.image,ScreenUVMode::Normalized,image.sampler};surface.minimapBackground.reset();
 std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(6);for(const auto& p:positions)vertices.push_back({{p[0],p[1],0,1},{1,1,1,1},{p[2],p[3],0,0}});
 // Exact original VS uses raw position.xy and position.zw, not fixed matrices.
 GetWorldDrawRecorder().RecordLOSView(vertices,surface,i.state.world,std::move(input));
}
void NativeUIRecorder::AppendTerraformOverlay(std::span<const std::array<float,4>> positions,std::shared_ptr<const NativeTerraformOverlayInput> input)
{
 auto& i=*impl;i.OutsidePrimitive();Require(i.CanDraw()&&i.worldStage&&i.worldStageId==NativeWorldDrawStage::PreUnit&&!IsCompiling()&&!HasActiveTarget()&&!HasActiveTypedTarget(),"Terraform Overlay requires original immediate DrawWorldPreUnit");
 Require(bool(input)&&positions.size()==6,"Terraform Overlay original quad/program missing");ValidateNativeTerraformOverlay(*input);
 Require(i.textureObserver&&!i.state.world.inheritedClipPlane,"Terraform Overlay actual sampler observer/clip output differs");
 auto depth=i.textureObserver->BoundImage(0),image=i.textureObserver->BoundImage(1);
 Require(depth.image&&image.image&&!depth.image->incomplete&&!image.image->incomplete,"Terraform Overlay actual bound image missing");
 Require(depth.image->format==ScreenTextureFormat::Depth32Float||depth.image->format==ScreenTextureFormat::Depth24Unorm||depth.image->format==ScreenTextureFormat::Depth16Unorm,"Terraform Overlay original map depth binding is not depth storage");
 Require(image.image->format!=ScreenTextureFormat::Depth32Float&&image.image->format!=ScreenTextureFormat::Depth24Unorm&&image.image->format!=ScreenTextureFormat::Depth16Unorm,"Terraform Overlay original file image is not color storage");
 i.CheckSample(depth.image);i.CheckSample(image.image);
 auto surface=i.DrawState().draw;surface.material=ScreenMaterial::RGBA;surface.texture=depth.image;surface.sampler=depth.sampler;surface.uvMode=ScreenUVMode::Normalized;surface.secondary={image.image,ScreenUVMode::Normalized,image.sampler};surface.minimapBackground.reset();
 std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(6);for(const auto& p:positions)vertices.push_back({{p[0],p[1],0,1},{1,1,1,1},{p[2],p[3],0,0}});
 // Exact original VS uses raw position.xy and position.zw, not fixed matrices.
 GetWorldDrawRecorder().RecordTerraformOverlay(vertices,surface,i.state.world,std::move(input));
}
void NativeUIRecorder::AppendSquadHull(std::span<const std::array<float,3>> positions,NativeDrawPrimitive primitive,std::shared_ptr<const NativeSquadHullInput> input)
{
 auto& i=*impl;i.OutsidePrimitive();Require(i.CanDraw()&&i.worldStage&&i.worldStageId==NativeWorldDrawStage::PreUnit&&!IsCompiling()&&!HasActiveTarget()&&!HasActiveTypedTarget(),"Squad Hull requires original immediate DrawWorldPreUnit");
 Require(bool(input),"Squad Hull original program snapshot missing");ValidateNativeSquadHull(*input);
 Require(!i.state.world.inheritedClipPlane,"Squad Hull inherited shader clip output requires its original contract");
 std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(positions.size());
 for(const auto& p:positions){auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),{p[0],p[1],p[2],1}));if(i.worldZeroToOne)clip[2]=2*clip[2]-clip[3];vertices.push_back({clip,input->color,{p[0],p[2],0,0}});}
 // worldPos is the original raw VBO position, not model-view transformed.
 // Both fill and border retain it through homogeneous clipping/interpolation.
 auto surface=i.DrawState().draw;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};surface.minimapBackground.reset();surface.sampler={};
 GetWorldDrawRecorder().RecordSquadHull(primitive,vertices,surface,i.state.world,std::move(input));
}
void NativeUIRecorder::AppendEasyFacing(std::span<const std::array<float,4>> local,std::shared_ptr<const NativeEasyFacingInput> program)
{
 auto& i=*impl;i.OutsidePrimitive();Require(i.worldStage&&!IsCompiling()&&!HasActiveTarget()&&!HasActiveTypedTarget()&&local.size()==4,"Easy Facing requires its original immediate world quad");
 Require(i.worldStageId==NativeWorldDrawStage::PreUnit||i.worldStageId==NativeWorldDrawStage::World,"Easy Facing callback differs");
 Require(bool(program),"Easy Facing original program snapshot missing");ValidateNativeEasyFacing(*program);std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(local.size());
 for(const auto& p:local){auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),p));if(i.worldZeroToOne)clip[2]=2*clip[2]-clip[3];vertices.push_back({clip,i.state.draw.color,{p[0],p[2],0,0}});}
 // The original GLSL reads only gl_Vertex and its uniforms; texture state is
 // unconsumed and must neither sample nor resolve a fixed-function binding.
 auto surface=i.DrawState().draw;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};surface.minimapBackground.reset();
 auto world=i.state.world;world.inheritedClipPlane=NativeInheritedWaterClipPlane(i.worldStageId,i.state.matrices[1].back(),i.worldZeroToOne);
 GetWorldDrawRecorder().RecordEasyFacing(i.worldStageId,vertices,surface,world,std::move(program));
}
void NativeUIRecorder::AppendUnitIcons(std::shared_ptr<const NativeUnitPrimitiveFrame> frame)
{
 impl->OutsidePrimitive();
 Require(frame && impl->CanDraw() && impl->screen && !impl->worldStage && impl->targets.empty() && impl->compiling.empty() && impl->miniMapStages.empty() && impl->pipMiniMapOutputs.empty(),"Original unit icons require their actual root DrawScreenEffects output");
 ValidateNativeUnitIconFrame(*frame);impl->CheckSample(frame->draws.front().atlas);impl->Output().AppendUnitIcons(std::move(frame));
}
void NativeUIRecorder::AppendSensorLOS(std::shared_ptr<const NativeSensorLOSDraw> draw)
{
 impl->OutsidePrimitive();
 Require(impl->compiling.empty()&&impl->genesis&&!impl->worldStage&&!impl->screen&&!impl->targets.empty()&&impl->CanDraw()&&draw,"LOS stencil fan requires its original DrawGenesis RTT");
 Require(impl->targets.back().destination->format==ScreenTextureFormat::R8Unorm&&!impl->targets.back().depthDestination,"LOS original target must be color-only R8");
 ValidateNativeSensorLOSDraw(*draw);Require(draw->program.defines->stencilPass,"LOS outline cannot be redirected into a target");
 ScreenDrawCommand marker{};marker.sensorLOS=draw;for(const auto& image:ScreenCommandImages(marker))impl->CheckSample(image);
 impl->Output().AppendSensorLOS(std::move(draw));
}
void NativeUIRecorder::AppendSensorPreview(std::shared_ptr<const NativeSensorPreviewDraw> draw)
{
 impl->OutsidePrimitive();
 const bool pipCallback=!impl->pipMiniMapOutputs.empty() && impl->pipMiniMapOutputs.back()==impl->activeOutput &&
  impl->activeOutput==&impl->output && impl->screen && !impl->worldStage && impl->targets.empty();
 Require(impl->compiling.empty()&&(!impl->miniMapStages.empty()||pipCallback)&&impl->CanDraw()&&draw,
  "Sensor Preview marker requires its actual active engine/PiP Minimap callback outside list/primitive/typed RTT");
 ValidateNativeSensorPreviewDraw(*draw);
 Require(draw->callin==NativeSensorPreviewCallin::Minimap&&draw->program.source->kind==NativeSensorPreviewProgramKind::Minimap,
  "Sensor Preview marker cannot redirect a World/private RTT program into cached UI");
 // The caller just captured current state and image versions. Retain it;
 // rebuilding here would reselect a newer World producer at the wrong time.
 ScreenDrawCommand marker{};marker.sensorPreview=draw;
 for(const auto& image:ScreenCommandImages(marker))impl->CheckSample(image);
 impl->Output().AppendSensorPreview(std::move(draw));
}
void NativeUIRecorder::AppendPanelShader(NativePanelShaderDraw draw)
{
	impl->Emit([draw=std::move(draw)](Impl& i) {
		i.OutsidePrimitive();
		Require(!i.worldStage,"Original GUI/Pause shader requires its screen or screen RTT callin");
		Require(i.CanDraw() || !i.compiling.empty(),"Original GUI/Pause shader outside screen/target/list stage");
		if(!i.CanDraw())return;
		auto packet=std::make_shared<NativePanelShaderDraw>(draw);
		packet->targetWidth=i.width;packet->targetHeight=i.height;
		// GLSL images were captured explicitly by the family at this replay.
		// ResolveDrawState would incorrectly apply GL_TEXTURE_2D enable flags.
		packet->surface=i.DrawState().draw;
		packet->modelViewProjection=Multiply(i.state.matrices[1].back(),i.state.matrices[0].back());
		packet->textureMatrix=i.state.matrices[2].back();
		for(const auto& image:packet->images)i.CheckSample(image);
		i.Output().AppendPanelShader(std::move(packet));
	});
}
void NativeUIRecorder::ClearTarget(std::optional<ScreenColor> color)
{
	Require(!impl->targets.empty(),"Native UI color clear requires an active converted target");
	ClearAttachments(ScreenAttachmentColor,color);
}
void NativeUIRecorder::SetWorldClearHandler(WorldClearHandler handler)
{
	impl->worldClearHandler=std::move(handler);
}
void NativeUIRecorder::SetShadowVertexSource(ShadowVertexSource source)
{
	Require(!impl->primitive&&impl->compiling.empty()&&!impl->worldStage,"Cannot change original shadow vertex source inside a primitive/list/stage");impl->shadowVertexSource=std::move(source);
}
ScreenClearValues NativeUIRecorder::CurrentClearValues() const {return impl->clear;}
void NativeUIRecorder::ClearAttachments(unsigned mask, std::optional<ScreenColor> color,
	std::optional<float> depth, std::optional<std::int32_t> stencil)
{
	Require(!(mask & ~ScreenAttachmentAll),"Native UI unsupported attachment clear mask");
	if(color)for(auto& channel:*color){Require(std::isfinite(channel),"Native UI non-finite clear color");channel=std::clamp(channel,0.0f,1.0f);}
	if(depth){Require(std::isfinite(*depth),"Native UI non-finite clear depth");*depth=std::clamp(*depth,0.0f,1.0f);}
	if(!impl->compiling.empty() && impl->primitive){impl->InvalidLegacyOperation();return;}
	impl->Emit([mask,color,depth,stencil](Impl& i) {
		if(i.primitive){i.InvalidLegacyOperation();return;}
		Require(i.CanDraw() || !i.compiling.empty(),"Native UI clear requires an actual draw stage or list");
		if(color)i.clear.color=*color;
		if(depth)i.clear.depth=*depth;
		if(stencil)i.clear.stencil=*stencil;
		if(!i.CanDraw() || !mask)return;
		const auto scissor=i.DrawState().draw.scissor;
		if(i.worldStage && i.targets.empty()){
			Require(bool(i.worldClearHandler),"Native world attachment clear has no active ordered consumer");
			i.worldClearHandler(mask,i.clear,i.state.world,scissor,i.worldStageId);
		}else{
			i.Output().ClearAttachments(mask,i.clear,i.width,i.height,scissor,i.state.world.colorWrite,
				i.state.world.depthWrite,i.state.world.frontStencil.writeMask);
		}
	});
}
void NativeUIRecorder::SetOriginalClearColor(ScreenColor color)
{
 for(auto& value:color){Require(std::isfinite(value),"Original clear color is nonfinite");value=std::clamp(value,0.f,1.f);}
 impl->Emit([color](Impl& i){i.OutsidePrimitive();i.clear.color=color;});
}
void NativeUIRecorder::SetOriginalClearDepth(double depth)
{
 Require(std::isfinite(depth),"Original clear depth is nonfinite");depth=std::clamp(depth,0.,1.);
 impl->Emit([depth](Impl& i){i.OutsidePrimitive();i.clear.depth=depth;});
}
NativeUIMatrixMode NativeUIRecorder::CurrentMatrixMode() const{return impl->state.mode;}
void NativeUIRecorder::MatrixMode(NativeUIMatrixMode mode) { impl->Emit([mode](Impl& i) { i.OutsidePrimitive(); i.state.mode = mode; }); }
void NativeUIRecorder::LoadMatrix(NativeUIMatrix value)
{
	for (const auto x: value) Require(std::isfinite(x), "Native UI non-finite matrix");
	impl->Emit([value](Impl& i) { i.OutsidePrimitive(); i.state.matrices[unsigned(i.state.mode)].back() = value; });
}
void NativeUIRecorder::MultMatrix(NativeUIMatrix value)
{
	for (const auto x: value) Require(std::isfinite(x), "Native UI non-finite matrix");
	impl->Emit([value](Impl& i) { i.OutsidePrimitive(); auto& m = i.state.matrices[unsigned(i.state.mode)].back(); m = Multiply(m,value); });
}
NativeUIMatrix NativeUIRecorder::GetMatrix(NativeUIMatrixMode mode) const { return impl->state.matrices[unsigned(mode)].back(); }
void NativeUIRecorder::PushMatrix()
{
	impl->Emit([](Impl& i) { i.OutsidePrimitive(); auto& v = i.state.matrices[unsigned(i.state.mode)]; Require(v.size() < 256,"Native UI matrix overflow"); v.push_back(v.back()); });
}
void NativeUIRecorder::PopMatrix()
{
	impl->Emit([](Impl& i) {
		i.OutsidePrimitive(); auto& v = i.state.matrices[unsigned(i.state.mode)];
		// GL_COMPILE records relative stack effects. A list may pop a matrix
		// pushed by its caller or by an earlier list (the original TopBar wind
		// lists do exactly this). Validate against the real stack at replay.
		if (v.size()==1 && !i.compiling.empty()) return;
		Require(v.size()>1,"Native UI matrix underflow"); v.pop_back();
	});
}
void NativeUIRecorder::PushAttrib(unsigned mask)
{
	Require((mask & ~NativeUIAttribute::All) == 0,"Native UI unsupported attribute mask");
	impl->Emit([mask](Impl& i) {
		i.OutsidePrimitive(); Require(i.state.attributes.size() < 256,"Native UI attribute overflow");
		Impl::State::Attribute saved{mask,i.state.draw,i.state.world,i.state.normal,i.state.textureBinding,i.state.textureEnabled,i.state.scissorEnabled,{},{}};
		if ((mask & Texture) && saved.textureBinding) {
			const auto* object=TextureObject(saved.textureBinding.get());
			saved.textureObject=saved.textureBinding;
			while (saved.textureObject.get()!=object) saved.textureObject=saved.textureObject->source;
			saved.textureParameters=object->sampler;
		}
		saved.textureBinding1=i.state.textureBinding1;saved.textureEnabled1=i.state.textureEnabled1;
		if((mask & Texture) && saved.textureBinding1){const auto* object=TextureObject(saved.textureBinding1.get());saved.textureObject1=saved.textureBinding1;while(saved.textureObject1.get()!=object)saved.textureObject1=saved.textureObject1->source;saved.textureParameters1=object->sampler;}
		saved.clear=i.clear;
		saved.textureObserved=i.state.textureObserved;saved.textureObserved1=i.state.textureObserved1;
		if(i.textureObserver&&i.compiling.empty()&&(mask&(Texture|Enable))){i.textureObserver->Push(mask&Texture,mask&(Texture|Enable));saved.textureHistoryPushed=true;}
		if((mask&Enable)&&i.compiling.empty()){
   const auto& clips=OriginalNativeModelContext().clips;
   if(clips.observed){saved.modelClipContext=clips.contextGeneration;saved.modelClipEnable=clips.enabled;saved.modelFogEnabled=OriginalNativeModelContext().fogEnabled;}
  }
		i.state.attributes.push_back(std::move(saved));
	});
}
void NativeUIRecorder::PopAttrib()
{
	impl->Emit([](Impl& i) {
		i.OutsidePrimitive(); Require(!i.state.attributes.empty(),"Native UI attribute underflow");
		const auto attribute=i.state.attributes.back();
		const auto& saved=attribute.draw; const auto mask=attribute.mask;
		if(attribute.textureHistoryPushed){Require(i.textureObserver!=nullptr,"Shared texture observer removed inside attribute scope");i.textureObserver->Pop();}
		if (mask & Texture) {i.RestoreTextureParameters(attribute.textureObject,attribute.textureParameters);i.RestoreTextureParameters(attribute.textureObject1,attribute.textureParameters1);}
		i.state.attributes.pop_back(); auto& s = i.state.draw;
		if (mask & Current) { s.color = saved.color; s.texCoord = saved.texCoord; i.state.normal=attribute.normal; }
		if (mask & Blend) {s.draw.blend=saved.draw.blend;i.clear.color=attribute.clear.color;}
		if (mask & Scissor) {s.draw.scissor=saved.draw.scissor;i.state.scissorEnabled=attribute.scissorEnabled;}
		if(mask & Viewport) s.draw.viewport=saved.draw.viewport;
		if (mask & Texture) { s.draw.texture = saved.draw.texture; s.draw.material = saved.draw.material; s.draw.sampler = saved.draw.sampler; s.draw.uvMode = saved.draw.uvMode; s.draw.minimapBackground=saved.draw.minimapBackground; i.state.textureBinding=attribute.textureBinding;i.state.textureEnabled=attribute.textureEnabled;s.draw.secondary=saved.draw.secondary;i.state.textureBinding1=attribute.textureBinding1;i.state.textureEnabled1=attribute.textureEnabled1;i.state.textureObserved=attribute.textureObserved;i.state.textureObserved1=attribute.textureObserved1; }
		if (mask & Alpha) s.draw.alphaTest = saved.draw.alphaTest;
		if(mask & Depth) {i.clear.depth=attribute.clear.depth;i.state.world.depthTest=attribute.world.depthTest;i.state.world.depthWrite=attribute.world.depthWrite;i.state.world.depthCompare=attribute.world.depthCompare;}
		if(mask & Polygon) {i.state.world.cull=attribute.world.cull;i.state.world.cullEnabled=attribute.world.cullEnabled;i.state.world.polygonOffset=attribute.world.polygonOffset;i.state.world.offsetFactor=attribute.world.offsetFactor;i.state.world.offsetUnits=attribute.world.offsetUnits;i.state.world.frontPolygonMode=attribute.world.frontPolygonMode;i.state.world.backPolygonMode=attribute.world.backPolygonMode;}
		if(mask & Stencil) {i.clear.stencil=attribute.clear.stencil;i.state.world.stencilTest=attribute.world.stencilTest;i.state.world.frontStencil=attribute.world.frontStencil;i.state.world.backStencil=attribute.world.backStencil;}
		if(mask & Line) {i.state.world.lineWidth=attribute.world.lineWidth;i.state.world.lineStipple=attribute.world.lineStipple;i.state.world.stippleFactor=attribute.world.stippleFactor;i.state.world.stipplePattern=attribute.world.stipplePattern;}
		if(mask & Point)i.state.world.pointSize=attribute.world.pointSize;
		if(mask & ColorMask){i.state.world.colorWrite=attribute.world.colorWrite;i.state.world.combinedColorWrite=attribute.world.combinedColorWrite;}
		if(mask & (Enable | Multisample))i.state.world.alphaToCoverage=attribute.world.alphaToCoverage;
		if (mask & Enable) {
   if(attribute.modelClipContext){
    auto& clips=OriginalNativeModelContext().clips;
    Require(clips.observed&&clips.contextGeneration==attribute.modelClipContext,"Original clip attribute scope crossed native contexts");
    clips.enabled=attribute.modelClipEnable;
    OriginalNativeModelContext().fogEnabled=attribute.modelFogEnabled;
   }
			s.draw.blend.enabled=saved.draw.blend.enabled;s.draw.alphaTest.enabled=saved.draw.alphaTest.enabled;
			i.state.scissorEnabled=attribute.scissorEnabled;i.state.textureEnabled=attribute.textureEnabled;i.state.textureEnabled1=attribute.textureEnabled1;
			i.state.world.depthTest=attribute.world.depthTest;i.state.world.depthClamp=attribute.world.depthClamp;i.state.world.cullEnabled=attribute.world.cullEnabled;i.state.world.polygonOffset=attribute.world.polygonOffset;i.state.world.stencilTest=attribute.world.stencilTest;i.state.world.lineStipple=attribute.world.lineStipple;
		}
	});
}
void NativeUIRecorder::AppendMiniMapIcons(std::vector<NativeMiniMapIcon> icons, bool secondAtlas)
{
	AppendEngineIcons(std::move(icons), secondAtlas, false);
}

void NativeUIRecorder::AppendScreenUnitIcons(std::vector<NativeMiniMapIcon> icons, bool secondAtlas)
{
	Require(impl->screen && !impl->worldStage, "Native screen unit icons require the original screen stage");
	AppendEngineIcons(std::move(icons), secondAtlas, true);
}

void NativeUIRecorder::AppendWorldUnitIcons(std::vector<NativeWorldUnitIcon> icons, bool secondAtlas, bool alphaToCoverage)
{
	Require(impl->worldStage && impl->worldStageId == NativeWorldDrawStage::World,
		"Native world icons require the original main World stage");
	impl->Emit([icons=std::move(icons),secondAtlas,alphaToCoverage](Impl& i) {
		i.OutsidePrimitive();
		Require(i.worldStage && i.worldStageId == NativeWorldDrawStage::World,
			"Native world icons replay outside their original World stage");
		// Match the original return before atlas binds and raster state changes.
		if (icons.empty()) return;
		auto surface=i.DrawState().draw;
		surface.material=ScreenMaterial::RGBA; surface.uvMode=ScreenUVMode::Normalized;
		surface.secondary={}; surface.minimapBackground.reset();
		surface.sampler={ScreenFilter::Linear,ScreenWrap::ClampEdge,ScreenWrap::ClampEdge,ScreenMipFilter::Linear};
		surface.alphaTest={true,ScreenAlphaCompare::Greater,0.05f};
		surface.blend.enabled=false;
		auto world=i.state.world;
		world.depthTest=false;
		world.alphaToCoverage=alphaToCoverage;
		constexpr std::array<unsigned,6> indices{3,0,1,3,1,2};
		for (const auto& icon:icons) {
			Require(bool(icon.atlas),"Native world unit icon has no real atlas image");
			i.CheckSample(icon.atlas); surface.texture=icon.atlas;
			const ScreenColor color{icon.color[0]/255.0f,icon.color[1]/255.0f,icon.color[2]/255.0f,icon.color[3]/255.0f};
			std::array<NativeWorldDrawVertex,6> geometry;
			for (unsigned n=0;n<indices.size();++n) {
				const auto index=indices[n];
				const auto& p=icon.positions[index];
				auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),{p[0],p[1],p[2],1}));
				if (i.worldZeroToOne) clip[2]=2*clip[2]-clip[3];
				geometry[n]={clip,color,{icon.uv[index][0],icon.uv[index][1],0,0}};
			}
			GetWorldDrawRecorder().Record(i.worldStageId,NativeDrawPrimitive::Triangles,geometry,surface,world);
		}
		// Original trailing atlas unbinds affect subsequent samplers, while the
		// recorded commands retain the exact immutable source atlas versions.
		if (i.textureObserver) {
			auto& history=OriginalNativeTextureContext().History();
			if (secondAtlas) {
				history.ActiveTexture(1); history.BindDefault(1,NativeCUSTextureTarget::Image2D);
				i.SyncObservedTexture(1);
			}
			history.ActiveTexture(0); history.BindDefault(0,NativeCUSTextureTarget::Image2D);
			i.SyncObservedTexture(0);
		}
	});
}

void NativeUIRecorder::AppendEngineIcons(std::vector<NativeMiniMapIcon> icons, bool secondAtlas, bool screenIcons)
{
	impl->Emit([icons=std::move(icons),secondAtlas,screenIcons](Impl& i) {
		i.OutsidePrimitive();
		Require(i.CanDraw() || !i.compiling.empty(),"Native minimap icons require an actual draw stage or list");
		// The original drawer returns before any texture/shader writes when empty.
		if (!i.CanDraw() || icons.empty()) return;
		auto surface=i.DrawState().draw;
		surface.material=ScreenMaterial::RGBA; surface.uvMode=ScreenUVMode::Normalized;
		surface.secondary={}; surface.minimapBackground.reset();
		surface.sampler={ScreenFilter::Linear,ScreenWrap::ClampEdge,ScreenWrap::ClampEdge,ScreenMipFilter::Linear};
		surface.alphaTest={true,ScreenAlphaCompare::Greater,screenIcons ? 0.05f : 0.0f};
		if (screenIcons) {
			// Original DrawUnitIconsScreen GL::SubState; preserve the equation.
			surface.depthStencil.depthTest=false;
			surface.blend.enabled=true;
			surface.blend.sourceColor=surface.blend.sourceAlpha=ScreenBlendFactor::SourceAlpha;
			surface.blend.destinationColor=surface.blend.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;
		}
		// Match AddQuadTriangles' original diagonal/order for SCREEN, while
		// leaving the already integrated minimap transport unchanged.
		const std::array<std::uint32_t,6> indices=screenIcons ?
			std::array<std::uint32_t,6>{3,0,1,3,1,2} : std::array<std::uint32_t,6>{0,1,2,0,2,3};
		for (const auto& icon:icons) {
			Require(bool(icon.atlas),"Native minimap icon has no real atlas image");
			i.CheckSample(icon.atlas); surface.texture=icon.atlas;
			const ScreenColor color{icon.color[0]/255.0f,icon.color[1]/255.0f,icon.color[2]/255.0f,icon.color[3]/255.0f};
			std::array<ScreenVertex,4> vertices;
			std::array<ScreenTexCoord,4> uv;
			std::vector<NativeWorldDrawVertex> geometry;
			const bool raster=i.worldStage || i.state.world.frontPolygonMode!=NativePolygonMode::Fill || i.state.world.backPolygonMode!=NativePolygonMode::Fill;
			if (raster) geometry.reserve(indices.size());
			for (unsigned n=0;n<4;++n) {
				uv[n]={icon.uv[n][0],icon.uv[n][1],0,0}; // shader bypasses the texture matrix
				if (!raster) {
					const auto p=i.Project(icon.positions[n][0],icon.positions[n][1],0,1);
					vertices[n]={p[0],p[1],p[2],color};
				}
			}
			if (raster) {
				for (const auto n:indices) {
					auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),{icon.positions[n][0],icon.positions[n][1],0,1}));
					if (i.worldStage && i.worldZeroToOne) clip[2]=2*clip[2]-clip[3];
					geometry.push_back({clip,color,uv[n]});
				}
				if (i.worldStage) GetWorldDrawRecorder().Record(i.worldStageId,NativeDrawPrimitive::Triangles,geometry,surface,i.state.world);
				else i.AppendScreenRaster(MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Triangles,geometry,surface,i.state.world));
			} else i.Output().AppendTriangles(vertices,uv,indices,surface);
		}
		// The closed engine draw retains its real atlas directly. Its original
		// trailing raw unbinds still affect later Lua samplers; they do not disable
		// texturing or restore the previously bound image. No reset workaround.
		if (i.textureObserver) {
			auto& history=OriginalNativeTextureContext().History();
			if (secondAtlas) {
				history.ActiveTexture(1); history.BindDefault(1,NativeCUSTextureTarget::Image2D);
				i.SyncObservedTexture(1);
			}
			history.ActiveTexture(0); history.BindDefault(0,NativeCUSTextureTarget::Image2D);
			i.SyncObservedTexture(0);
		}
	});
}

void NativeUIRecorder::AppendDeferredVisualizer(std::shared_ptr<const NativeDeferredVisualizerInput> input)
{
 impl->Emit([input=std::move(input)](Impl& i){
  i.OutsidePrimitive();Require(i.CanDraw()&&i.screen&&!i.worldStage&&i.targets.empty()&&!i.typedTargetDepth&&i.compiling.empty(),"Deferred visualizer requires original root DrawScreenPost");
  Require(input!=nullptr,"Deferred visualizer input missing");ValidateNativeDeferredVisualizer(*input);
  Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Deferred visualizer special raster requires general shader raster support");
  for(const auto& image:input->images)i.CheckSample(image.texture);
  auto state=i.DrawState().draw;state.material=ScreenMaterial::DeferredVisualizer;state.deferredVisualizer=input;state.texture.reset();state.secondary={};state.minimapBackground.reset();state.sampler={};state.uvMode=ScreenUVMode::Normalized;
  // Exact original gl.TexRect(0,-1,1,1,.5,0,1,1); its shader uses gl_Vertex
  // directly, independent of fixed matrices/current color. Keep four corners.
  const std::array<ScreenVertex,4> vertices{{{0,-1,0,{1,1,1,1}},{1,-1,0,{1,1,1,1}},{1,1,0,{1,1,1,1}},{0,1,0,{1,1,1,1}}}};
  const std::array<ScreenTexCoord,4> uv{{{.5f,0},{1,0},{1,1},{.5f,1}}};
  const std::array<uint32_t,6> indices{0,1,2,0,2,3};i.Output().AppendTriangles(vertices,uv,indices,state);
 });
}

void NativeUIRecorder::AppendTargetPriorityTriangles(std::vector<ScreenVertex> vertices,std::vector<ScreenTexCoord> uv,std::array<float,2> view)
{
	impl->Emit([vertices=std::move(vertices),uv=std::move(uv),view](Impl& i){
		i.OutsidePrimitive();Require(i.screen&&!i.worldStage&&i.targets.empty()&&!i.typedTargetDepth&&i.compiling.empty(),"TargetPriorityLines requires original root screen invocation");
		Require(vertices.size()==uv.size()&&vertices.size()%3==0&&std::isfinite(view[0])&&std::isfinite(view[1])&&view[0]>0&&view[1]>0,"TargetPriorityLines geometry/view extent differs");
		Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"TargetPriorityLines special polygon/clip state requires general raster implementation");
		auto state=i.DrawState().draw;state.material=ScreenMaterial::TargetPriorityStroke;state.targetPriorityViewGeometry=view;state.texture.reset();state.secondary={};state.minimapBackground.reset();state.sampler={};state.uvMode=ScreenUVMode::Normalized;
		// Original shader ignores fixed-function matrices and uses the current
		// engine UBO viewGeometry. Retain actual viewport/scissor/state separately.
		auto projected=vertices;std::vector<uint32_t> indices;indices.reserve(vertices.size());
		for(size_t index=0;index<projected.size();++index){auto& vertex=projected[index];vertex.z=0;indices.push_back(uint32_t(index));}
		i.Output().AppendTriangles(projected,uv,indices,state);
	});
}

void NativeUIRecorder::AppendProfilerHistogram(std::vector<std::array<float,2>> coords,std::array<float,2> view,std::array<float,4> box,ScreenColor color)
{
	impl->Emit([coords=std::move(coords),view,box,color](Impl& i){
		i.OutsidePrimitive();Require(i.CanDraw()&&i.screen&&!i.worldStage&&i.targets.empty()&&!i.typedTargetDepth&&i.compiling.empty(),"ProfilerHistograms requires original root screen invocation");
		Require(std::isfinite(view[0])&&std::isfinite(view[1])&&view[0]>0&&view[1]>0,"ProfilerHistograms actual view extent invalid");
		for(float value:box)Require(std::isfinite(value),"ProfilerHistograms bounding box nonfinite");
		for(float value:color)Require(std::isfinite(value),"ProfilerHistograms uniform color nonfinite");
		Require(!i.state.world.inheritedClipPlane,"ProfilerHistograms shader clip output requires its original contract");
		auto surface=i.DrawState().draw;surface.material=ScreenMaterial::Solid;surface.texture.reset();surface.secondary={};surface.minimapBackground.reset();surface.sampler={};surface.uvMode=ScreenUVMode::Normalized;
		std::vector<NativeWorldDrawVertex> vertices;vertices.reserve(coords.size());
		for(const auto& coord:coords){
			Require(std::isfinite(coord[0])&&std::isfinite(coord[1]),"ProfilerHistograms coordinate nonfinite");
			// The exact original VS is affine with w=1 and a uniform FS color.
			// Evaluate its clip coordinates before the common native line rasterizer.
			// In particular the original subtracts .5, NOT the usual 2*x-1.
			const float x=(coord[0]*(box[2]-box[0])+box[0])/view[0]-.5f;
			const float y=(coord[1]*(box[3]-box[1])+box[1])/view[1]-.5f;
			vertices.push_back({{x,y,.5f,1},color,{}});
		}
		// Original LINE_STRIP, current physical viewport, line width/stipple,
		// clipping, endpoint coverage and fixed depth/blend/mask state survive.
		// Fixed-function matrices and gl.Color are not inputs of this shader.
		i.AppendScreenRaster(MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::LineStrip,vertices,surface,i.state.world));
	});
}

void NativeUIRecorder::AppendTerraformCapture(std::shared_ptr<const NativeTerraformCaptureInput> input,
 std::vector<ScreenVertex> positions,std::vector<ScreenTexCoord> coords,std::shared_ptr<const WorldModel> model)
{
 impl->Emit([input=std::move(input),positions=std::move(positions),coords=std::move(coords),model=std::move(model)](Impl& i){
  i.OutsidePrimitive();Require(i.CanDraw()&&!i.worldStage&&!i.targets.empty()&&i.targets.back().editorTarget&&!i.typedTargetDepth&&i.compiling.empty()&&i.miniMapStages.empty(),"Terraform Capture requires original immediate editor RTT");
  Require(bool(input),"Terraform Capture immutable input missing");ValidateNativeTerraformCapture(*input);
  Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Terraform Capture requires original fill raster state");
  for(unsigned n=0;n<(input->program==NativeTerraformCaptureProgram::Reproject?2u:1u);++n)i.CheckSample(input->images[n].texture);
  auto state=i.DrawState().draw;state.material=ScreenMaterial::TerraformCapture;state.terraformCapture=input;
  state.minimapBackground.reset();state.deferredVisualizer.reset();state.decalCapture.reset();state.splatPainter.reset();state.diffusePainter.reset();
  state.texture=input->images[0].texture;state.sampler=input->images[0].sampler;state.uvMode=ScreenUVMode::Normalized;
  state.secondary=input->program==NativeTerraformCaptureProgram::Reproject?input->images[1]:ScreenSecondaryTexture{};
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;std::vector<std::uint32_t> indices;
  if(input->program==NativeTerraformCaptureProgram::Model){
   Require(model&&positions.empty()&&coords.empty(),"Terraform Capture needs actual static-definition geometry");
   // Engine preorder and bposeTransform are exactly DrawStatic, without live animation.
   for(const auto& piece:model->pieces){
    if(piece.indices.empty())continue;
    Require(piece.indices.size()%3==0&&piece.vertices.size()<=UINT32_MAX-vertices.size(),"Terraform Capture model triangle stream exceeds index range");
    const auto mv=Multiply(i.state.matrices[0].back(),piece.bindPose);
    const auto cross=[](std::array<float,3> a,std::array<float,3> b){return std::array<float,3>{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
    const std::array<float,3> a{mv[0],mv[1],mv[2]},b{mv[4],mv[5],mv[6]},c{mv[8],mv[9],mv[10]};
    const auto co0=cross(b,c),co1=cross(c,a),co2=cross(a,b);const float det=a[0]*co0[0]+a[1]*co0[1]+a[2]*co0[2];
    Require(std::isfinite(det)&&det!=0,"Terraform Capture singular modelview normal matrix");
    const auto base=std::uint32_t(vertices.size());
    for(const auto& v:piece.vertices){
     const auto clip=Transform(i.state.matrices[1].back(),Transform(mv,{v.position[0],v.position[1],v.position[2],1}));
     ScreenColor normal{0,0,0,1};for(unsigned r=0;r<3;++r)normal[r]=(co0[r]*v.normal[0]+co1[r]*v.normal[1]+co2[r]*v.normal[2])/det;
     for(float value:clip)Require(std::isfinite(value),"Terraform Capture nonfinite model clip coordinate");
     for(float value:normal)Require(std::isfinite(value),"Terraform Capture nonfinite eye-space normal");
     vertices.push_back({clip[0],clip[1],clip[2],normal});uv.push_back({v.uv[0][0],v.uv[0][1],clip[3],0});
    }
    for(auto index:piece.indices){Require(index<piece.vertices.size(),"Terraform Capture actual model index exceeds piece");indices.push_back(base+index);}
   }
  }else{
   Require(!model&&positions.size()==4&&coords.size()==4,"Terraform Capture original raw clip quad missing");
   // VERT_SRC writes gl_Vertex directly: caller matrices must not be applied.
   for(unsigned n=0;n<4;++n){const auto& v=positions[n];vertices.push_back({v.x,v.y,v.z,{1,1,1,1}});uv.push_back({coords[n][0],coords[n][1],1,0});}
   indices={0,1,2,0,2,3};
  }
  if(!indices.empty())i.Output().AppendTriangles(vertices,uv,indices,state);
 });
}

void NativeUIRecorder::AppendDiffusePainter(std::shared_ptr<const NativeDiffusePainterInput> input,
 std::vector<ScreenVertex> positions,std::vector<ScreenTexCoord> coords)
{
 impl->Emit([input=std::move(input),positions=std::move(positions),coords=std::move(coords)](Impl& i){
  i.OutsidePrimitive();Require(i.CanDraw()&&!i.worldStage&&!i.targets.empty()&&i.targets.back().editorTarget&&!i.typedTargetDepth&&i.compiling.empty()&&i.miniMapStages.empty(),"Diffuse Painter requires original immediate editor RTT");
  Require(input&&positions.size()==coords.size()&&positions.size()==6,"Diffuse Painter original quad triangles missing");ValidateNativeDiffusePainter(*input);
  Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Diffuse Painter requires original fill raster state");
  for(unsigned n=0;n<(input->program==NativeDiffusePainterProgram::Copy?1u:4u);++n)i.CheckSample(input->images[n].texture);
  auto state=i.DrawState().draw;state.material=ScreenMaterial::DiffusePainter;state.diffusePainter=input;
  state.minimapBackground.reset();state.deferredVisualizer.reset();state.decalCapture.reset();state.splatPainter.reset();
  state.texture=input->images[0].texture;state.sampler=input->images[0].sampler;state.uvMode=ScreenUVMode::Normalized;
  state.secondary=input->program==NativeDiffusePainterProgram::Copy?ScreenSecondaryTexture{}:input->images[1];
  std::vector<ScreenVertex> vertices;std::vector<ScreenTexCoord> uv;std::vector<std::uint32_t> indices;
  for(std::size_t n=0;n<positions.size();++n){const auto& v=positions[n];
   const auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),{v.x,v.y,v.z,1}));
   for(float value:clip)Require(std::isfinite(value),"Diffuse Painter nonfinite projected position");
   vertices.push_back({clip[0],clip[1],clip[2],{1,1,1,1}});uv.push_back({coords[n][0],coords[n][1],clip[3],0});indices.push_back(std::uint32_t(n));
  }
  // Original fixed MVP and raw MultiTexCoord0, with perspective w retained.
  i.Output().AppendTriangles(vertices,uv,indices,state);
 });
}

void NativeUIRecorder::AppendSplatPainter(std::shared_ptr<const NativeSplatPainterInput> input,
 std::vector<ScreenVertex> positions,std::vector<ScreenTexCoord> coords)
{
 impl->Emit([input=std::move(input),positions=std::move(positions),coords=std::move(coords)](Impl& i){
  i.OutsidePrimitive();Require(i.CanDraw()&&!i.typedTargetDepth&&i.compiling.empty()&&i.miniMapStages.empty(),"Splat Painter requires original immediate drawing");
  Require(input&&positions.size()==coords.size()&&!positions.empty()&&positions.size()%3==0,"Splat Painter original triangles missing");ValidateNativeSplatPainter(*input);
  const bool overlay=input->program==NativeSplatPainterProgram::Overlay;
  Require(overlay?(i.worldStage&&i.worldStageId==NativeWorldDrawStage::World&&i.targets.empty()):
   (!i.worldStage&&!i.targets.empty()&&i.targets.back().editorTarget),"Splat Painter original World overlay/editor RTT scope differs");
  Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Splat Painter requires original fill raster state");
  for(unsigned n=0;n<(input->program==NativeSplatPainterProgram::Paint?2u:1u);++n)i.CheckSample(input->images[n].texture);
  auto state=i.DrawState().draw;state.material=overlay?ScreenMaterial::RGBA:ScreenMaterial::SplatPainter;state.splatPainter=input;
  state.minimapBackground.reset();state.deferredVisualizer.reset();state.decalCapture.reset();
  state.texture=input->images[0].texture;state.sampler=input->images[0].sampler;state.uvMode=ScreenUVMode::Normalized;
  state.secondary=input->program==NativeSplatPainterProgram::Paint?input->images[1]:ScreenSecondaryTexture{};
  std::vector<NativeWorldDrawVertex> world;std::vector<ScreenVertex> vertices;
  std::vector<ScreenTexCoord> uv;std::vector<std::uint32_t> indices;
  for(std::size_t n=0;n<positions.size();++n){const auto& v=positions[n];
   auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),{v.x,v.y,v.z,1}));
   if(overlay&&i.worldZeroToOne)clip[2]=2*clip[2]-clip[3];
   for(float value:clip)Require(std::isfinite(value),"Splat Painter nonfinite projected position");
   // Original paired VS uses fixed MVP and raw MultiTexCoord0, without
   // current gl.Color or texture matrix. Preserve w before GPU clipping.
   if(overlay)world.push_back({clip,{1,1,1,1},{coords[n][0],coords[n][1],0,0}});
   else {vertices.push_back({clip[0],clip[1],clip[2],{1,1,1,1}});uv.push_back({coords[n][0],coords[n][1],clip[3],0});indices.push_back(std::uint32_t(n));}
  }
  if(overlay)GetWorldDrawRecorder().Record(i.worldStageId,NativeDrawPrimitive::Triangles,world,state,i.state.world);
  else i.Output().AppendTriangles(vertices,uv,indices,state);
 });
}

void NativeUIRecorder::AppendDecalCapture(std::shared_ptr<const NativeDecalCaptureInput> input,
 std::vector<ScreenVertex> vertices,std::vector<ScreenTexCoord> uv)
{
 impl->Emit([input=std::move(input),vertices=std::move(vertices),uv=std::move(uv)](Impl& i){
  i.OutsidePrimitive();Require(i.CanDraw()&&i.screen&&!i.worldStage&&!i.targets.empty()&&!i.typedTargetDepth&&i.compiling.empty(),"Decal Capture requires its actual screen RTT callback");
  Require(input&&vertices.size()==4&&uv.size()==4,"Decal Capture original quad is incomplete");ValidateNativeDecalCapture(*input);
  Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Decal Capture special raster state requires general implementation");
  for(const auto& image:input->images)i.CheckSample(image.texture);
  auto state=i.DrawState().draw;state.material=ScreenMaterial::DecalCapture;state.decalCapture=input;
  state.minimapBackground.reset();state.deferredVisualizer.reset();
  state.texture=input->images[0].texture;state.sampler=input->images[0].sampler;state.uvMode=ScreenUVMode::Normalized;state.secondary=input->images[1];
  const std::array<std::uint32_t,6> indices{0,1,2,0,2,3};
  i.Output().AppendTriangles(vertices,uv,indices,state);
 });
}

void NativeUIRecorder::AppendSpectatorHUDTriangles(std::vector<ScreenVertex> vertices,std::vector<ScreenTexCoord> clipW)
{
 impl->Emit([vertices=std::move(vertices),clipW=std::move(clipW)](Impl& i){
  i.OutsidePrimitive();Require(i.CanDraw()&&i.screen&&!i.worldStage&&i.targets.empty()&&!i.typedTargetDepth&&i.compiling.empty(),"Spectator HUD requires original root screen draw");
  Require(vertices.size()==clipW.size()&&vertices.size()%3==0,"Spectator HUD original indexed triangle extent differs");
  Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"Spectator HUD special raster state requires general implementation");
  auto state=i.DrawState().draw;state.material=ScreenMaterial::SpectatorHUD;state.texture.reset();state.secondary={};state.minimapBackground.reset();state.deferredVisualizer.reset();state.sampler={};state.uvMode=ScreenUVMode::Normalized;
  // Exact affine VS: all four components of pos + posBias. The fourth
  // component travels in unused UV storage, not an implicit w=1 shortcut.
  // Original shader does not read fixed-function matrices or gl.Color.
  std::vector<uint32_t> indices;indices.reserve(vertices.size());
  for(size_t n=0;n<vertices.size();++n)indices.push_back(uint32_t(n));
  i.Output().AppendTriangles(vertices,clipW,indices,state);
 });
}

void NativeUIRecorder::AppendFrameGrapherTriangles(std::vector<ScreenVertex> vertices,std::array<float,2> view,std::array<float,4> parameters)
{
	impl->Emit([vertices=std::move(vertices),view,parameters](Impl& i){
		i.OutsidePrimitive();Require(i.screen&&!i.worldStage&&i.targets.empty()&&!i.typedTargetDepth&&i.compiling.empty(),"FrameGrapher requires original root screen invocation");
		Require(vertices.size()%3==0&&std::isfinite(view[0])&&std::isfinite(view[1])&&view[0]>0&&view[1]>0,"FrameGrapher geometry/view extent differs");
		Require(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill&&!i.state.world.inheritedClipPlane,"FrameGrapher special polygon/clip state requires general raster implementation");
		auto state=i.DrawState().draw;state.material=ScreenMaterial::FrameGrapher;state.targetPriorityViewGeometry=view;state.frameGrapherParameters=parameters;state.texture.reset();state.secondary={};state.minimapBackground.reset();state.sampler={};state.uvMode=ScreenUVMode::Normalized;
		// Original shader ignores fixed-function matrices and uses the current
		// engine UBO viewGeometry. Retain actual viewport/scissor/state separately.
		for(float value:parameters)Require(std::isfinite(value),"FrameGrapher parameters must be finite");
		std::vector<ScreenTexCoord> uv(vertices.size());auto projected=vertices;std::vector<uint32_t> indices;indices.reserve(vertices.size());
		for(size_t index=0;index<projected.size();++index){auto& vertex=projected[index];vertex.z=0;indices.push_back(uint32_t(index));}
		i.Output().AppendTriangles(projected,uv,indices,state);
	});
}

void NativeUIRecorder::AppendFontTriangles(std::vector<ScreenVertex> vertices, std::vector<ScreenTexCoord> uv,
	std::vector<std::uint32_t> indices, std::shared_ptr<const ScreenTexture> atlas, bool callerBlending)
{
	impl->Emit([vertices=std::move(vertices),uv=std::move(uv),indices=std::move(indices),atlas=std::move(atlas),callerBlending](Impl& i) {
		i.OutsidePrimitive(); Require(atlas != nullptr,"Native UI missing real glyph atlas");
		Require(i.CanDraw() || !i.compiling.empty(),"Native UI font draw outside screen/list stage");
		if (!i.CanDraw()) return;
		auto state = i.DrawState().draw; state.secondary={};state.texture=atlas; state.uvMode=ScreenUVMode::Texel;
		state.minimapBackground.reset();
		state.material = atlas->format == ScreenTextureFormat::R8Unorm? ScreenMaterial::FontCoverage: ScreenMaterial::RGBA;
		state.sampler = {ScreenFilter::Linear,ScreenWrap::TransparentBorder,ScreenWrap::TransparentBorder}; state.alphaTest.enabled=false;
		if (!callerBlending) {const auto colorEquation=state.blend.colorEquation,alphaEquation=state.blend.alphaEquation;state.blend={};state.blend.colorEquation=colorEquation;state.blend.alphaEquation=alphaEquation;}
		state.blend.enabled = true;
		if(i.worldStage) {
			std::vector<NativeWorldDrawVertex> geometry;geometry.reserve(indices.size());
			Require(vertices.size()==uv.size(),"Native world font UV stream mismatch");
			for(auto index:indices) {
				Require(index<vertices.size(),"Native world font index out of range");const auto& v=vertices[index];
				auto clip=Transform(i.state.matrices[1].back(),Transform(i.state.matrices[0].back(),{v.x,v.y,v.z,1}));
				if(i.worldZeroToOne)clip[2]=2*clip[2]-clip[3];
				geometry.push_back({clip,v.color,uv[index]});
			}
			auto world=i.state.world;
			// Original CglShaderFontRenderer::PushGLState disables depth testing
			// independently of the surrounding world widget and preserves it on Pop.
			world.depthTest=false;
			GetWorldDrawRecorder().Record(i.worldStageId,NativeDrawPrimitive::Triangles,geometry,state,world);
		} else {
			auto projected=vertices;
			for(auto& v:projected){const auto p=i.Project(v.x,v.y,v.z,1);v.x=p[0];v.y=p[1];v.z=p[2];}
			if(i.state.world.frontPolygonMode==NativePolygonMode::Fill&&i.state.world.backPolygonMode==NativePolygonMode::Fill)i.Output().AppendTriangles(projected,uv,indices,state);
			else {
				std::vector<NativeWorldDrawVertex> geometry;geometry.reserve(indices.size());
				Require(vertices.size()==uv.size(),"Native font UV stream mismatch");
				for(auto index:indices){Require(index<projected.size(),"Native font index out of range");const auto& v=projected[index];geometry.push_back({{v.x*2/float(i.width)-1,v.y*2/float(i.height)-1,v.z,1},v.color,uv[index]});}
				i.AppendScreenRaster(MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Triangles,geometry,state,i.state.world));
			}
		}
	});
}

} // namespace rendering::ui
