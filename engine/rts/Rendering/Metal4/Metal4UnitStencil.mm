/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4UnitStencil.h"
#include "NativeUnitStencilShaders.h"
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeCUSRenderInputs.h"
#import <simd/simd.h>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
namespace rendering::metal4 {
namespace {
struct Uniforms {float addRadius=10,stencilColor=1;uint32_t firstRecord=0,clipZeroToOne=0;simd_float4 alpha{};};
static_assert(sizeof(Uniforms)==32);
struct Image final:NativeUnitStencilResource {
	std::weak_ptr<const NativeUnitStencilVersion> owner;
	std::shared_ptr<NativeUnitStencilProducer> producer;
	__strong id<MTLTexture> texture=nil;
	__strong id<MTLSamplerState> sampler=nil;
};
struct Linked {
	__strong id<MTLLibrary> library=nil;
	mutable std::map<std::vector<uint64_t>,id<MTLRenderPipelineState>> pipelines;
};
id<MTLBuffer> Upload(RuntimeLease& lease,const void* source,size_t bytes){
	Require(bytes&&bytes<=lease.Device().maxBufferLength,"Unit Stencil actual immutable upload exceeds device extent");auto b=[lease.Device()newBufferWithBytes:source length:bytes options:MTLResourceStorageModeShared];Require(b!=nil,"Cannot allocate Unit Stencil actual buffer");lease.AddAllocation(b);return b;
}
MTLBlendOperation Equation(ScreenBlendEquation e){switch(e){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Unit Stencil unknown original blend equation");}
id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const Linked& linked,const ScreenDrawState& surface,const NativeWorldDrawState& world,bool clear,bool validation,double& ms){
	const auto& b=surface.blend;std::vector<uint64_t> key{uint64_t(clear),uint64_t(validation),uint64_t(world.colorWrite[0]),uint64_t(b.enabled),uint64_t(b.sourceColor),uint64_t(b.destinationColor),uint64_t(b.sourceAlpha),uint64_t(b.destinationAlpha),uint64_t(b.colorEquation),uint64_t(b.alphaEquation)};
	if(auto i=linked.pipelines.find(key);i!=linked.pipelines.end())return i->second;const auto start=Clock::now();
	auto d=[MTL4RenderPipelineDescriptor new];auto v=[MTL4LibraryFunctionDescriptor new],f=[MTL4LibraryFunctionDescriptor new];v.library=f.library=linked.library;
	v.name=clear?@"UnitStencilClearVertex":@"UnitStencilVertex";f.name=clear?@"UnitStencilClearFragment":@"UnitStencilFragment";d.vertexFunctionDescriptor=v;d.fragmentFunctionDescriptor=f;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
	auto a=d.colorAttachments[0];a.pixelFormat=MTLPixelFormatR8Unorm;a.writeMask=world.colorWrite[0]?MTLColorWriteMaskRed:MTLColorWriteMask(0);
	a.blendingState=!clear&&b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
	a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);
	a.rgbBlendOperation=Equation(b.colorEquation);a.alphaBlendOperation=Equation(b.alphaEquation);
	auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;
	NSError* error=nil;auto p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile exact original Unit Stencil R8 target pipeline: "+Error(error));linked.pipelines.emplace(std::move(key),p);ms+=Milliseconds(start);return p;
}
bool Readable(const Image& image,uint64_t submission){const auto& p=*image.producer;return(p.allocationOnly&&p.allocationVerified)||p.completed||(submission&&p.encoded&&p.submissionIdentity==submission);}
}
struct PreparedUnitStencil::Impl {
	struct Command {
		bool clear=false,skip=false;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTL4ArgumentTable> arguments=nil;
		NSUInteger instances=0;
		NativeWorldDrawState world;
		effectstate::Raster raster;
	};
	struct Pass {
		uint64_t sequence=0,submission=0;
		std::shared_ptr<Image> output,previous;
		std::vector<Command> commands;
		bool copyPrevious=false;
		mutable bool encoded=false;
	};
	std::shared_ptr<const NativeUnitStencilFrame> source;
	std::vector<Pass> passes;
	mutable bool completed=false;
};
PreparedUnitStencil::PreparedUnitStencil():impl(std::make_unique<Impl>()){}
PreparedUnitStencil::~PreparedUnitStencil()=default;
size_t PreparedUnitStencil::PassCount()const{return impl->passes.size();}
uint64_t PreparedUnitStencil::SequenceAt(size_t i)const{return impl->passes.at(i).sequence;}
void PreparedUnitStencil::EncodePass(size_t index,id<MTL4CommandBuffer> command)const{
	auto& pass=impl->passes.at(index);Require(command&&!pass.encoded&&!impl->completed,"Unit Stencil original Genesis producer encoded twice or command absent");
	if(pass.copyPrevious){
		Require(pass.previous&&Readable(*pass.previous,pass.submission),"Unit Stencil partial clear lost previous original generation");
		auto copy=[command computeCommandEncoder];Require(copy!=nil,"Cannot preserve Unit Stencil previous R8 contents");
		[copy barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
		[copy copyFromTexture:pass.previous->texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(pass.output->texture.width,pass.output->texture.height,1) toTexture:pass.output->texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];
		[copy barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[copy endEncoding];
	}
	auto descriptor=[MTL4RenderPassDescriptor new];auto color=descriptor.colorAttachments[0];color.texture=pass.output->texture;color.loadAction=pass.copyPrevious?MTLLoadActionLoad:MTLLoadActionDontCare;color.storeAction=MTLStoreActionStore;
	auto encoder=[command renderCommandEncoderWithDescriptor:descriptor];Require(encoder!=nil,"Cannot begin Unit Stencil real quarterR8 render pass");
	[encoder barrierAfterQueueStages:MTLStageFragment|MTLStageBlit beforeStages:MTLStageVertex|MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];
	for(const auto& draw:pass.commands){
		if(draw.skip)continue;
		effectstate::RasterState(encoder,draw.world,draw.raster);
		[encoder setDepthClipMode:draw.clear||!draw.world.depthClamp?MTLDepthClipModeClip:MTLDepthClipModeClamp];
		[encoder setCullMode:draw.clear||!draw.world.cullEnabled?MTLCullModeNone:draw.world.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
		[encoder setTriangleFillMode:!draw.clear&&draw.world.frontPolygonMode==NativePolygonMode::Line?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
		[encoder setRenderPipelineState:draw.pipeline];[encoder setArgumentTable:draw.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.clear?3:18 instanceCount:draw.instances];
	}
	[encoder endEncoding];pass.output->producer->submissionIdentity=pass.submission;pass.output->producer->encoded=true;pass.encoded=true;
}
void PreparedUnitStencil::DidComplete()const{
	Require(!impl->completed,"Unit Stencil completed twice");for(const auto& pass:impl->passes)Require(pass.encoded&&pass.output->producer->encoded,"Unit Stencil cannot retire an unencoded original producer");
	for(const auto& pass:impl->passes)pass.output->producer->completed=true;impl->source->completed=true;impl->completed=true;
}
struct UnitStencilGPU::Impl {
	std::map<std::pair<uint64_t,uint64_t>,std::shared_ptr<Image>> images;
	std::map<const void*,std::weak_ptr<const Linked>> programs;
	std::shared_ptr<Linked> clearProgram;
	std::shared_ptr<Image> ImageFor(RuntimeLease& lease,const std::shared_ptr<const NativeUnitStencilVersion>& v){
		Require(v&&v->image&&v->image->width&&v->image->height,"Unit Stencil image lacks its original actual extent/version");
		if(v->Resource()){auto resource=std::dynamic_pointer_cast<const Image>(v->Resource());Require(resource&&resource->owner.lock()==v&&resource->texture.device==lease.Device(),"Unit Stencil immutable image has another actual version/device");return std::const_pointer_cast<Image>(resource);}
		const auto& source=*v->image;Require(source.width<=lease.Capabilities().maxTextureDimension2D&&source.height<=lease.Capabilities().maxTextureDimension2D,"Unit Stencil actual quarter image exceeds device limits");
		auto descriptor=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm width:source.width height:source.height mipmapped:NO];descriptor.storageMode=MTLStorageModePrivate;descriptor.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;
		auto image=std::make_shared<Image>();image->owner=v;image->producer=v->producer;image->texture=[lease.Device()newTextureWithDescriptor:descriptor];Require(image->texture!=nil,"Cannot allocate actual Unit Stencil quarterR8 image");
		auto sampler=[MTLSamplerDescriptor new];sampler.supportArgumentBuffers=YES;sampler.minFilter=sampler.magFilter=MTLSamplerMinMagFilterNearest;sampler.mipFilter=MTLSamplerMipFilterNotMipmapped;sampler.sAddressMode=sampler.tAddressMode=MTLSamplerAddressModeClampToEdge;
		image->sampler=[lease.Device()newSamplerStateWithDescriptor:sampler];Require(image->sampler!=nil,"Cannot allocate Unit Stencil original nearest/clamp sampler");
		v->Fulfill(image);v->producer->allocationVerified=true;images.emplace(std::pair{source.identity,v->generation},image);return image;
	}
};
UnitStencilGPU::UnitStencilGPU():impl(std::make_unique<Impl>()){}UnitStencilGPU::~UnitStencilGPU()=default;
void UnitStencilGPU::ClearResources(){impl->images.clear();impl->programs.clear();impl->clearProgram.reset();}
std::shared_ptr<const NativeUnitStencilProgramConsumer> UnitStencilGPU::LinkOriginalProgram(RuntimeLease& lease,const NativeUnitStencilProgramSource& source,bool validation,double& ms){
	ValidateNativeUnitStencilProgram(source);const auto start=Clock::now();auto linked=std::make_shared<Linked>();auto descriptor=[MTL4LibraryDescriptor new];descriptor.name=@"Original31657 Unit Stencil GS-equivalent threefaces";descriptor.source=String(unitStencil::SHADER);
	auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;NSError* error=nil;
	linked->library=lease.NewLibrary(descriptor,&error);Require(linked->library!=nil,"Cannot compile original Unit Stencil native stages: "+Error(error));
	ms+=Milliseconds(start);ScreenDrawState surface;NativeWorldDrawState world;world.colorWrite={true,true,true,true};Pipeline(lease,*linked,surface,world,false,validation,ms);Pipeline(lease,*linked,surface,world,true,validation,ms);
	auto result=std::make_shared<NativeUnitStencilProgramConsumer>();result->pipelineLifetime=linked;result->vertexSHA512=source.vertexSHA512;result->geometrySHA512=source.geometrySHA512;result->fragmentSHA512=source.fragmentSHA512;
	result->uniforms={{"addRadius",0x1406,1,false},{"stencilColor",0x1406,1,false}};result->defaultFloat={{"addRadius",{10}},{"stencilColor",{1}}};impl->programs[linked.get()]=linked;impl->clearProgram=linked;return result;
}
CUSSourceImage UnitStencilGPU::Resolve(RuntimeLease& lease,std::shared_ptr<const NativeUnitStencilVersion> version,uint64_t submission){
	auto image=impl->ImageFor(lease,version);lease.AddAllocation(image->texture);lease.Retain(image->sampler);CUSSourceImage result;result.texture=image->texture;result.sampler=image->sampler;result.owner=image;result.flipV=true;
	result.requireReadable=[image,submission]{Require(Readable(*image,submission),"Unit Stencil R8 sampled before its actual Genesis producer encoded/retired");};return result;
}
std::unique_ptr<PreparedUnitStencil> UnitStencilGPU::Prepare(RuntimeLease& lease,std::shared_ptr<const NativeUnitStencilFrame> source,bool validation,double& ms,uint64_t submission){
	Require(source&&submission,"Unit Stencil actual Genesis frame/submission missing");std::erase_if(impl->images,[](const auto& i){return i.second->owner.expired();});std::erase_if(impl->programs,[](const auto& i){return i.second.expired();});
	auto result=std::unique_ptr<PreparedUnitStencil>(new PreparedUnitStencil());result->impl->source=std::move(source);
	for(const auto& sourcePass:result->impl->source->passes){
		Require(sourcePass.output&&sourcePass.previous&&!sourcePass.commands.empty()&&sourcePass.commands.front().clear&&impl->clearProgram,
			"Unit Stencil actual requested pass lacks clear/original linked producer");
		PreparedUnitStencil::Impl::Pass pass;pass.sequence=sourcePass.sequence;pass.submission=submission;pass.output=impl->ImageFor(lease,sourcePass.output);
		const unsigned width=sourcePass.output->image->width,height=sourcePass.output->image->height;lease.AddAllocation(pass.output->texture);lease.Retain(pass.output->sampler);
		const auto& first=sourcePass.commands.front();bool fullClear=first.world.colorWrite[0];if(first.surface.scissor){const auto& sc=*first.surface.scissor;fullClear=fullClear&&sc.x<=0&&sc.y<=0&&int64_t(sc.x)+sc.width>=width&&int64_t(sc.y)+sc.height>=height;}
		pass.copyPrevious=!fullClear;
		if(pass.copyPrevious){pass.previous=impl->ImageFor(lease,sourcePass.previous);lease.AddAllocation(pass.previous->texture);}
		for(const auto& command:sourcePass.commands){
			PreparedUnitStencil::Impl::Command next;next.clear=command.clear;
			Require(command.clear||command.draw.has_value(),"Unit Stencil command has no original draw");
			const auto& surface=command.clear?command.surface:command.draw->surface;
			next.world=command.clear?command.world:command.draw->world;
			WorldCamera camera;camera.viewport={0,0,int32_t(width),int32_t(height)};
			if(!command.clear&&surface.viewport){const auto& view=*surface.viewport;camera.viewport={view.x,view.y,int32_t(view.width),int32_t(view.height)};}
			auto raster=effectstate::Viewport(camera,width,height,width,height,surface.scissor);
			next.skip=!raster||(!command.clear&&next.world.cullEnabled&&next.world.cull==NativeWorldCull::FrontAndBack);
			if(next.skip){pass.commands.push_back(std::move(next));continue;}next.raster=*raster;
			auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=4;descriptor.initializeBindings=YES;NSError* error=nil;
			next.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(next.arguments!=nil,"Cannot allocate Unit Stencil genuine source arguments: "+Error(error));lease.Retain(next.arguments);
			if(command.clear){
				auto clear=Upload(lease,command.clearColor.data(),16);[next.arguments setAddress:clear.gpuAddress atIndex:3];
				next.instances=1;next.pipeline=Pipeline(lease,*impl->clearProgram,surface,next.world,true,validation,ms);
			}else{
				Require(command.draw.has_value(),"Unit Stencil pass command lacks its original captured draw");const auto& draw=*command.draw;ValidateNativeUnitStencilDraw(draw);
				auto linked=impl->programs.find(draw.program->pipelineLifetime.get());Require(linked!=impl->programs.end(),"Unit Stencil source program belongs to another native lifetime");auto program=linked->second.lock();Require(bool(program),"Unit Stencil actual compiled owner expired");
				Require(next.world.frontPolygonMode==next.world.backPolygonMode&&next.world.frontPolygonMode!=NativePolygonMode::Point,"Unit Stencil exact original polygon mode lacks point-mode expansion");
				auto records=Upload(lease,draw.records->bytes.data(),draw.records->bytes.size());auto matrices=Upload(lease,draw.engineUniforms->matrices->bytes.data(),draw.engineUniforms->matrices->bytes.size());
				const auto& published=*draw.modelUniforms;Require(published.capacityElements<=SIZE_MAX/published.stride,"Unit Stencil original model uniform capacity overflow");
				const size_t bytes=published.capacityElements*published.stride;Require(bytes&&bytes<=lease.Device().maxBufferLength,"Unit Stencil original model uniform buffer exceeds device extent");
				auto objects=[lease.Device()newBufferWithLength:bytes options:MTLResourceStorageModeShared];Require(objects!=nil,"Cannot allocate Unit Stencil original published object uniforms");lease.AddAllocation(objects);
				for(const auto& range:NativeCUSObservedRanges(published))for(size_t offset=range.offset;offset<range.offset+range.size;){
					const size_t page=offset/NativeCUSPublishedPage::Bytes,local=offset%NativeCUSPublishedPage::Bytes,n=std::min(range.offset+range.size-offset,NativeCUSPublishedPage::Bytes-local);
					std::memcpy(static_cast<uint8_t*>(objects.contents)+offset,published.pages.at(page)->bytes.data()+local,n);offset+=n;
				}
				Uniforms u;u.addRadius=draw.addRadius;u.stencilColor=draw.stencilColor;u.firstRecord=draw.first;uint32_t caps=0;std::memcpy(&caps,draw.engineUniforms->params->bytes.data()+12,4);u.clipZeroToOne=caps&1u;
				u.alpha={float(surface.alphaTest.enabled),surface.alphaTest.reference,float(surface.alphaTest.compare),0};auto uniforms=Upload(lease,&u,sizeof(u));
				[next.arguments setAddress:records.gpuAddress atIndex:0];[next.arguments setAddress:matrices.gpuAddress atIndex:1];[next.arguments setAddress:objects.gpuAddress atIndex:2];[next.arguments setAddress:uniforms.gpuAddress atIndex:3];
				next.instances=draw.count;next.pipeline=Pipeline(lease,*program,surface,next.world,false,validation,ms);lease.Retain(program->library);
			}
			lease.Retain(next.pipeline);pass.commands.push_back(std::move(next));
		}
		result->impl->passes.push_back(std::move(pass));
	}
	return result;
}
} // namespace rendering::metal4
