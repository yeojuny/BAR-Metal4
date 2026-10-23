/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Included inside rendering::metal4 by the existing map-edge consumer.
namespace {
const char* MAP_EDGE_DEFERRED_SHADER=R"MSL(
vertex EdgeVarying MapEdgeDeferredVertex(uint vertexId [[vertex_id]],uint instanceId [[instance_id]],
	constant EdgeUniform& u [[buffer(0)]],device const float4* instances [[buffer(1)]],texture2d<float> heightMap [[texture(1)]],sampler heightSampler [[sampler(1)]]) {
	const float4 aMirrorParams=instances[instanceId];
	const float2 flip=aMirrorParams.xy,offset=aMirrorParams.zw;
	const int cols=int(flip.x>0.5f?u.mesh.z:u.mesh.x),rows=int(flip.y>0.5f?u.mesh.w:u.mesh.y);
	const int rev=int((flip.x>0.5f&&offset.x<0.0f)!=(flip.y>0.5f&&offset.y<0.0f));
	const int mapRev=int((flip.x>0.5f&&offset.x>0.0f)!=(flip.y>0.5f&&offset.y>0.0f));
	const bool alongZ=rev==mapRev;const int swap=alongZ?1-rev:rev;
	const int outer=alongZ?cols:rows,inner=alongZ?rows:cols,perLine=2*(inner+1)+2;
	const int line=int(vertexId)/perLine,k=int(vertexId)-line*perLine;
	int i=k>>1,o=line+((k&1)^swap);
	if(k==perLine-2){i=inner;o=line+(1^swap);}
	else if(k==perLine-1){i=0;o=line+1+swap;}
	if(line>=outer){i=0;o=outer+swap;}
	const int c=alongZ?o:i,r=alongZ?i:o;
	const float2 t=float2(float(c),float(r))*u.parameters.x;
	const bool2 mirrored=flip>float2(0.5f);
	const float2 edge=step(float2(0.0f),offset)*u.mapSize.xy,dir=sign(offset);
	const float2 mapPos=select(t,edge-dir*t,mirrored);
	const float2 worldXZ=select(t,edge+dir*t,mirrored)-offset*u.reserved.x;
	const float2 adjusted=mapPos-float2(8)*(mapPos/u.mapSize.xy)+float2(4);
	float2 heightUV=clamp(adjusted,float2(8),u.mapSize.xy-float2(8))/u.mapSize.xy;
	if(u.options.z&2u)heightUV.y=1.0f-heightUV.y;
	float height=heightMap.sample(heightSampler,heightUV,level(0)).r;
	float alpha=1.0f;
	if(u.parameters.z==1.0f){
		const float2 dist=flip*(t-u.reserved.x),bend=dist/150.0f; height-=dot(bend,bend);
		const float2 fade=dist/u.mapSize.xy;alpha=clamp(1.0f+6.0f*(0.18f-dot(fade,fade)),0.0f,1.0f);
	}
	const float4 worldPos=float4(worldXZ.x,height,worldXZ.y,1.0f);
	float fog=1.0f;
	if(u.parameters.w==1.0f){const float distance=length((u.view*worldPos).xyz);fog=clamp((u.fogParameters.y-distance)*u.fogParameters.w,0.0f,1.0f);}
	EdgeVarying output;output.alphaFog=float2(alpha,fog);output.uv=mapPos/u.mapSize.xy;
	output.normalSign=1.0f-2.0f*flip;output.position=u.viewProjection*worldPos;
	if(u.options.x==1)output.position.z=(output.position.z+output.position.w)*0.5f;
	return output;
}

struct MapEdgeDeferredOutput {float4 normal [[color(0)]];float4 diffuse [[color(1)]];};
inline bool MapEdgeDeferredAlpha(float alpha,constant EdgeUniform& u){
 if(!(u.options.y&256u))return true;
 const float reference=u.reserved.y;
 switch(u.options.y&255u){case 0u:return false;case 1u:return alpha<reference;case 2u:return alpha==reference;
 case 3u:return alpha<=reference;case 4u:return alpha>reference;case 5u:return alpha!=reference;case 6u:return alpha>=reference;default:return true;}
}
fragment MapEdgeDeferredOutput MapEdgeDeferredFragment(EdgeVarying input [[stage_in]],constant EdgeUniform& u [[buffer(0)]],
 texture2d<float> color [[texture(0)]],sampler colorSampler [[sampler(0)]],
 texture2d<float> normals [[texture(2)]],sampler normalSampler [[sampler(2)]]){
 const float halfTexel=0.5f/1024.0f;
 float2 uv=clamp(input.uv,float2(halfTexel),float2(1.0f-halfTexel));
 float2 normalUV=uv;
 if(u.options.z&1u)uv.y=1.0f-uv.y;
 if(u.options.z&4u)normalUV.y=1.0f-normalUV.y;
 float4 finalColor=color.sample(colorSampler,uv);
 // The original native $normals logical view is R/G/G/G. GLSL .ra
 // therefore reads the physical RG16F .rg, once, without reconstructing from terrain.
 const float2 packedNormal=normals.sample(normalSampler,normalUV).rg;
 float3 normal;normal.xz=packedNormal;normal.y=sqrt(max(0.0f,1.0f-dot(packedNormal,packedNormal)));
 if(input.alphaFog.x<0.05f)discard_fragment();
 const float luma=dot(finalColor.rgb,float3(0.2126f,0.7152f,0.0722f));
 finalColor.rgb+=clamp(luma*u.parameters.y,0.0f,1.0f)-luma;
 const float normalLength=length(normal);normal=normalLength>0.001f?normal/normalLength:float3(0,1,0);
 normal.xz*=input.normalSign;
 finalColor.rgb*=dot(normal,u.sun.xyz)*0.5f+1.0f;
 finalColor.rgb=clamp(finalColor.rgb,float3(0),float3(1));
 finalColor.rgb=mix(u.fogColor.rgb,finalColor.rgb,input.alphaFog.y);finalColor.a=input.alphaFog.x;
 // Compatibility alpha test follows original output0 (normal alpha is 1),
 // while the actual diffuse attachment retains the original curvature alpha.
 if(!MapEdgeDeferredAlpha(1.0f,u))discard_fragment();
 return {float4(normal*0.5f+0.5f,1.0f),finalColor};
}
)MSL";
MTLBlendOperation EdgeDeferredEquation(ScreenBlendEquation value){
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;
	case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Unknown original map-edge deferred blend equation");
}
uint64_t EdgeDeferredBlendKey(const ScreenBlendState& s,const std::array<bool,4>& mask){
	uint64_t bits=uint64_t(s.enabled)|uint64_t(s.sourceColor)<<1|uint64_t(s.destinationColor)<<6|
		uint64_t(s.sourceAlpha)<<11|uint64_t(s.destinationAlpha)<<16|uint64_t(s.colorEquation)<<21|uint64_t(s.alphaEquation)<<24;
	for(unsigned i=0;i<4;++i)bits|=uint64_t(mask[i])<<(27+i);return bits;
}
}
std::shared_ptr<PreparedMapEdge> MapEdgeGPU::PrepareDeferred(RuntimeLease& lease,std::shared_ptr<const mapedge::DrawPacket> draw,
	const CUSSourceTarget& target,const CUSSourceImageProvider& images,bool validation,double& milliseconds)
{
	Require(draw&&draw->parameters.deferred&&draw->mapMRT&&draw->mapMRT->role==NativeGeometryTextureRole::Map&&
		draw->sourceDrawFrame==draw->mapMRT->sourceDrawFrame&&!draw->mapMRT->clearOnly,
		"Original deferred map-edge requires its actual selected Map MRT source phase");
	ValidateNativeCUSDeferredPhase(*draw->mapMRT);mapedge::Validate(draw->parameters,draw->mirrors);
	Require(draw->original&&bool(images)&&!draw->mirrors.empty(),"Original deferred map-edge lost its source uniforms/images/mirrors");
	ValidateNativeCUSUniformPublication(*draw->original);
	const auto& metadata=draw->mapMRT->depth.backing->Metadata();
	for(const auto format:target.colors)Require(format==MTLPixelFormatRGBA8Unorm,"Original deferred map-edge requires all five actual Map color attachments");
	Require(target.depth==MTLPixelFormatDepth32Float&&target.stencil==MTLPixelFormatInvalid&&target.samples==1&&
		target.width==metadata.width&&target.height==metadata.height&&target.logicalWidth==metadata.width&&target.logicalHeight==metadata.height,
		"Original deferred map-edge target differs from the same phase's real five-RGBA8/D32/no-stencil target");
	const auto& state=draw->raster;
	Require(!state.depthClamp&&!state.inheritedClipPlane&&!state.lineStipple&&state.frontPolygonMode==NativePolygonMode::Fill&&
		state.backPolygonMode==NativePolygonMode::Fill,"Original deferred map-edge inherited raster requires its selected typed consumer");
	auto result=std::shared_ptr<PreparedMapEdge>(new PreparedMapEdge);auto& out=*result->impl;out.source=draw;out.deferred=true;out.raster=state;
	auto camera=target.camera;if(draw->surface.viewport){const auto& v=*draw->surface.viewport;camera.viewport={v.x,v.y,int32_t(v.width),int32_t(v.height)};}
	auto raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,draw->surface.scissor);
	if(!raster)return result;out.viewport=*raster;
	Uniform u{};
	// Actual original UniformMatricesBuffer: cameraView row3, cameraViewProj row5.
	// Actual UniformParamsBuffer: mapSize row3, fogColor5, fogParams6, sunDir7.
	std::memcpy(&u.view,draw->original->matrices->bytes.data()+3*64,64);
	std::memcpy(&u.viewProjection,draw->original->matrices->bytes.data()+5*64,64);
	std::memcpy(&u.mapSize,draw->original->params->bytes.data()+3*16,16);
	std::memcpy(&u.fogColor,draw->original->params->bytes.data()+5*16,16);
	std::memcpy(&u.fogParameters,draw->original->params->bytes.data()+6*16,16);
	std::memcpy(&u.sun,draw->original->params->bytes.data()+7*16,16);
	uint32_t caps=0;std::memcpy(&caps,draw->original->params->bytes.data()+12,4);
	const auto& p=draw->parameters;const auto& mesh=p.mesh;
	u.parameters=simd_make_float4(p.gridSize,p.brightness,p.curvature?1.f:0.f,p.fog?1.f:0.f);
	u.options=simd_make_uint4(!(caps&1u),uint32_t(draw->surface.alphaTest.compare)|(uint32_t(draw->surface.alphaTest.enabled)<<8),0,0);
	u.reserved=simd_make_float4(0.5f,draw->surface.alphaTest.reference,0,0);
	Require(std::isfinite(u.mapSize.x)&&std::isfinite(u.mapSize.y)&&u.mapSize.x>=64&&u.mapSize.y>=64&&u.mapSize.x<=65536&&u.mapSize.y<=65536,
		"Original deferred map-edge map size is outside its actual source bounds");
	const uint32_t cols=uint32_t(std::ceil(double(u.mapSize.x)/64)),rows=uint32_t(std::ceil(double(u.mapSize.y)/64));
	const uint32_t trimCols=mesh[2],trimRows=mesh[3]; // Actual original Lua-selected trim, without recomputing its float arithmetic.
	Require(mesh[0]==cols&&mesh[1]==rows&&(p.curvature||(trimCols==cols&&trimRows==rows)),"Original deferred map-edge source mesh/settings differ");
	uint32_t count=0;for(const auto& m:draw->mirrors)count=std::max(count,m[0]&&m[1]?trimCols*(2*(trimRows+1)+2):m[0]?rows*(2*(trimCols+1)+2):trimRows*(2*(cols+1)+2));
	Require(mesh[4]==count,"Original deferred map-edge draw count differs from its actual existing mirror state");
	u.mesh=simd_make_uint4(mesh[0],mesh[1],mesh[2],mesh[3]);
	std::array<id<MTLTexture>,3> textures{};std::array<id<MTLSamplerState>,3> samplers{};
	for(unsigned i=0;i<3;++i){
		const auto& binding=draw->images[i];Require(binding.object&&binding.object->target==NativeCUSTextureTarget::Image2D,
			"Original deferred map-edge image lost its actual captured binding");
		auto image=images(lease,binding,i);const auto& a=binding.object->allocation;
		Require(image.texture&&image.sampler&&image.owner&&image.requireReadable&&!image.depthImage&&image.texture.device==lease.Device()&&
			image.sampler.device==lease.Device()&&image.texture.textureType==MTLTextureType2D&&
			image.texture.width==a.width&&image.texture.height==a.height&&image.texture.mipmapLevelCount==a.levels&&image.texture.sampleCount==1,
			"Original deferred map-edge image resolver substituted its actual storage/producer");
		const auto& swizzle=binding.object->sampling.swizzle;
		if(i==0)Require(swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},"Original deferred map-edge color needs its actual channel consumer");
		if(i==1)Require(swizzle[0]==0x1903&&image.texture.pixelFormat==MTLPixelFormatR32Float,"Original deferred map-edge height red source differs");
		if(i==2)Require(swizzle[0]==0x1903&&swizzle[3]==0x1904&&image.texture.pixelFormat==MTLPixelFormatRG16Float,
			"Original deferred map-edge $normals .ra must retain its actual native RG16F R/G/G/G source");
		u.options.z|=uint32_t(image.flipV)<<i;textures[i]=image.texture;samplers[i]=image.sampler;
		lease.AddAllocation(image.texture);lease.Retain(image.sampler);out.images.push_back(std::move(image));
	}
	if(!impl->deferredLibrary){
		const auto start=Clock::now();NSError* error=nil;auto descriptor=[MTL4LibraryDescriptor new];
		descriptor.source=String(std::string(SHADER)+MAP_EDGE_DEFERRED_SHADER);descriptor.name=@"Original31657 Map-edge deferred .ra/normal+diffuse";
		auto options=[MTLCompileOptions new];options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;descriptor.options=options;
		auto library=[lease.Compiler()newLibraryWithDescriptor:descriptor error:&error];Require(library!=nil,"Cannot compile original Map-edge deferred stages: "+Error(error));
		impl->deferredLibrary=library;milliseconds+=Milliseconds(start);
	}
	const auto key=std::tuple{EdgeDeferredBlendKey(draw->surface.blend,state.colorWrite),validation,target.combinedColor};auto found=impl->deferredPipelines.find(key);
	if(found==impl->deferredPipelines.end()){
		const auto start=Clock::now();NSError* error=nil;auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vs=[MTL4LibraryFunctionDescriptor new],fs=[MTL4LibraryFunctionDescriptor new];vs.library=fs.library=impl->deferredLibrary;
		vs.name=@"MapEdgeDeferredVertex";fs.name=@"MapEdgeDeferredFragment";descriptor.vertexFunctionDescriptor=vs;descriptor.fragmentFunctionDescriptor=fs;
		descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;descriptor.rasterSampleCount=1;
		const auto& b=draw->surface.blend;
		for(unsigned i=0;i<5;++i){auto a=descriptor.colorAttachments[i];a.pixelFormat=target.colors[i];
			// Original shader writes normal0 and diffuse1 only. Remaining original
			// attachments retain their existing contents, never broadcast this shader.
			a.writeMask=i<2?MTLColorWriteMask((state.colorWrite[0]?MTLColorWriteMaskRed:0)|(state.colorWrite[1]?MTLColorWriteMaskGreen:0)|
				(state.colorWrite[2]?MTLColorWriteMaskBlue:0)|(state.colorWrite[3]?MTLColorWriteMaskAlpha:0)):MTLColorWriteMaskNone;
			a.blendingState=i<2&&b.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
			a.sourceRGBBlendFactor=effectstate::Factor(b.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(b.destinationColor);
			a.sourceAlphaBlendFactor=effectstate::Factor(b.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(b.destinationAlpha);
			a.rgbBlendOperation=EdgeDeferredEquation(b.colorEquation);a.alphaBlendOperation=EdgeDeferredEquation(b.alphaEquation);
		}
		if(target.combinedColor!=MTLPixelFormatInvalid){descriptor.colorAttachments[5].pixelFormat=target.combinedColor;descriptor.colorAttachments[5].writeMask=MTLColorWriteMaskNone;}
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto pipeline=[lease.Compiler()newRenderPipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&error];
		Require(pipeline!=nil,"Cannot compile original Map-edge five-attachment PSO: "+Error(error));
		found=impl->deferredPipelines.emplace(key,pipeline).first;milliseconds+=Milliseconds(start);
	}
	auto uniforms=Buffer(lease,&u,sizeof(u));auto instances=Buffer(lease,draw->mirrors.data(),draw->mirrors.size()*sizeof(mapedge::Mirror));
	auto descriptor=[MTL4ArgumentTableDescriptor new];descriptor.maxBufferBindCount=2;descriptor.maxTextureBindCount=3;descriptor.maxSamplerStateBindCount=3;descriptor.initializeBindings=YES;
	NSError* error=nil;out.arguments=[lease.Device()newArgumentTableWithDescriptor:descriptor error:&error];Require(out.arguments!=nil,"Cannot allocate actual Map-edge deferred argument table");
	[out.arguments setAddress:uniforms.gpuAddress atIndex:0];[out.arguments setAddress:instances.gpuAddress atIndex:1];
	for(unsigned i=0;i<3;++i){[out.arguments setTexture:textures[i].gpuResourceID atIndex:i];[out.arguments setSamplerState:samplers[i].gpuResourceID atIndex:i];}
	out.pipeline=found->second;out.depth=effectstate::Depth(lease,state,MTLPixelFormatInvalid);out.vertices=mesh[4];out.instances=draw->mirrors.size();out.cull=p.cullBackFaces;
	lease.Retain(impl->deferredLibrary);lease.Retain(out.pipeline);lease.Retain(out.arguments);return result;
}
