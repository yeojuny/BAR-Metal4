/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDOFContext.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rendering {
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::uint64_t Hash(std::string_view source)
{
	std::uint64_t value = 14695981039346656037ULL;
	for (unsigned char byte: source) { value ^= byte; value *= 1099511628211ULL; }
	return value;
}
}
NativeDOFContext::~NativeDOFContext()
{
 // The original LuaTextures owner deletes any surviving texture names on
 // handle destruction, independently of a widget's explicit Shutdown.
 if(textureEvents.deleted)for(const auto&[name,binding]:textures)textureEvents.deleted(binding);
}
void NativeDOFContext::SetTextureEvents(NativeDOFTextureEvents events)
{
 Check(textures.empty()&&bool(events.created)&&bool(events.deleted)&&bool(events.bound)&&bool(events.disabled),"DOF texture events must attach before original allocations");textureEvents=std::move(events);
}
std::string NativeDOFContext::CreateTexture(std::uint32_t width, std::uint32_t height, NativeDOFTextureOptions options)
{
	Check(maximumDimension && width<=maximumDimension && height<=maximumDimension,"Original DOF image exceeds actual native device dimensions");
	Check(textures.size() < 2048, "Original DOF image allocation limit exceeded");
	auto binding = CreateNativePostprocessBinding(width,height,options.format,options.sampler,options.renderToTexture,maximumDimension);
	const auto name = "!native-dof-" + std::to_string(binding->image->object);
	auto [entry,inserted]=textures.emplace(name,binding);
 try {if(textureEvents.created)textureEvents.created(binding,name);}
 catch(...) {textures.erase(entry);throw;}
 return name;
}
std::shared_ptr<NativePostprocessBinding> NativeDOFContext::GetTexture(std::string_view name) const
{
	auto found = textures.find(name); Check(found != textures.end() && !found->second->deleted, "Unknown/deleted private DOF image"); return found->second;
}
void NativeDOFContext::DeleteTexture(std::string_view name)
{
	if (name.empty()) return; // Original DeleteTexture(nil) makes no GL call.
	auto binding = GetTexture(name);if(textureEvents.deleted)textureEvents.deleted(binding); binding->deleted = true; binding->current.reset(); textures.erase(std::string(name));
	// An already-recorded immutable version remains owned by its frame packet.
}
std::shared_ptr<NativeDOFFBO> NativeDOFContext::CreateFBO(std::span<const std::string> names)
{
	Check(!names.empty() && names.size() <= 4, "Original DOF requires one to four real color attachments");
	auto fbo = std::make_shared<NativeDOFFBO>();
	for (const auto& name: names) {
		auto image = GetTexture(name); Check(!IsNativePostprocessDepth(image->image->format), "DOF depth cannot masquerade as color");
		if (!fbo->attachments.empty()) Check(fbo->attachments[0]->image->width == image->image->width && fbo->attachments[0]->image->height == image->image->height,
			"Original DOF MRT attachment sizes differ");
		Check(std::find(fbo->attachments.begin(), fbo->attachments.end(), image) == fbo->attachments.end(), "Duplicate DOF FBO image");
		fbo->attachments.push_back(std::move(image));
	}
	return fbo;
}
void NativeDOFContext::DeleteFBO(const std::shared_ptr<NativeDOFFBO>& fbo)
{
	if (!fbo) return; // Original LuaFBOs::DeleteFBO explicitly accepts nil.
	if (activeFBO == fbo) activeFBO.reset();
	fbo->deleted = true; fbo->attachments.clear();
}
std::shared_ptr<NativeDOFProgram> NativeDOFContext::CreateProgram(std::string_view vertex, std::string_view fragment,
	bool zeroToOneDepth, std::int32_t pass, std::int32_t quality)
{
	Check(IsNativeDOFConsumerAvailable(),"Original DOF native shader consumer has not compiled successfully");
	// Complete fixed31657 paired VS/FS; the vertex stage really samples depth
	// and computes the flat focus/aperture used by the filter fragment stage.
	Check(vertex.size()==4362 && Hash(vertex)==0x372a6aee81fb669cULL,"Original31657 DOF vertex source identity changed");
	Check(fragment.size()==18360 && Hash(fragment)==0x1003997de988310dULL,"Original31657 DOF fragment source identity changed");
	Check(pass>=0 && pass<=7 && (quality==1 || (pass==5 && quality==0)),"Original DOF compiled pass/quality changed");
	auto program=std::make_shared<NativeDOFProgram>();
	program->uniforms.zeroToOneDepth=zeroToOneDepth;program->uniforms.pass=pass;program->uniforms.quality=quality;
	return program;
}

