/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldEnvironment.h"
#include "Metal4WorldEnvironmentShader.h"
#include "Metal4CubeTextureSource.h"
#include "Metal4SkyCubeDecode.h"
#include "Metal4NativeEffectState.h"
#include "Rendering/Backend/NativeWaterGroundSourceData.h"
#import <simd/simd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace rendering::metal4 {
namespace {
struct SkyUniform {
	simd_float4x4 vp, rotation;
	simd_float4 cameraPosition, midMap, skyColor, sunColor, cloudColor, planeColor, sun;
	simd_float4 parameters;
	simd_uint4 flags;
};
simd_float4x4 Matrix(const WorldMatrix& value) { simd_float4x4 out;std::memcpy(&out,value.data(),sizeof(out));return out; }
simd_float4 Vector(const std::array<float,4>& v) { return {v[0],v[1],v[2],v[3]}; }
uint32_t Levels(uint32_t size) { uint32_t count=1;while(size>1){size/=2;++count;}return count; }
id<MTLTexture> Cube(RuntimeLease& lease,MTLPixelFormat format,uint32_t size,uint32_t levels,MTLStorageMode storage,MTLTextureUsage usage,NSString* label)
{
	Require(size>0&&size<=lease.Capabilities().maxTextureDimension2D&&levels>0&&levels<=Levels(size),"Original environment cube extent exceeds actual device limits");
	auto* d=[MTLTextureDescriptor new];d.textureType=MTLTextureTypeCube;d.width=d.height=size;d.depth=1;d.arrayLength=1;d.mipmapLevelCount=levels;d.pixelFormat=format;d.storageMode=storage;d.usage=usage;
	id<MTLTexture> texture=[lease.Device() newTextureWithDescriptor:d];Require(texture!=nil,"Cannot allocate original environment cube");texture.label=label;return texture;
}
id<MTLBuffer> Uniform(RuntimeLease& lease,const SkyUniform& value)
{
	id<MTLBuffer> out=[lease.Device() newBufferWithBytes:&value length:sizeof(value) options:MTLResourceStorageModeShared];Require(out!=nil,"Cannot allocate original sky uniforms");lease.AddAllocation(out);return out;
}
SkyUniform SkyState(const NativeWorldEnvironmentFrame& f,const WorldCamera& input,bool reflection)
{
	SkyUniform out{};auto view=Matrix(input.view);const bool modern=f.sky->kind==NativeSkyKind::Modern;
	if(!modern)view.columns[3]={0,0,0,1};out.vp=simd_mul(Matrix(input.projection),view);
	out.rotation=Matrix(f.skyRotation);
	const bool debugFace=reflection&&f.reflectionGround&&bool(f.reflectionGround->debugCubeSky);
	if(debugFace)out.rotation=matrix_identity_float4x4; // Debug.Draw has no CSkyBox axis rotation.
	out.cameraPosition={input.position[0],input.position[1],input.position[2],0};
	out.midMap={float(f.terrain.squaresX)*f.terrain.squareSize*.5f,0,float(f.terrain.squaresZ)*f.terrain.squareSize*.5f,0};
	out.skyColor=Vector(f.skyColor);out.sunColor=Vector(f.sunColor);out.cloudColor=Vector(f.cloudColor);out.planeColor=Vector(f.waterPlaneColor);out.sun=Vector(reflection && f.sourceCubeUpdate ? f.reflectionLighting.sunDirection : f.lighting.sunDirection);
	if(debugFace)out.planeColor={0,0,0,0}; // Debug shader never sets planeColor.
	if(reflection&&f.sourceCubeUpdate)out.sunColor.w=f.reflectionLighting.sunDirection[3];
	out.parameters.x=f.animationTime;out.flags={modern?2u:0u,uint32_t(reflection),uint32_t(f.sky->mipmappedSampling),0};
	for(const auto& v:{out.skyColor,out.sunColor,out.cloudColor,out.planeColor,out.sun,out.parameters})for(size_t i=0;i<4;++i)Require(std::isfinite(v[i]),"Non-finite original sky input");
	return out;
}
}

