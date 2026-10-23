/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeShadowTextureSource.h"
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
constexpr auto Image2D=NativeCUSTextureTarget::Image2D;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool DepthFormat(uint32_t value){return value==0x81A5||value==0x81A6||value==0x81A7||value==0x8CAC;}
}
NativeShadowTextureSource& OriginalNativeShadowTextures(){static NativeShadowTextureSource source;return source;}
void NativeShadowTextureSource::Attach(NativeTextureRegistry& r,NativeCUSTextureHistory& h,NativeShadowTextureServices s)
{
	Check(!Attached()&&r.ContextGeneration()&&r.MatchesHistory(h)&&s.ready&&s.allocate&&s.checkFramebuffer&&s.sourceDrawFrame&&s.depthBufferBits&&s.capture,
		"Original shadow source has no actual context, allocation provider or pass graph");
	registry=&r;history=&h;services=std::move(s);
}
void NativeShadowTextureSource::Detach(){registry=nullptr;history=nullptr;services={};depthName=0;}
bool NativeShadowTextureSource::Ready()const{return Attached()&&services.ready();}
int NativeShadowTextureSource::SupportedDepthBits()const
{
	Check(Ready(),"Native shadow depth capability queried before its actual graph exists");
	const int bits=services.depthBufferBits();
	Check(bits==16||bits==24||bits==32,"Actual native shadow depth support is not a recognized source format");return bits;
}
void NativeShadowTextureSource::Bind(uint32_t name)
{
	Check(Attached()&&history->ActiveUnit().has_value(),"Original shadow bind active texture unit is unobserved");
	registry->Bind(*history->ActiveUnit(),name,Image2D);
}
void NativeShadowTextureSource::Delete(uint32_t name)
{
	if(!name)return;
	Check(Attached(),"Original shadow delete outlived its actual context");
	registry->Delete(name);if(depthName==name)depthName=0;
}
std::shared_ptr<const NativeShadowTextureBacking> NativeShadowTextureSource::Image(uint32_t name)const
{
	Check(Attached(),"Original shadow context is detached");
	const auto object=registry->Find(name);
	Check(object&&object->target==Image2D,"Original shadow texture name is not live");
	auto backing=std::dynamic_pointer_cast<const NativeShadowTextureBacking>(object->backing);
	Check(backing&&backing->resource,"Original shadow texture has no actual native resource");
	return backing;
}
uint32_t NativeShadowTextureSource::Create(const NativeShadowTextureRequest& request,bool compatible)
{
	auto object=registry->Create(Image2D,request.kind==NativeShadowTextureKind::Depth?"original ShadowHandler depth":"original ShadowHandler color");
	Check(object->name<=std::numeric_limits<uint32_t>::max(),"Original shadow texture name exceeds GLuint");
	const auto name=uint32_t(object->name);
	try {
		Bind(name);
		NativeTextureSampling sampling;
		sampling.borderColor={1,1,1,1};sampling.wrapS=sampling.wrapT=compatible?0x812F:0x812D;
		sampling.minFilter=sampling.magFilter=compatible?0x2600:0x2601;
		sampling.baseLevel=sampling.maxLevel=0;
		if(request.kind==NativeShadowTextureKind::Color)
			sampling.swizzle=request.colored?std::array<unsigned,4>{0x1903,0x1904,0x1905,1}:std::array<unsigned,4>{0x1903,0x1903,0x1903,1};
		registry->SetSampling(name,sampling);
		registry->Allocate(name,{request.size,request.size,1,1,1,request.originalFormat});
		auto image=std::make_shared<NativeShadowTextureBacking>();image->request=request;image->resource=services.allocate(request);
		Check(bool(image->resource),"Actual original shadow texture allocation failed");
		image->storageGeneration=object->identity;image->contentVersion=registry->Find(name)->version+1;
		image->sourceDrawFrame=services.sourceDrawFrame();
		// InitFBO clears both actual attachments even with Shadows=0. This is
		// a pending real clear, not an already completed white/default image.
		image->content=NativeTextureContent::ProducerVersion;
		registry->PublishContent(name,{},image);Bind(0);
	}catch(...){registry->Delete(name);throw;}
	return name;
}
bool NativeShadowTextureSource::Initialize(uint32_t& depth,uint32_t& color,uint32_t size,uint32_t depthFormat,bool colored)
{
	Check(Ready()&&size&&DepthFormat(depthFormat),"Original shadow allocation has an incomplete native graph or invalid extent/format");
	bool complete=false;
	for(bool compatible:{false,true}){
		Delete(depth);depth=0;
		depth=Create({NativeShadowTextureKind::Depth,size,depthFormat,colored},compatible);
		Delete(color);color=0;
		color=Create({NativeShadowTextureKind::Color,size,colored?0x8051u:0x8229u,colored},compatible);
		complete=services.checkFramebuffer(*Image(depth),*Image(color),compatible);
		if(complete)break;
	}
	depthName=depth;
	return complete;
}
void NativeShadowTextureSource::SamplerRaw(bool compare)
{
	Check(Attached()&&history->ActiveUnit().has_value(),"Original shadow sampler active unit is unobserved");
	const auto unit=*history->ActiveUnit();const auto binding=history->Binding(unit,Image2D);
	Check(binding.knowledge!=NativeCUSBindingKnowledge::Unobserved,"Original shadow sampler binding is unobserved");
	auto sampling=binding.object?binding.object->sampling:binding.defaultSampling;
	sampling.compareMode=compare?0x884E:0;sampling.depthMode=0x1909;
	if(compare)sampling.compareFunction=0x0203;
	registry->SetBoundSampling(unit,Image2D,sampling);
}
void NativeShadowTextureSource::SetupSamplerRaw(){SamplerRaw(true);}
void NativeShadowTextureSource::ResetSamplerRaw(){SamplerRaw(false);}
void NativeShadowTextureSource::SetupSampler(unsigned unit,bool enable)
{
	Check(Attached(),"Original shadow sampler has no actual context");history->ActiveTexture(unit);Bind(depthName);
	if(enable)history->Enable(unit,Image2D,true);SetupSamplerRaw();
}
void NativeShadowTextureSource::ResetSampler(unsigned unit,bool disable)
{
	Check(Attached(),"Original shadow sampler has no actual context");history->ActiveTexture(unit);Bind(0);
	if(disable)history->Enable(unit,Image2D,false);ResetSamplerRaw();
}
NativeShadowTextureWrite NativeShadowTextureSource::Advance(uint32_t name,NativeShadowTextureOperation operation)
{
	NativeShadowTextureWrite write;write.previous=registry->Find(name);const auto previous=Image(name);
	auto image=std::make_shared<NativeShadowTextureBacking>();image->request=previous->request;image->operation=operation;
	image->resource=services.allocate(image->request);Check(bool(image->resource),"Actual shadow write has no native destination");
	image->storageGeneration=write.previous->identity;image->contentVersion=write.previous->version+1;
	image->sourceDrawFrame=services.sourceDrawFrame();image->content=NativeTextureContent::ProducerVersion;
	registry->PublishContent(name,{},image);write.current=registry->Find(name);write.backing=std::move(image);return write;
}
std::shared_ptr<const NativeShadowTextureFrame> NativeShadowTextureSource::BeginFrame(uint32_t depth,uint32_t color,bool sunPositive)
{
	Check(Ready()&&depth==depthName,"Original shadow frame has no installed source graph");
	Check(Image(depth)->request.kind==NativeShadowTextureKind::Depth&&Image(color)->request.kind==NativeShadowTextureKind::Color,
		"Original shadow attachments were exchanged");
	auto frame=std::make_shared<NativeShadowTextureFrame>();frame->sourceDrawFrame=services.sourceDrawFrame();frame->redrawColor=sunPositive;
	try {
		frame->depth=Advance(depth,NativeShadowTextureOperation::DepthPass);
		if(sunPositive)frame->color=Advance(color,NativeShadowTextureOperation::ColorPass);
		else frame->retainedColor=Image(color);
	}catch(...){Cancel(*frame);throw;}
	return frame;
}
void NativeShadowTextureSource::Cancel(const NativeShadowTextureFrame& frame)noexcept
{
	if(!registry)return;
	if(frame.color.current)registry->CancelContentPublication(frame.color.current,frame.color.previous);
	if(frame.depth.current)registry->CancelContentPublication(frame.depth.current,frame.depth.previous);
}
void NativeShadowTextureSource::Capture(uint32_t depth,uint32_t color,bool sunPositive)
{
	auto frame=BeginFrame(depth,color,sunPositive);
	try {services.capture(frame);}catch(...){Cancel(*frame);throw;}
}
} // namespace rendering