void NativeDOFContext::DeleteProgram(const std::shared_ptr<NativeDOFProgram>& program)
{
	if (!program) return;
	if (activeProgram == program) activeProgram.reset(); program->deleted = true;
}
void NativeDOFContext::UseProgram(std::shared_ptr<NativeDOFProgram> program)
{
	Check(!program || !program->deleted, "Deleted native DOF program"); activeProgram = std::move(program);
}
void NativeDOFContext::Uniform(std::string_view name, std::span<const float> values)
{
	Check(activeProgram && !activeProgram->deleted, "DOF uniform outside actual program activation");
	for (float value: values) Check(std::isfinite(value), "Nonfinite original DOF uniform");
	auto& u = activeProgram->uniforms;
	auto scalar = [&](float& out) { Check(values.size() == 1, "DOF scalar arity changed"); out = values[0]; };
	auto integer = [&](std::int32_t& out) { Check(values.size() == 1 && values[0] >= 0 && values[0] <= 1 && std::trunc(values[0]) == values[0], "DOF integer value changed"); out = std::int32_t(values[0]); };
	auto pair = [&](auto& out) { Check(values.size() == out.size(), "DOF vector arity changed"); std::copy(values.begin(), values.end(), out.begin()); };
	if (name == "resolution") pair(u.resolution);
	else if (name == "mouseDepthCoord") pair(u.mouseDepthCoordinate);
	else if (name == "autofocusFudgeFactor") scalar(u.autofocusFudgeFactor);
	else if (name == "autofocusPower") scalar(u.autofocusPower);
	else if (name == "autofocusFocalLength") scalar(u.autofocusFocalLength);
	else if (name == "manualFocusDepth") scalar(u.manualFocusDepth);
	else if (name == "fStop") scalar(u.fStop);
	else if (name == "autofocus") integer(u.autofocus);
	else if (name == "mousefocus") integer(u.mousefocus);
	else throw std::runtime_error("Unknown original DOF uniform");
}
void NativeDOFContext::Projection(std::array<float, 16> matrix)
{
	Check(activeProgram && !activeProgram->deleted && activeProgram->uniforms.pass==0, "DOF projection outside active filter program");
	for (float value: matrix) Check(std::isfinite(value), "Nonfinite original DOF projection");
	activeProgram->uniforms.projection = matrix;
}
void NativeDOFContext::Texture(std::uint32_t unit, std::string_view name)
{
	Check(unit < bindings.size(), "Original DOF texture unit exceeds known shader signature");auto image=GetTexture(name);
 const auto actual=textureEvents.bound?textureEvents.bound(unit,image):unit;if(actual<bindings.size())bindings[actual]=std::move(image);
}
void NativeDOFContext::Unbind(std::uint32_t unit)
{
	Check(unit < bindings.size(), "Original DOF texture unit exceeds known shader signature"); // GL Texture(false) disables fixed-function texturing, but leaves the
	// real image bound. Original GLSL sampler reads ignore that enable bit.
 if(textureEvents.disabled)textureEvents.disabled(unit);
}
void NativeDOFContext::Copy(std::string_view name, NativePostprocessRect source, NativePostprocessRect destination, std::uint32_t offset)
{
	auto binding = GetTexture(name);
 const auto actual=textureEvents.bound?textureEvents.bound(0,binding):0;
 if(actual<bindings.size())bindings[actual]=binding;
 // The original bind/enable occurs before validating/executing the copy.
 if(!source.width||!source.height)return;
	recorder.Copy(binding, IsNativePostprocessDepth(binding->image->format) ? NativePostprocessOperationKind::CopyDepth : NativePostprocessOperationKind::CopyColor,
		source, destination, offset);
	// CopyToTexture leaves that actual inherited unit bound and enabled.
}
std::shared_ptr<NativeDOFFBO> NativeDOFContext::BindFBO(std::shared_ptr<NativeDOFFBO> fbo)
{
	Check(!fbo || (!fbo->deleted && !fbo->attachments.empty()),"Invalid/deleted original DOF framebuffer");
	auto previous=activeFBO;activeFBO=std::move(fbo);return previous;
}
void NativeDOFContext::Viewport(NativePostprocessRect rectangle)
{
	Check(rectangle.width<=maximumDimension && rectangle.height<=maximumDimension,"Original DOF viewport exceeds device limits");
	viewport=rectangle;
}
void NativeDOFContext::TexRect(std::array<float,4> bounds,std::array<float,4> uv,std::uint32_t offset,std::uint32_t width,std::uint32_t height)
{
	Check(activeProgram && !activeProgram->deleted && width && height && viewport,"DOF draw requires active program, explicit viewport and logical screen basis");
	const auto& uniforms=activeProgram->uniforms;
	NativePostprocessQuad quad;quad.clip=bounds;quad.uv=uv;
	// Original dof.vs bypasses ftransform only in FINAL_BLUR and FINAL_NEAR_BLUR.
	if (uniforms.pass!=2 && uniforms.pass!=4)
		quad.clip={2*bounds[0]/width-1,2*bounds[1]/height-1,2*bounds[2]/width-1,2*bounds[3]/height-1};
	std::array<std::shared_ptr<const NativePostprocessVersion>,6> inputs{};std::array<bool,6> enabled{};
	const auto mask=NativeDOFInputMask(uniforms.pass,uniforms.quality);
	for (std::size_t i=0;i<bindings.size();++i) if(mask&(1u<<i)) {
		Check(bindings[i] && !bindings[i]->deleted,"Original DOF active sampler has no live bound image");
		inputs[i]=bindings[i]->current;enabled[i]=true;
	}
	const std::vector<std::shared_ptr<NativePostprocessBinding>> empty;
	Check(!activeFBO || !activeFBO->deleted,"DOF draw uses a deleted target");
	recorder.Draw(activeFBO?activeFBO->attachments:empty,inputs,enabled,uniforms,quad,*viewport,offset,!activeFBO);
}
} // namespace rendering