struct WorldEnvironmentGPU::Impl {
	std::shared_ptr<const NativeWorldSkyResource> skyOwner;
	std::shared_ptr<const NativeWorldSpecularResource> specularOwner;
	__strong id<MTLTexture> sky=nil,reflection=nil,skyReflection=nil,specular=nil,brdf=nil,depth=nil,equirect=nil,skyArray=nil;
	__strong id<MTLLibrary> library=nil;
	__strong id<MTLComputePipelineState> brdfPipeline=nil,equirectPipeline=nil;
	__strong id<MTLRenderPipelineState> mainSky=nil,reflectionSky=nil;
	__strong id<MTLDepthStencilState> skyDepth=nil;
	bool shaderValidation=false,compiled=false,brdfReady=false,reflectionReady=false;
	bool reflectionMipmaps=false,skyOnly=false,skyReady=false;
	uint32_t reflectionSize=0,cursor=0,initializedFaces=0;
	MTLPixelFormat mainFormat=MTLPixelFormatInvalid;
	NativeSkyKind pipelineSkyKind=NativeSkyKind::OriginalNull;
	bool pipelineHasCube=false;unsigned mainSamples=1;bool sampleShading=false;

	void Pipelines(RuntimeLease& lease,MTLPixelFormat format,bool validation,double& milliseconds,unsigned samples)
	{
		Require(skyOwner!=nullptr,"Original sky pipeline has no captured selection");
		const auto skyKind=skyOwner->kind;
		const bool hasCube=sky!=nil;
		Require(hasCube==(skyKind==NativeSkyKind::CubeMap||skyKind==NativeSkyKind::Equirectangular),
			"Original sky selection and actual cube presence differ");
		if(compiled&&mainFormat==format&&shaderValidation==validation&&mainSamples==samples&&sampleShading==lease.SampleShadingEnabled(samples)&&pipelineSkyKind==skyKind&&pipelineHasCube==hasCube)return;
		auto start=Clock::now();NSError* error=nil;
		auto* ld=[MTL4LibraryDescriptor new];ld.source=String(WORLD_ENVIRONMENT_SHADER);ld.name=@"Original engine sky and exact30770 BRDF producer";
		library=lease.NewLibrary(ld,&error);Require(library!=nil,"Cannot compile native original environment MSL: "+Error(error));
		auto function=[&](NSString* name){auto* f=[MTL4LibraryFunctionDescriptor new];f.library=library;f.name=name;return f;};
		auto* options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);
		auto* cd=[MTL4ComputePipelineDescriptor new];cd.computeFunctionDescriptor=function(@"OriginalUnitBRDF");cd.options=options;
		brdfPipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:cd compilerTaskOptions:nil error:&error];Require(brdfPipeline!=nil,"Cannot compile exact original unit BRDF producer: "+Error(error));
		cd.computeFunctionDescriptor=function(@"OriginalEquiRectToCube");equirectPipeline=[lease.Compiler() newComputePipelineStateWithDescriptor:cd compilerTaskOptions:nil error:&error];Require(equirectPipeline!=nil,"Cannot compile original equirectangular sky conversion: "+Error(error));
		auto pipeline=[&](MTLPixelFormat pf,unsigned outputSamples){auto* d=[MTL4RenderPipelineDescriptor new];d.rasterSampleCount=outputSamples;d.vertexFunctionDescriptor=function(@"OriginalSkyVertex");
			auto constants=[MTLFunctionConstantValues new];
			[constants setConstantValue:&hasCube type:MTLDataTypeBool atIndex:0];
			auto specialized=[MTL4SpecializedFunctionDescriptor new];
			specialized.functionDescriptor=function(@"OriginalSkyFragment");specialized.constantValues=constants;
			d.fragmentFunctionDescriptor=specialized;
			d.colorAttachments[0].pixelFormat=pf;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.options=options;
			// ModernSky enables these original factors; cube sky has alpha 1 but
			// uses the separate opaque PSO below to preserve original DDS alpha.
			if(skyKind==NativeSkyKind::Modern){auto b=d.colorAttachments[0];b.blendingState=MTL4BlendStateEnabled;b.sourceRGBBlendFactor=b.sourceAlphaBlendFactor=MTLBlendFactorSourceAlpha;b.destinationRGBBlendFactor=b.destinationAlphaBlendFactor=MTLBlendFactorOneMinusSourceAlpha;}
			id<MTLRenderPipelineState> p=lease.NewRenderPipeline(d,&error);Require(p!=nil,"Cannot compile original sky pipeline: "+Error(error));return p;};
		mainSky=pipeline(format,samples);reflectionSky=pipeline(MTLPixelFormatRGBA8Unorm,1);
		auto* dd=[MTLDepthStencilDescriptor new];dd.depthCompareFunction=MTLCompareFunctionLessEqual;dd.depthWriteEnabled=YES;skyDepth=[lease.Device() newDepthStencilStateWithDescriptor:dd];Require(skyDepth!=nil,"Cannot allocate original sky depth state");
		mainFormat=format;shaderValidation=validation;mainSamples=samples;sampleShading=lease.SampleShadingEnabled(samples);pipelineSkyKind=skyKind;pipelineHasCube=hasCube;compiled=true;milliseconds+=Milliseconds(start);
	}
};
struct PreparedWorldEnvironment::Impl {
	WorldEnvironmentGPU::Impl* cache=nullptr;
	std::shared_ptr<const NativeWorldEnvironmentFrame> frame;
	__strong id<MTLBuffer> mainUniform=nil,reflectionUniform=nil;
	std::vector<id<MTLBuffer>> waterUniforms;
	__strong id<MTL4ArgumentTable> arguments=nil;
	__strong MTL4RenderPassDescriptor* pass=nil;
	__strong id<MTLDepthStencilState> debugDepth=nil;
	uint32_t face=0;bool skyOnly=false,wrap=false,willBeReady=false,dependenciesEncoded=false,reflectionFinished=false;
	std::shared_ptr<PreparedCubeTexture> sourceReflection, sourceSky, sourceSpecular, sourceFace, sourceMips;
};
WorldEnvironmentGPU::WorldEnvironmentGPU():impl(std::make_unique<Impl>()){}
WorldEnvironmentGPU::~WorldEnvironmentGPU()=default;
PreparedWorldEnvironment::PreparedWorldEnvironment(std::unique_ptr<Impl> value):impl(std::move(value)){}
PreparedWorldEnvironment::~PreparedWorldEnvironment()=default;
void WorldEnvironmentGPU::ClearResources()
{
	impl->skyOwner.reset();impl->specularOwner.reset();impl->sky=nil;impl->reflection=nil;impl->skyReflection=nil;impl->specular=nil;impl->brdf=nil;impl->depth=nil;impl->equirect=nil;impl->skyArray=nil;impl->skyReady=false;
	impl->brdfReady=impl->reflectionReady=false;impl->cursor=impl->initializedFaces=0;impl->compiled=false;
}
std::unique_ptr<PreparedWorldEnvironment> WorldEnvironmentGPU::Prepare(RuntimeLease& lease,const NativeWorldEnvironmentFrame& input,const WorldCamera& mainCamera,MTLPixelFormat mainFormat,bool validation,double& shaderMilliseconds,uint64_t sourceSubmission,unsigned samples)
{
	Require(input.sky&&(input.specular||input.sourceSpecular),"Original environment capture requires real sky/specular resource owners");
	const bool originalSource = bool(input.sourceCubeUpdate);
	Require(!originalSource || (input.sourceReflection && input.sourceSpecular && sourceSubmission), "Original named environment requires source objects and actual submission identity");
	Require(input.unitBRDFSize==512&&input.unitBRDFGOption==1,"Unsupported original unit BRDF producer definition");
	Require(input.unsupported.empty(),input.unsupported.empty()?"":"Unsupported original environment mode: "+input.unsupported.front());
	if(impl->skyOwner!=input.sky){impl->skyOwner=input.sky;impl->sky=nil;impl->equirect=nil;impl->skyArray=nil;impl->skyReady=false;impl->reflectionReady=false;impl->initializedFaces=impl->cursor=0;impl->compiled=false;}
	if(impl->sky==nil){
		if(input.sky->kind==NativeSkyKind::CubeMap){
			const auto& faces=input.sky->cubeFaces;Require(!faces[0].empty(),"Original sky cube has no owned face mip data");
			const auto format=input.sky->format;const size_t block=(format==WorldTextureFormat::BC1RGBA||format==WorldTextureFormat::BC4Unorm)?8:16;
			const bool raw=format==WorldTextureFormat::RGBA8Unorm;
			Require(format!=WorldTextureFormat::BGRA8Unorm,"Original sky BGRA must be expanded by its captured source");
			const uint32_t faceSize=faces[0][0].width;
			Require(!input.sky->generateMipmaps||faces[0].size()==1,"Original sky embedded mip levels must not be regenerated");
			const bool decodeForMipGeneration=!raw&&input.sky->generateMipmaps&&faceSize>1;
			Require(raw||decodeForMipGeneration||lease.Device().supportsBCTextureCompression,"Original sky cube needs actual BC support");
			const auto compressedFormat=raw?MTLPixelFormatRGBA8Unorm:format==WorldTextureFormat::BC1RGBA?MTLPixelFormatBC1_RGBA:
				format==WorldTextureFormat::BC2RGBA?MTLPixelFormatBC2_RGBA:
				format==WorldTextureFormat::BC3RGBA?MTLPixelFormatBC3_RGBA:
				format==WorldTextureFormat::BC4Unorm?MTLPixelFormatBC4_RUnorm:
				format==WorldTextureFormat::BC5Unorm?MTLPixelFormatBC5_RGUnorm:
				format==WorldTextureFormat::BC7RGBA?MTLPixelFormatBC7_RGBAUnorm:MTLPixelFormatBC7_RGBAUnorm_sRGB;
			const auto decodedFormat=format==WorldTextureFormat::BC7RGBASRGB?MTLPixelFormatRGBA8Unorm_sRGB:MTLPixelFormatRGBA8Unorm;
			id<MTLTexture> nextSky=Cube(lease,decodeForMipGeneration?decodedFormat:compressedFormat,faceSize,input.sky->generateMipmaps?Levels(faceSize):uint32_t(faces[0].size()),MTLStorageModeShared,MTLTextureUsageShaderRead,String(input.sky->sourcePath));
			// Metal cannot generate mips into BC storage. Decode only base-only
			// cubes that need generated levels, using the engine's existing decoder.
			// The retained DDS payload already has nv_dds's flip and +/-Y swap;
			// decode those same bytes without another orientation transform.
			// Embedded chains keep their original compressed bytes and levels.
			std::vector<uint8_t> decoded;
			for(size_t face=0;face<6;++face){Require(faces[face].size()==faces[0].size(),"Original sky faces have unequal mip counts");uint32_t size=faceSize;
				for(size_t level=0;level<faces[face].size();++level){const auto& mip=faces[face][level];const size_t row=raw?size_t(size)*4:size_t((size+3)/4)*block;Require(mip.width==size&&mip.height==size&&mip.bytesPerRow==row&&mip.bytes.size()==row*(raw?size:(size+3)/4),"Original sky cube mip bytes/extent mismatch");
					if(decodeForMipGeneration){
						DecodeSkyCubeBase(mip,format,lease.Capabilities().maxTextureDimension2D,decoded);
						[nextSky replaceRegion:MTLRegionMake2D(0,0,size,size) mipmapLevel:level slice:face withBytes:decoded.data() bytesPerRow:size_t(size)*4 bytesPerImage:0];
					}else{
						[nextSky replaceRegion:MTLRegionMake2D(0,0,size,size) mipmapLevel:level slice:face withBytes:mip.bytes.data() bytesPerRow:row bytesPerImage:0];
					}
					size=std::max(1u,size/2);}}
			// Publish only after every face/mip validates and uploads. Generated
			// levels still require the existing GPU producer and completion path.
			impl->sky=nextSky;impl->skyReady=!input.sky->generateMipmaps||nextSky.mipmapLevelCount==1;
		}else if(input.sky->kind==NativeSkyKind::Equirectangular){
			Require(input.sky->equirectangular&&input.sky->equirectangular->mips.size()==1,"Original equirectangular sky image is missing or has unsupported mip storage");
			const auto& image=*input.sky->equirectangular;const auto& mip=image.mips[0];
			Require(image.format==WorldTextureFormat::RGBA8Unorm&&!image.flipV&&mip.width>0&&mip.height>0&&mip.width<=lease.Capabilities().maxTextureDimension2D&&mip.height<=lease.Capabilities().maxTextureDimension2D&&mip.bytesPerRow==size_t(mip.width)*4&&mip.bytes.size()==mip.bytesPerRow*mip.height,"Original equirectangular sky RGBA payload mismatch");
			auto* td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:mip.width height:mip.height mipmapped:NO];td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
			id<MTLTexture> nextEqui=[lease.Device() newTextureWithDescriptor:td];Require(nextEqui!=nil,"Cannot allocate original equirectangular source image");
			[nextEqui replaceRegion:MTLRegionMake2D(0,0,mip.width,mip.height) mipmapLevel:0 withBytes:mip.bytes.data() bytesPerRow:mip.bytesPerRow];
			id<MTLTexture> nextSky=Cube(lease,MTLPixelFormatRGBA8Unorm,mip.height,input.sky->generateMipmaps?Levels(mip.height):1,MTLStorageModePrivate,MTLTextureUsageShaderRead|MTLTextureUsageShaderWrite,String(input.sky->sourcePath));
			id<MTLTexture> nextArray=[nextSky newTextureViewWithPixelFormat:MTLPixelFormatRGBA8Unorm textureType:MTLTextureType2DArray levels:NSMakeRange(0,1) slices:NSMakeRange(0,6)];Require(nextArray!=nil,"Cannot create actual sky cube face write view");
			impl->equirect=nextEqui;impl->sky=nextSky;impl->skyArray=nextArray;
		}else if(input.sky->kind==NativeSkyKind::Modern||input.sky->kind==NativeSkyKind::OriginalNull){
			// Modern's specialized fragment omits the cube argument. OriginalNull
			// keeps the existing no-draw path in main/reflection/water encoding.
		}else throw std::runtime_error("Unsupported original sky selection");
	}
	impl->Pipelines(lease,mainFormat,validation,shaderMilliseconds,samples);
	if(!originalSource && (impl->reflection==nil||impl->reflectionSize!=input.reflectionSize||impl->reflectionMipmaps!=input.reflectionMipmaps||impl->skyOnly!=input.skyOnlyReflections)){
		impl->reflection=Cube(lease,MTLPixelFormatRGBA8Unorm,input.reflectionSize,input.reflectionMipmaps?Levels(input.reflectionSize):1,MTLStorageModePrivate,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget,@"Original engine environment reflection cube");
		impl->skyReflection=input.skyOnlyReflections?Cube(lease,MTLPixelFormatRGBA8Unorm,input.reflectionSize,1,MTLStorageModePrivate,MTLTextureUsageShaderRead|MTLTextureUsageRenderTarget,@"Original SMF sky-only reflection cube"):nil;
		auto* depth=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:input.reflectionSize height:input.reflectionSize mipmapped:NO];depth.storageMode=MTLStorageModePrivate;depth.usage=MTLTextureUsageRenderTarget;
		impl->depth=[lease.Device() newTextureWithDescriptor:depth];Require(impl->depth!=nil,"Cannot allocate original reflection depth attachment");
		impl->reflectionSize=input.reflectionSize;impl->reflectionMipmaps=input.reflectionMipmaps;impl->skyOnly=input.skyOnlyReflections;impl->cursor=impl->initializedFaces=0;impl->reflectionReady=false;
	}
	if(!originalSource && impl->specularOwner!=input.specular){const auto& source=*input.specular;
		if(impl->specular==nil||impl->specular.width!=source.size)impl->specular=Cube(lease,MTLPixelFormatRGBA8Unorm,source.size,1,MTLStorageModeShared,MTLTextureUsageShaderRead,@"Original CPU-generated specular cube");
		for(uint32_t face=0;face<6;++face){Require(source.faces[face].size()==size_t(source.size)*source.size*4,"Original specular cube face extent mismatch");[impl->specular replaceRegion:MTLRegionMake2D(0,0,source.size,source.size) mipmapLevel:0 slice:face withBytes:source.faces[face].data() bytesPerRow:size_t(source.size)*4 bytesPerImage:0];}
		impl->specularOwner=input.specular;
	}
	if(impl->brdf==nil){auto* d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRG16Float width:512 height:512 mipmapped:NO];d.storageMode=MTLStorageModePrivate;d.usage=MTLTextureUsageShaderWrite|MTLTextureUsageShaderRead;impl->brdf=[lease.Device() newTextureWithDescriptor:d];Require(impl->brdf!=nil,"Cannot allocate exact original unit RG16F BRDF");impl->brdf.label=@"Exact30770 generated unit BRDF 512 G_OPTION1";}
	auto value=std::make_unique<PreparedWorldEnvironment::Impl>();value->cache=impl.get();value->frame=std::make_shared<NativeWorldEnvironmentFrame>(input);
	if (originalSource) {
		auto prepare = [&](const std::shared_ptr<const NativeCUSTextureObject>& object) {
			Require(bool(object),"Original cube object was deleted before capture");
			auto backing=std::dynamic_pointer_cast<const NativeCubeTextureBacking>(object->backing);
			Require(bool(backing),"Original environment name is not backed by CubeMapHandler");
			return PrepareNativeCubeTexture(lease,backing,object->sampling,sourceSubmission);
		};
		value->sourceReflection=prepare(input.sourceReflection);value->sourceSpecular=prepare(input.sourceSpecular);
		if(input.sourceSkyReflection)value->sourceSky=prepare(input.sourceSkyReflection);
		value->sourceFace=prepare(input.sourceCubeUpdate->current);
		if(input.sourceCubeUpdate->mipCurrent)value->sourceMips=prepare(input.sourceCubeUpdate->mipCurrent);
		impl->reflection=value->sourceReflection->Image().texture;impl->specular=value->sourceSpecular->Image().texture;
		impl->skyReflection=value->sourceSky?value->sourceSky->Image().texture:nil;
		impl->depth=NativeCubeDepth(*input.sourceCubeUpdate->backing);
		impl->reflectionSize=input.reflectionSize;impl->reflectionMipmaps=input.reflectionMipmaps;impl->skyOnly=input.skyOnlyReflections;
	}
	for(id object:{id(impl->library),id(impl->mainSky),id(impl->reflectionSky),id(impl->brdfPipeline),id(impl->equirectPipeline),id(impl->skyDepth)})lease.Retain(object);
	for(id<MTLTexture> texture:{impl->sky,impl->reflection,impl->skyReflection,impl->specular,impl->brdf,impl->depth,impl->equirect,impl->skyArray})if(texture)lease.AddAllocation(texture);

	value->face=originalSource?input.sourceCubeUpdate->face:impl->cursor%6;value->skyOnly=originalSource?input.sourceCubeUpdate->skyOnly:impl->cursor>=6;value->wrap=originalSource?(input.sourceCubeUpdate->backing->regenerateMipmaps||bool(input.sourceCubeUpdate->mipBacking)):impl->cursor+1==(input.skyOnlyReflections?12u:6u);
	value->willBeReady=impl->reflectionReady||(((impl->initializedFaces|(value->skyOnly?0u:1u<<value->face))==63u)&&(!input.reflectionMipmaps||value->wrap));
	if(input.reflectionGround&&input.reflectionGround->debugCubeSky){
		Require(input.reflectionGround->debugCubeSky==input.sky&&!input.reflectionGround->groundInvoked,
			"Original debug face must retain its actual cube and skip terrain");
		const auto& state=input.reflectionGround->debugCubeState;
		Require(state.frontPolygonMode==NativePolygonMode::Fill&&state.backPolygonMode==NativePolygonMode::Fill&&!state.inheritedClipPlane,
			"Debug cube special polygon/clip state needs the general raster implementation");
		Require(std::all_of(state.colorWrite.begin(),state.colorWrite.end(),[](bool enabled){return enabled;}),
			"Debug cube masked output needs the general raster implementation");
		value->debugDepth=effectstate::Depth(lease,state,MTLPixelFormatInvalid);
	}
	value->mainUniform=Uniform(lease,SkyState(input,mainCamera,false));value->reflectionUniform=Uniform(lease,SkyState(input,input.reflectionCameras[value->face],true));
	NSError* error=nil;auto* ad=[MTL4ArgumentTableDescriptor new];ad.maxBufferBindCount=1;ad.maxTextureBindCount=2;ad.initializeBindings=YES;
	value->arguments=[lease.Device() newArgumentTableWithDescriptor:ad error:&error];Require(value->arguments!=nil,"Cannot create original environment argument table: "+Error(error));lease.Retain(value->arguments);
	value->pass=[MTL4RenderPassDescriptor new];auto ca=value->pass.colorAttachments[0];ca.texture=value->skyOnly?impl->skyReflection:impl->reflection;ca.slice=value->face;ca.loadAction=originalSource?MTLLoadActionLoad:MTLLoadActionClear;ca.storeAction=MTLStoreActionStore;
	const auto& reflectedLighting=originalSource?input.reflectionLighting:input.lighting;
	ca.clearColor=MTLClearColorMake(reflectedLighting.fogColor[0],reflectedLighting.fogColor[1],reflectedLighting.fogColor[2],1);
	// The original reflection FBO keeps its depth attachment for sky-only
 // faces too. Retain it through same-target dependency/SSAO encoder breaks.
 value->pass.depthAttachment.texture=impl->depth;
 value->pass.depthAttachment.loadAction=originalSource?MTLLoadActionLoad:MTLLoadActionClear;
 value->pass.depthAttachment.storeAction=MTLStoreActionStore;value->pass.depthAttachment.clearDepth=1;
	value->pass.renderTargetWidth=value->pass.renderTargetHeight=input.reflectionSize;
	return std::unique_ptr<PreparedWorldEnvironment>(new PreparedWorldEnvironment(std::move(value)));
}
void PreparedWorldEnvironment::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(!impl->dependenciesEncoded,"Original environment dependency pass encoded twice");impl->dependenciesEncoded=true;
	if(impl->sourceFace)impl->sourceFace->EncodeDependencies(command);
	if(!impl->cache->skyReady&&impl->cache->sky){
		id<MTL4ComputeCommandEncoder> encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode original sky conversion/mip producer");
		if(impl->cache->equirect){[impl->arguments setTexture:impl->cache->equirect.gpuResourceID atIndex:0];[impl->arguments setTexture:impl->cache->skyArray.gpuResourceID atIndex:1];[encoder setComputePipelineState:impl->cache->equirectPipeline];[encoder setArgumentTable:impl->arguments];[encoder dispatchThreads:MTLSizeMake(impl->cache->sky.width,impl->cache->sky.height,6) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[encoder barrierAfterEncoderStages:MTLStageDispatch beforeEncoderStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];}
		if(impl->frame->sky->generateMipmaps&&impl->cache->sky.mipmapLevelCount>1)[encoder generateMipmapsForTexture:impl->cache->sky];
		[encoder barrierAfterStages:MTLStageDispatch|MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
	}
	if(!impl->cache->brdfReady){id<MTL4ComputeCommandEncoder> encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode original unit BRDF producer");
		[impl->arguments setTexture:impl->cache->brdf.gpuResourceID atIndex:0];[encoder setComputePipelineState:impl->cache->brdfPipeline];[encoder setArgumentTable:impl->arguments];
		[encoder dispatchThreads:MTLSizeMake(512,512,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
		[encoder barrierAfterStages:MTLStageDispatch beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];}
}
MTL4RenderPassDescriptor* PreparedWorldEnvironment::ReflectionPass() const{return impl->pass;}
const WorldCamera& PreparedWorldEnvironment::ReflectionCamera() const{return impl->frame->reflectionCameras[impl->face];}
bool PreparedWorldEnvironment::ReflectionSkyOnly() const{return impl->skyOnly;}
void PreparedWorldEnvironment::EncodeSky(id<MTL4RenderCommandEncoder> encoder,bool reflection)
{
	const bool debugFace=reflection&&impl->frame->reflectionGround&&bool(impl->frame->reflectionGround->debugCubeSky);
	if((!debugFace&&!impl->frame->drawSky)||impl->frame->sky->kind==NativeSkyKind::OriginalNull)return;
	auto* cache=impl->cache;[encoder setRenderPipelineState:reflection?cache->reflectionSky:cache->mainSky];
	[encoder setDepthStencilState:debugFace?impl->debugDepth:((reflection&&impl->skyOnly)?nil:cache->skyDepth)];
	[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setCullMode:MTLCullModeNone];[encoder setDepthBias:0 slopeScale:0 clamp:0];
	if(debugFace){
		const auto& state=impl->frame->reflectionGround->debugCubeState;
		[encoder setDepthClipMode:state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
		if(state.cullEnabled&&state.cull==NativeWorldCull::FrontAndBack)return;
		[encoder setFrontFacingWinding:MTLWindingCounterClockwise];[encoder setTriangleFillMode:MTLTriangleFillModeFill];
		[encoder setCullMode:(!state.cullEnabled||state.cull==NativeWorldCull::None)?MTLCullModeNone:(state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack)];
		[encoder setDepthBias:state.polygonOffset?state.offsetUnits:0 slopeScale:state.polygonOffset?state.offsetFactor:0 clamp:0];
	}
	[impl->arguments setAddress:(reflection?impl->reflectionUniform:impl->mainUniform).gpuAddress atIndex:0];
	[impl->arguments setTexture:cache->sky?cache->sky.gpuResourceID:MTLResourceID{0} atIndex:0];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:debugFace?impl->face*6:0 vertexCount:debugFace?6:36];
}
uint32_t PreparedWorldEnvironment::PrepareWaterSky(RuntimeLease& lease,const WorldCamera& camera)
{
	Require(impl->waterUniforms.size()<2,"Original water has at most two ordered scene views");
	impl->waterUniforms.push_back(Uniform(lease,SkyState(*impl->frame,camera,false)));
	return uint32_t(impl->waterUniforms.size()-1);
}
void PreparedWorldEnvironment::EncodeWaterSky(id<MTL4RenderCommandEncoder> encoder,uint32_t index)
{
	Require(index<impl->waterUniforms.size(),"Water sky camera was not prepared");
	if(!impl->frame->drawSky||impl->frame->sky->kind==NativeSkyKind::OriginalNull)return;
	auto* cache=impl->cache;
	[encoder setRenderPipelineState:cache->reflectionSky];[encoder setDepthStencilState:cache->skyDepth];
	[encoder setDepthClipMode:MTLDepthClipModeClip];[encoder setCullMode:MTLCullModeNone];[encoder setDepthBias:0 slopeScale:0 clamp:0];
	[impl->arguments setAddress:impl->waterUniforms[index].gpuAddress atIndex:0];
	[impl->arguments setTexture:cache->sky?cache->sky.gpuResourceID:MTLResourceID{0} atIndex:0];
	[encoder setArgumentTable:impl->arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:36];
}
void PreparedWorldEnvironment::FinishReflection(id<MTL4CommandBuffer> command)
{
	Require(!impl->reflectionFinished&&impl->dependenciesEncoded,"Original reflection face ordering violated");impl->reflectionFinished=true;
	if(impl->sourceFace){impl->sourceFace->FinishReflection(command);if(impl->sourceMips)impl->sourceMips->EncodeDependencies(command);return;}
	id<MTL4ComputeCommandEncoder> encoder=[command computeCommandEncoder];Require(encoder!=nil,"Cannot encode original reflection completion barrier");
	[encoder barrierAfterQueueStages:MTLStageFragment beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];
	if(impl->cache->reflectionMipmaps&&impl->wrap)[encoder generateMipmapsForTexture:impl->cache->reflection];
	[encoder barrierAfterStages:MTLStageBlit beforeQueueStages:MTLStageFragment visibilityOptions:MTL4VisibilityOptionDevice];[encoder endEncoding];
}
void PreparedWorldEnvironment::DidComplete()
{
	Require(impl->dependenciesEncoded&&impl->reflectionFinished,"Cannot publish original environment resources before encoded retirement");
	if(impl->sourceFace){impl->sourceFace->DidComplete();if(impl->sourceMips)impl->sourceMips->DidComplete();impl->sourceReflection->DidComplete();impl->sourceSpecular->DidComplete();if(impl->sourceSky)impl->sourceSky->DidComplete();}
	auto* cache=impl->cache;cache->brdfReady=true;cache->skyReady=true;if(!impl->skyOnly)cache->initializedFaces|=1u<<impl->face;cache->reflectionReady=impl->willBeReady;
	if(!impl->sourceFace)cache->cursor=(cache->cursor+1)%(impl->frame->skyOnlyReflections?12u:6u);
}
bool PreparedWorldEnvironment::ReflectionReady() const{return impl->willBeReady;}
id<MTLTexture> PreparedWorldEnvironment::Reflection() const{return impl->cache->reflection;}
id<MTLTexture> PreparedWorldEnvironment::SkyReflection() const{return impl->cache->skyReflection;}
id<MTLTexture> PreparedWorldEnvironment::Specular() const{return impl->cache->specular;}
id<MTLTexture> PreparedWorldEnvironment::UnitBRDF() const{return impl->cache->brdf;}
uint32_t PreparedWorldEnvironment::Face() const{return impl->face;}
} // namespace rendering::metal4
