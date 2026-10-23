/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4UnitGlass.h"
#include "NativeUnitGlassShaders.h"
#include "System/Config/ConfigHandler.h"
#include <simd/simd.h>
#include <cstring>
#include <map>
#include <limits>
CONFIG(bool, NativeUnitGlass).defaultValue(false).description("Opt in to the native conditional unit-glass consumer; does not enable the game gadget.");
namespace rendering::metal4 {namespace {
struct Uniform {simd_float4x4 modelView,modelViewProjection,viewInverse,normalMatrix;simd_float4 sun,specular,team,pbrA,pbrB,alpha;simd_uint4 flags;};static_assert(sizeof(Uniform)==368);
struct Vertex {simd_float4 position,normal,tangent,bitangent,uv;};static_assert(sizeof(Vertex)==80);
id<MTLBuffer> Upload(RuntimeLease& l,const void* p,size_t n){Require(p&&n&&n<=l.Device().maxBufferLength,"Glass upload is empty or exceeds device");auto b=[l.Device()newBufferWithBytes:p length:n options:MTLResourceStorageModeShared];Require(b!=nil,"Glass upload failed");l.AddAllocation(b);return b;}
MTLBlendOperation Equation(ScreenBlendEquation v){switch(v){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}throw std::runtime_error("Glass blend equation invalid");}
ScreenSampler Sampling(const NativeTextureSampling& p,unsigned levels){
 ScreenSampler s;
 switch(p.minFilter){case 0x2600:s.filter=ScreenFilter::Nearest;break;case 0x2601:s.filter=ScreenFilter::Linear;break;case 0x2700:s.filter=ScreenFilter::Nearest;s.mipFilter=ScreenMipFilter::Nearest;break;case 0x2701:s.filter=ScreenFilter::Linear;s.mipFilter=ScreenMipFilter::Nearest;break;case 0x2702:s.filter=ScreenFilter::Nearest;s.mipFilter=ScreenMipFilter::Linear;break;case 0x2703:s.filter=ScreenFilter::Linear;s.mipFilter=ScreenMipFilter::Linear;break;default:throw std::runtime_error("Glass actual minification filter differs");}
 Require(p.magFilter==0x2600||p.magFilter==0x2601,"Glass actual magnification filter differs");s.magnificationFilter=p.magFilter==0x2600?ScreenFilter::Nearest:ScreenFilter::Linear;
 auto wrap=[](unsigned v){switch(v){case 0x2901:return ScreenWrap::Repeat;case 0x812F:return ScreenWrap::ClampEdge;default:throw std::runtime_error("Glass source addressing requires an unimplemented original boundary");}};s.wrapS=wrap(p.wrapS);s.wrapT=wrap(p.wrapT);
 Require(p.compareMode==0&&p.baseLevel==0&&levels&&p.maxLevel>=int(levels-1)&&p.minLod==-1000&&p.maxLod==1000&&p.lodBias==0,"Glass source sampling interval differs");
 Require(std::isfinite(p.anisotropy)&&p.anisotropy>=1&&p.anisotropy<=16&&std::floor(p.anisotropy)==p.anisotropy,"Glass actual anisotropy differs");s.anisotropy=uint32_t(p.anisotropy);return s;
}
}
struct PreparedUnitGlass::Impl {
 std::shared_ptr<const NativeUnitGlassInput> source;NativeWorldDrawState state;effectstate::Raster raster;uint32_t count=0;
 __strong id<MTLRenderPipelineState> pipeline=nil;__strong id<MTLDepthStencilState> depth=nil;__strong id<MTL4ArgumentTable> arguments=nil;__strong id<MTLBuffer> indices=nil;
 std::vector<CUSSourceImage> images;
};
PreparedUnitGlass::PreparedUnitGlass():impl(std::make_unique<Impl>()){}PreparedUnitGlass::~PreparedUnitGlass()=default;
void PreparedUnitGlass::EncodeDependencies(id<MTL4CommandBuffer> c)const{for(const auto& i:impl->images)if(i.encodeDependencies)i.encodeDependencies(c);}
void PreparedUnitGlass::DidComplete()const{for(const auto& i:impl->images)if(i.didComplete)i.didComplete();}
void PreparedUnitGlass::Encode(id<MTL4RenderCommandEncoder> e)const{
 auto& d=*impl;if(!d.count)return;for(const auto& i:d.images)i.requireReadable();effectstate::RasterState(e,d.state,d.raster);
 [e setCullMode:d.state.cullEnabled?(d.state.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack):MTLCullModeNone];[e setRenderPipelineState:d.pipeline];[e setDepthStencilState:d.depth];[e setArgumentTable:d.arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:d.count indexType:MTLIndexTypeUInt32 indexBuffer:d.indices.gpuAddress indexBufferLength:d.indices.length instanceCount:1 baseVertex:0 baseInstance:0];
}
struct UnitGlassGPU::Impl {
 std::shared_ptr<const void> compiled;__strong id<MTLLibrary> library=nil;
 std::map<std::array<unsigned,17>,id<MTLRenderPipelineState>> pipelines;
 id<MTLRenderPipelineState> Pipeline(RuntimeLease& l,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,const ScreenBlendState& b,const std::array<bool,4>& mask,bool validation,double& ms,unsigned samples,bool coverage){
 std::array<unsigned,17> key{unsigned(color),unsigned(depth),unsigned(stencil),unsigned(b.enabled),unsigned(b.sourceColor),unsigned(b.destinationColor),unsigned(b.sourceAlpha),unsigned(b.destinationAlpha),unsigned(b.colorEquation),unsigned(b.alphaEquation),unsigned(mask[0]),unsigned(mask[1]),unsigned(mask[2]),unsigned(mask[3]),unsigned(validation),l.SampleShadingKey(samples),unsigned(coverage)};if(auto i=pipelines.find(key);i!=pipelines.end())return i->second;
 auto start=Clock::now();NSError* error=nil;if(library==nil){auto d=[MTL4LibraryDescriptor new];d.source=String(RECOIL_NATIVE_UNIT_GLASS_MSL);d.name=@"Original conditional unit glass";auto o=[MTLCompileOptions new];o.mathMode=MTLMathModeSafe;o.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;d.options=o;library=l.NewLibrary(d,&error);Require(library!=nil,"Glass original shader failed: "+Error(error));}
 auto d=[MTL4RenderPipelineDescriptor new];auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=library;vs.name=@"GlassVertexMain";fs.name=@"GlassFragmentMain";d.vertexFunctionDescriptor=vs;d.fragmentFunctionDescriptor=fs;d.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;d.rasterSampleCount=samples;d.alphaToCoverageState=coverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;
 auto a=d.colorAttachments[0];a.pixelFormat=color;a.blendingState=b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);a.rgbBlendOperation=Equation(b.colorEquation);a.alphaBlendOperation=Equation(b.alphaEquation);a.writeMask=MTLColorWriteMask((mask[0]?MTLColorWriteMaskRed:0)|(mask[1]?MTLColorWriteMaskGreen:0)|(mask[2]?MTLColorWriteMaskBlue:0)|(mask[3]?MTLColorWriteMaskAlpha:0));
 auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);d.options=options;auto result=l.NewRenderPipeline(d,&error);Require(result!=nil,"Glass pipeline failed: "+Error(error));pipelines.emplace(key,result);ms+=Milliseconds(start);return result;
 }
};
UnitGlassGPU::UnitGlassGPU():impl(std::make_unique<Impl>()){}
UnitGlassGPU::~UnitGlassGPU(){if(UnitGlassConsumerOwner()==impl->compiled)UnitGlassConsumerOwner().reset();}
void UnitGlassGPU::Initialize(RuntimeLease& l,MTLPixelFormat color,bool validation,double& ms,unsigned samples){
 if(!configHandler->GetBool("NativeUnitGlass"))return;
 ScreenBlendState b;b.enabled=true;b.sourceColor=b.sourceAlpha=ScreenBlendFactor::SourceAlpha;b.destinationColor=b.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;
 impl->Pipeline(l,color,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,b,{true,true,true,true},validation,ms,samples,false);
 impl->compiled=std::make_shared<int>(0);UnitGlassConsumerOwner()=impl->compiled;
}
void UnitGlassGPU::ClearResources(){}
std::shared_ptr<PreparedUnitGlass> UnitGlassGPU::Prepare(RuntimeLease& l,const NativeWorldDrawCommand& c,const CUSSourceImageProvider& images,const WorldCamera& view,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,MTLPixelFormat color,MTLPixelFormat depth,MTLPixelFormat stencil,bool validation,double& ms,unsigned samples){
 Require(c.unitGlass&&impl->compiled&&c.unitGlass->compiled==impl->compiled,"Glass consumer lifetime differs");ValidateNativeWorldDrawCommand(c);const auto& d=*c.unitGlass;
 Require(!c.world.inheritedClipPlane&&c.world.frontPolygonMode==NativePolygonMode::Fill&&c.world.backPolygonMode==NativePolygonMode::Fill,"Glass source raster profile differs");
 auto result=std::shared_ptr<PreparedUnitGlass>(new PreparedUnitGlass);auto& out=*result->impl;out.source=c.unitGlass;out.state=c.world;auto camera=view;if(c.surface.viewport){const auto& v=*c.surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}auto raster=effectstate::Viewport(camera,width,height,logicalWidth,logicalHeight,c.surface.scissor);if(!raster||(c.world.cullEnabled&&c.world.cull==NativeWorldCull::FrontAndBack))return result;out.raster=*raster;
 const auto& piece=d.model->pieces.at(d.piece);if(piece.indices.empty())return result;Require(piece.indices.size()<=std::numeric_limits<uint32_t>::max(),"Glass source index extent exceeds native draw");
 std::vector<Vertex> vertices;vertices.reserve(piece.vertices.size());for(const auto& v:piece.vertices)vertices.push_back({simd_make_float4(v.position[0],v.position[1],v.position[2],1),simd_make_float4(v.normal[0],v.normal[1],v.normal[2],0),simd_make_float4(v.sTangent[0],v.sTangent[1],v.sTangent[2],0),simd_make_float4(v.tTangent[0],v.tTangent[1],v.tTangent[2],0),simd_make_float4(v.uv[0][0],v.uv[0][1],0,0)});
 auto vb=Upload(l,vertices.data(),vertices.size()*sizeof(Vertex));out.indices=Upload(l,piece.indices.data(),piece.indices.size()*sizeof(uint32_t));out.count=uint32_t(piece.indices.size());
 Uniform u{};simd_float4x4 projection;std::memcpy(&u.modelView,d.modelView.data(),64);std::memcpy(&projection,d.projection.data(),64);std::memcpy(&u.viewInverse,d.viewInverse.data(),64);u.modelViewProjection=simd_mul(projection,u.modelView);simd_float3x3 upper={u.modelView.columns[0].xyz,u.modelView.columns[1].xyz,u.modelView.columns[2].xyz};const float determinant=simd_determinant(upper);Require(std::isfinite(determinant)&&determinant!=0,"Glass actual normal matrix is singular/nonfinite");const auto normal=simd_transpose(simd_inverse(upper));u.normalMatrix=matrix_identity_float4x4;for(unsigned column=0;column<3;++column)u.normalMatrix.columns[column]=simd_make_float4(normal.columns[column],0);u.sun=simd_make_float4(d.sun[0],d.sun[1],d.sun[2],0);u.specular=simd_make_float4(d.specular[0],d.specular[1],d.specular[2],0);std::memcpy(&u.team,d.team.data(),16);std::memcpy(&u.pbrA,d.pbr.data(),16);std::memcpy(&u.pbrB,d.pbr.data()+4,16);u.alpha=simd_make_float4(c.surface.alphaTest.reference,unsigned(c.surface.alphaTest.compare),c.surface.alphaTest.enabled,0);u.flags=simd_make_uint4(d.zeroToOne,0,0,0);
 auto desc=[MTL4ArgumentTableDescriptor new];desc.maxBufferBindCount=2;desc.maxTextureBindCount=4;desc.maxSamplerStateBindCount=4;desc.initializeBindings=YES;NSError* error=nil;out.arguments=[l.Device()newArgumentTableWithDescriptor:desc error:&error];Require(out.arguments!=nil,"Glass argument table failed: "+Error(error));[out.arguments setAddress:vb.gpuAddress atIndex:0];
 for(unsigned n=0;n<4;++n){Require(bool(images),"Glass needs its actual original model image provider");(void)Sampling(d.images[n].object->sampling,d.images[n].object->allocation.levels);auto image=images(l,d.images[n],n);Require(image.texture&&image.sampler&&image.owner&&image.requireReadable,"Glass sampled resource lost immutable owner");if(n<3&&image.flipV)u.flags.y|=1u<<n;Require(n<3||!image.flipV,"Glass reflection cube must retain its native cube orientation");[out.arguments setTexture:image.texture.gpuResourceID atIndex:n];[out.arguments setSamplerState:image.sampler.gpuResourceID atIndex:n];out.images.push_back(std::move(image));}
 auto ub=Upload(l,&u,sizeof(u));[out.arguments setAddress:ub.gpuAddress atIndex:1];out.pipeline=impl->Pipeline(l,color,depth,stencil,c.surface.blend,c.world.colorWrite,validation,ms,samples,c.world.alphaToCoverage);out.depth=effectstate::Depth(l,c.world,stencil);l.Retain(out.arguments);l.Retain(out.pipeline);l.Retain(impl->library);return result;
}
}
