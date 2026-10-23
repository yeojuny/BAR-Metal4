/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Test-only original GLSL reference; this file is never linked into the engine.
#define GL_SILENCE_DEPRECATION
#include "Rendering/Metal4/Metal4WorldParticles.h"
#include "Rendering/Backend/NativeLuaNanoRecorder.h"
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <filesystem>
#include <regex>
using namespace rendering;
using namespace rendering::metal4;
namespace {
constexpr unsigned width=96,height=96;
unsigned checks=0,renders=0;
void Check(bool valid,const char* message){++checks;if(!valid)throw std::runtime_error(message);}
std::array<float,16> Identity(){return {1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
std::string Read(const char* path){std::ifstream file(path,std::ios::binary);std::ostringstream bytes;bytes<<file.rdbuf();Check(bool(file),"Missing original shader reference");return bytes.str();}
uint64_t Hash(const std::string& bytes){uint64_t value=14695981039346656037ull;for(unsigned char c:bytes)value=(value^c)*1099511628211ull;return value;}
NativeBufferSnapshotRef Snapshot(NativeBufferLayout layout,const void* data,size_t bytes){NativeBufferSnapshotSource source;source.Define(layout);return source.Capture({static_cast<const uint8_t*>(data),bytes});}
NativeLuaNanoFrame Frame(unsigned shape,float time,bool glow)
{
	NativeLuaNanoFrame frame;frame.serial=1;frame.infoIsNormal=true;frame.mapSquares={16,16,16,16};frame.cameraViewInverse=frame.cameraViewProjection=Identity();
	NativeLuaNanoDraw draw;draw.shape=shape;draw.instanceCount=2;
	std::array<float,28> slots;for(unsigned n=0;n<28;++n)slots[n]=n;
	draw.vertexSlots=Snapshot({NativeBufferKind::Vertex,28,4,{{0,"vertexSlot",NativeScalarType::Float32,NativeValueShape::Scalar,1,0,4,false}}},slots.data(),sizeof(slots));
	std::vector<uint16_t> indices;if(shape==0)for(unsigned q=0;q<6;++q)for(unsigned n:{0u,1u,2u,0u,2u,3u})indices.push_back(q*4+n);else for(unsigned n=0;n<24;++n)indices.push_back(n);
	for(unsigned n:{0u,1u,2u,0u,2u,3u})indices.push_back(24+n);
	draw.indices=Snapshot({NativeBufferKind::Index,indices.size(),2,{{0,"",NativeScalarType::UInt16,NativeValueShape::Scalar,1,0,2,false}}},indices.data(),indices.size()*2);
	// Two original four-vec4 records, independently seeded by explicit offsets.
	const std::array<float,32> instances={-.35f,-.05f,.15f,4*1024.f+256.f,.003f,.002f,0,10,.12f,.31f,.06f,.22f,0,1,10,100,
		.32f,.12f,-.1f,8*1024.f+192.f,-.002f,-.001f,0,10,.30f,.05f,.18f,.18f,7,-2,10,100};
	NativeBufferLayout layout{NativeBufferKind::Vertex,2,64,{}};for(unsigned n=0;n<4;++n)layout.attributes.push_back({n+1,"",NativeScalarType::Float32,NativeValueShape::Scalar,4,n*16,16,false});
	draw.instances=Snapshot(layout,instances.data(),sizeof(instances));
	draw.uniforms={{"drawRadius",.15f},{"glowScale",glow?2.5f:1.f},{"glowIntensity",glow?.2f:0.f},{"glowFalloff",3},
		{"losAlwaysVisible",0},{"cubeShowInside",0},{"cubeNoise",0},{"cubeNoiseSpeed",25},{"cubeNoiseScale",1.75f},{"coreBoost",1},{"hueJitter",0},{"whiteHotspot",0},{"whiteHotspotThreshold",.6f}};
	(void)time;frame.draws.push_back(draw);draw.ghostStencilPass=true;frame.draws.push_back(draw);return frame;
}
GLuint Shader(GLenum type,const std::string& source)
{auto shader=glCreateShader(type);const char* text=source.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);GLint ready=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ready);if(!ready){char log[8192]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);throw std::runtime_error(log);}return shader;}
struct GLReference {
	CGLContextObj context=nullptr;GLuint fbo=0,color=0,depthStencil=0,program=0,vao=0,buffers[3]{},info=0;
	GLReference(std::string vertex,std::string fragment)
	{
		CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAOpenGLProfile,CGLPixelFormatAttribute(kCGLOGLPVersion_3_2_Core),CGLPixelFormatAttribute(0)};
		CGLPixelFormatObj format=nullptr;GLint count=0;Check(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError&&format,"Original NoGS reference GL core context unavailable");
		const auto error=CGLCreateContext(format,nullptr,&context);CGLDestroyPixelFormat(format);Check(error==kCGLNoError&&context,"Original NoGS reference context creation failed");Check(CGLSetCurrentContext(context)==kCGLNoError,"Original NoGS reference context activation failed");
		const auto adapt=[](std::string source){const std::string version="#version 430 core",token="//__ENGINEUNIFORMBUFFERDEFS__";auto at=source.find(version);Check(at!=std::string::npos,"Original GLSL version identity differs");source.replace(at,version.size(),"#version 410 core");at=source.find(token);Check(at!=std::string::npos,"Original engine UBO injection identity differs");source.replace(at,token.size(),"uniform mat4 cameraViewProj,cameraViewInv;uniform vec4 timeInfo,mapSize;");
			// macOS CGL ends at GLSL410, predating arrays-of-arrays. Flatten
			// the two original local tables bijectively; every corner expression
			// and access order is unchanged. Production MSL uses original arrays.
			const auto cube=source.find("// ---- CUBE (default)");
			if(cube!=std::string::npos){auto octa=source.substr(0,cube),tail=source.substr(cube);const std::regex access(R"(corners\[([^\]]+)\]\[([^\]]+)\])");
				octa=std::regex_replace(octa,access,"corners[($1)*3+($2)]");tail=std::regex_replace(tail,access,"corners[($1)*4+($2)]");
				const std::string first="vec3 corners[(8)*3+(3)];",second="vec3 corners[(6)*4+(4)];";auto pos=octa.find(first);Check(pos!=std::string::npos,"Original octa corner declaration changed");octa.replace(pos,first.size(),"vec3 corners[24];");pos=tail.find(second);Check(pos!=std::string::npos,"Original cube corner declaration changed");tail.replace(pos,second.size(),"vec3 corners[24];");source=octa+tail;}
			return source;};
		auto vs=Shader(GL_VERTEX_SHADER,adapt(vertex)),fs=Shader(GL_FRAGMENT_SHADER,adapt(fragment));program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);glDeleteShader(vs);glDeleteShader(fs);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked){char log[8192]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);throw std::runtime_error(log);}
		glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
		glGenRenderbuffers(1,&depthStencil);glBindRenderbuffer(GL_RENDERBUFFER,depthStencil);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,depthStencil);Check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Original NoGS reference FBO incomplete");
		glGenVertexArrays(1,&vao);glGenBuffers(3,buffers);glGenTextures(1,&info);glBindTexture(GL_TEXTURE_2D,info);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB10_A2,16,16,0,GL_RGBA,GL_UNSIGNED_INT_2_10_10_10_REV,nullptr);
		auto infoFbo=GLuint(0);glGenFramebuffers(1,&infoFbo);glBindFramebuffer(GL_FRAMEBUFFER,infoFbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,info,0);Check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Original empty-mode info FBO incomplete");glClearColor(.5,.5,.5,1);glClear(GL_COLOR_BUFFER_BIT);glBindTexture(GL_TEXTURE_2D,info);glGenerateMipmap(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glDeleteFramebuffers(1,&infoFbo);
	}
	~GLReference(){if(context){CGLSetCurrentContext(context);glDeleteProgram(program);glDeleteVertexArrays(1,&vao);glDeleteBuffers(3,buffers);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteTextures(1,&info);glDeleteRenderbuffers(1,&depthStencil);CGLSetCurrentContext(nullptr);CGLDestroyContext(context);}}
	std::vector<uint8_t> Render(const NativeLuaNanoFrame& frame,float time,unsigned mask,float clearDepth)
	{
		CGLSetCurrentContext(context);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glViewport(0,0,width,height);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_DITHER);glColorMask(true,true,true,true);glDepthMask(true);glStencilMask(0xFF);glClearColor(.02f,.03f,.04f,1);glClearDepth(clearDepth);glClearStencil(mask);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);glUseProgram(program);
		glUniformMatrix4fv(glGetUniformLocation(program,"cameraViewProj"),1,false,frame.cameraViewProjection.data());glUniformMatrix4fv(glGetUniformLocation(program,"cameraViewInv"),1,false,frame.cameraViewInverse.data());glUniform4f(glGetUniformLocation(program,"timeInfo"),time,0,0,0);glUniform4f(glGetUniformLocation(program,"mapSize"),128,128,128,128);
		glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,info);glUniform1i(glGetUniformLocation(program,"infoTex"),1);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glEnable(GL_STENCIL_TEST);glStencilMask(0);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glDepthMask(false);
		for(const auto& draw:frame.draws){glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffers[0]);glBufferData(GL_ARRAY_BUFFER,draw.vertexSlots->bytes.size(),draw.vertexSlots->bytes.data(),GL_STATIC_DRAW);glEnableVertexAttribArray(0);glVertexAttribPointer(0,1,GL_FLOAT,false,4,nullptr);glVertexAttribDivisor(0,0);
			glBindBuffer(GL_ARRAY_BUFFER,buffers[1]);glBufferData(GL_ARRAY_BUFFER,draw.instances->bytes.size(),draw.instances->bytes.data(),GL_STATIC_DRAW);for(unsigned n=0;n<4;++n){glEnableVertexAttribArray(n+1);glVertexAttribPointer(n+1,4,GL_FLOAT,false,64,reinterpret_cast<const void*>(uintptr_t(n*16)));glVertexAttribDivisor(n+1,1);}
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,buffers[2]);glBufferData(GL_ELEMENT_ARRAY_BUFFER,draw.indices->bytes.size(),draw.indices->bytes.data(),GL_STATIC_DRAW);
			for(const auto& [name,value]:draw.uniforms)glUniform1f(glGetUniformLocation(program,name.c_str()),value);glUniform1i(glGetUniformLocation(program,"u_shape"),draw.shape);
			if(draw.ghostStencilPass)glDisable(GL_DEPTH_TEST);else{glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);}glStencilFunc(draw.ghostStencilPass?GL_EQUAL:GL_NOTEQUAL,0x40,0x40);glDrawElementsInstanced(GL_TRIANGLES,draw.indices->layout.elements,draw.indices->layout.stride==2?GL_UNSIGNED_SHORT:GL_UNSIGNED_INT,nullptr,draw.instanceCount);
		}
		std::vector<uint8_t> bottom(width*height*4),top(bottom.size());glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,bottom.data());Check(glGetError()==GL_NO_ERROR,"Original NoGS GL reference reported an error");for(unsigned y=0;y<height;++y)std::copy_n(bottom.data()+size_t(y)*width*4,size_t(width)*4,top.data()+size_t(height-1-y)*width*4);return top;
	}
};
std::vector<uint8_t> Native(WorldParticlesGPU& particles,const NativeLuaNanoFrame& frame,float time,unsigned mask,float clearDepth,bool omit)
{
	RuntimeLease lease;const auto start=Clock::now();auto target=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];target.storageMode=MTLStorageModeShared;target.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto color=[lease.Device() newTextureWithDescriptor:target];Check(color!=nil,"Cannot allocate native oracle color");lease.AddAllocation(color);
	target=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float width:width height:height mipmapped:NO];target.storageMode=MTLStorageModePrivate;target.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;auto depth=[lease.Device() newTextureWithDescriptor:target];Check(depth!=nil,"Cannot allocate native oracle depth");lease.AddAllocation(depth);
	target=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatStencil8 width:width height:height mipmapped:NO];target.storageMode=MTLStorageModePrivate;target.usage=MTLTextureUsageRenderTarget;auto stencil=[lease.Device() newTextureWithDescriptor:target];Check(stencil!=nil,"Cannot allocate native oracle stencil");lease.AddAllocation(stencil);
	constexpr size_t depthRowBytes=512;auto depthBytes=[lease.Device() newBufferWithLength:depthRowBytes*height options:MTLResourceStorageModeShared];Check(depthBytes!=nil,"Cannot allocate actual private-depth readback buffer");lease.AddAllocation(depthBytes);
	WorldCamera camera;camera.view=camera.projection=Identity();camera.clipDepth=ClipDepthConvention::MinusOneToOne;camera.viewport={0,0,width,height};double compile=0;
	std::cerr<<"fixture native prepare\n";
	auto prepared=omit?std::unique_ptr<PreparedWorldParticles>{}:particles.PrepareLuaNano(lease,frame,camera,time,MTLPixelFormatRGBA8Unorm,MTLPixelFormatDepth32Float,MTLPixelFormatStencil8,true,compile);
	auto pass=[MTL4RenderPassDescriptor new];pass.renderTargetWidth=width;pass.renderTargetHeight=height;pass.colorAttachments[0].texture=color;pass.colorAttachments[0].loadAction=MTLLoadActionClear;pass.colorAttachments[0].storeAction=MTLStoreActionStore;pass.colorAttachments[0].clearColor=MTLClearColorMake(.02,.03,.04,1);
	pass.depthAttachment.texture=depth;pass.depthAttachment.loadAction=MTLLoadActionClear;pass.depthAttachment.storeAction=MTLStoreActionStore;pass.depthAttachment.clearDepth=clearDepth;pass.stencilAttachment.texture=stencil;pass.stencilAttachment.loadAction=MTLLoadActionClear;pass.stencilAttachment.storeAction=MTLStoreActionStore;pass.stencilAttachment.clearStencil=mask;
	lease.CommitResources();auto command=lease.Begin("Original NoGS typed instance/stencil comparison");std::cerr<<"fixture native dependencies\n";if(prepared)prepared->EncodeDependencies(command);auto encoder=[command renderCommandEncoderWithDescriptor:pass];Check(encoder!=nil,"Cannot create actual native particle encoder");[encoder setViewport:MTLViewport{0,0,width,height,0,1}];[encoder setScissorRect:MTLScissorRect{0,0,width,height}];std::cerr<<"fixture native encode\n";if(prepared)prepared->Encode(NativeWorldParticleStage::LuaNanoLaterWorld,encoder);[encoder endEncoding];
	auto copy=[command computeCommandEncoder];Check(copy!=nil,"Cannot encode actual private-depth readback");[copy barrierAfterQueueStages:MTLStageAll beforeStages:MTLStageBlit visibilityOptions:MTL4VisibilityOptionDevice];[copy copyFromTexture:depth sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1) toBuffer:depthBytes destinationOffset:0 destinationBytesPerRow:depthRowBytes destinationBytesPerImage:0];[copy endEncoding];
	std::cerr<<"fixture native submit\n";lease.SubmitAndWait(start,renders++);std::cerr<<"fixture native retired\n";if(prepared)prepared->DidComplete();double readMs=0;
	const auto result=lease.Readback(color,width,height,readMs);
	for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){float value;std::memcpy(&value,static_cast<const uint8_t*>(depthBytes.contents)+y*depthRowBytes+x*4,4);Check(value==clearDepth,"Original nano pass wrote to scene depth");}
	return result;
}
}
int main(int argc,char** argv)
{
	@autoreleasepool {try {
		Check(argc>=4,"usage: nano-gpu original-vs original-fs output.json [omit]");const auto vs=Read(argv[1]),fs=Read(argv[2]);Check(Hash(vs)==0x6680a1835b107bafull&&Hash(fs)==0x7527fb7d38d2a2bbull,"Not the exact game30770 original NoGS shader sources");
		std::cerr<<"fixture GL initialize\n";GLReference reference(vs,fs);std::cerr<<"fixture Metal initialize\n";WorldParticlesGPU particles;{RuntimeLease lease;double compile=0;particles.InitializeLuaNano(lease,MTLPixelFormatRGBA8Unorm,true,compile);}
		const bool omit=argc>4;unsigned maximum=0,pixelDifferences=0;std::ostringstream cases;
		for(unsigned shape:{0u,1u})for(bool glow:{false,true})for(float time:{30.f,97.f,100.f}) {
			auto frame=Frame(shape,time,glow);const unsigned mask=time==97?0x40:0;const float depth=time==97?.2f:.95f;
			std::cerr<<"fixture GL render "<<shape<<' '<<glow<<' '<<time<<'\n';const auto gl=reference.Render(frame,time,mask,depth);std::cerr<<"fixture Metal render\n";const auto metal=Native(particles,frame,time,mask,depth,omit);unsigned local=0,different=0;
			for(size_t n=0;n<gl.size();++n){const unsigned delta=std::abs(int(gl[n])-int(metal[n]));local=std::max(local,delta);if(delta)++different;}
			maximum=std::max(maximum,local);pixelDifferences+=different;
			cases<<"{\"shape\":"<<shape<<",\"glow\":"<<(glow?"true":"false")<<",\"time\":"<<time<<",\"maxChannelError\":"<<local<<",\"differentChannels\":"<<different<<"},";
			Check(local<=2,"Actual original NoGS GL/Metal pixels exceed bounded two-channel quantization tolerance");
		}
		std::string list=cases.str();if(!list.empty())list.pop_back();std::ofstream out(argv[3]);out<<"{\"status\":\"PASS\",\"checks\":"<<checks<<",\"renders\":"<<renders<<",\"maxChannelError\":"<<maximum<<",\"differentChannels\":"<<pixelDifferences<<",\"referenceRenderer\":\""<<glGetString(GL_RENDERER)<<"\",\"referenceVersion\":\""<<glGetString(GL_VERSION)<<"\",\"cases\":["<<list<<"]}\n";
		std::cout<<"PASS "<<renders<<" actual original NoGS GL/Metal renders / "<<checks<<" checks, max channel error "<<maximum<<'\n';return 0;
	}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
}
