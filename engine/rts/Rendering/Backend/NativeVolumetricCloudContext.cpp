/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeVolumetricCloudContext.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace rendering {
namespace {
void Check(bool valid, const char* reason)
{
	if (!valid) throw std::runtime_error(std::string("Original volumetric-cloud context: ") + reason);
}
}
void NativeVolumetricCloudRecorder::BeginFrame(std::uint64_t frame)
{
	Check(!recording, "previous cloud frame is still recording");
	auto candidate = std::make_shared<NativeVolumetricCloudFrame>();
	candidate->sourceDrawFrame = frame;
	pending.reset(); recording = std::move(candidate);
}
std::shared_ptr<const NativeVolumetricCloudFrame> NativeVolumetricCloudRecorder::EndFrame()
{
	Check(recording != nullptr, "cloud frame is not recording");
	pending = std::move(recording); return pending;
}
std::shared_ptr<const NativeVolumetricCloudFrame> NativeVolumetricCloudRecorder::SnapshotPrefix(std::uint64_t cutoff) const
{
	Check(recording && cutoff,"cloud prefix requires its active actual source frame and cutoff");
	auto prefix=std::make_shared<NativeVolumetricCloudFrame>(*recording);
	std::erase_if(prefix->draws,[&](const auto& draw){return draw.sequence>=cutoff;});
	return prefix;
}
void NativeVolumetricCloudRecorder::Append(NativeVolumetricCloudDraw draw)
{
	Check(recording != nullptr, "cloud draw requires the original active frame");
	Check(draw.sequence != 0 && (recording->draws.empty() || draw.sequence > recording->draws.back().sequence), "cloud draw order is invalid");
	recording->draws.push_back(std::move(draw));
}
void NativeVolumetricCloudRecorder::AbortFrame() noexcept { recording.reset(); pending.reset(); }
void NativeVolumetricCloudRecorder::RetireCompletedFrame(std::uint64_t frame)
{
	Check(pending && pending->sourceDrawFrame == frame, "completed cloud frame identity differs");
	pending.reset();
}
void NativeVolumetricCloudRecorder::Reset() noexcept { AbortFrame(); }
NativeVolumetricCloudContext::NativeVolumetricCloudContext(NativePostprocessRecorder& images, NativeVolumetricCloudRecorder& clouds, std::uint32_t limit):
	images(images), clouds(clouds), maximumDimension(limit)
{}
std::string NativeVolumetricCloudContext::CreateTexture(double width, double height, const NativeVolumetricCloudTextureOptions& options)
{
	Check(options.format == NativePostprocessFormat::RGBA8 || options.format == NativePostprocessFormat::Depth24Unorm, "texture format is outside the verified original family");
	Check(options.sampler.wrapS != NativePostprocessWrap::LegacyClamp && options.sampler.wrapT != NativePostprocessWrap::LegacyClamp, "clouds do not request legacy border wrapping");
	const auto w = NativeVolumetricCloudTextureExtent(width,maximumDimension), h = NativeVolumetricCloudTextureExtent(height,maximumDimension);
	auto binding = CreateNativePostprocessBinding(w,h,options.format,options.sampler,options.renderToTexture,maximumDimension);
	// Allocation-only storage is originally undefined; it is not a produced
	// generation0, an invented clear image, or an available depth-copy result.
	const auto name = "native-cloud-image:" + std::to_string(binding->image->object);
	textures.emplace(name,std::move(binding)); return name;
}
void NativeVolumetricCloudContext::DeleteTexture(std::string_view name)
{
	const auto found = textures.find(name);
	if (found == textures.end()) return; // Original deletion of an absent name is harmless.
	found->second->deleted = true; textures.erase(found);
}
void NativeVolumetricCloudContext::AliasTexture(std::string_view oldName, std::string commonName)
{
	Check(!commonName.empty() && !textures.contains(commonName), "common image name is empty or already owned");
	const auto previous = textures.find(oldName);
	Check(previous != textures.end(), "image alias has no owned original binding");
	textures.emplace(std::move(commonName),previous->second); textures.erase(previous);
}
std::shared_ptr<NativePostprocessBinding> NativeVolumetricCloudContext::GetTexture(std::string_view name) const
{
	const auto found = textures.find(name);
	Check(found != textures.end() && !found->second->deleted, "texture name has no live owned image");
	return found->second;
}
std::shared_ptr<NativeVolumetricCloudProgramState> NativeVolumetricCloudContext::CreateProgram(std::string_view vertex, std::string_view fragment)
{
	auto result = std::make_shared<NativeVolumetricCloudProgramState>();
	result->program = ParseNativeVolumetricCloudProgram(vertex,fragment);
	return result;
}
void NativeVolumetricCloudContext::DeleteProgram(const std::shared_ptr<NativeVolumetricCloudProgramState>& program)
{
	if (!program) return;
	program->deleted = true; if (activeProgram == program) activeProgram.reset();
}
void NativeVolumetricCloudContext::UseProgram(std::shared_ptr<NativeVolumetricCloudProgramState> program)
{
	Check(!program || !program->deleted, "cannot activate a deleted original program");
	activeProgram = std::move(program);
}
void NativeVolumetricCloudContext::Uniform(std::string_view name, std::span<const float> values)
{
	Check(activeProgram && !activeProgram->deleted, "uniform requires the active original cloud program");
	Check(std::all_of(values.begin(),values.end(),[](float x) { return std::isfinite(x); }), "uniform contains nonfinite values");
	auto& u = activeProgram->uniforms;
	if (name == "time") { Check(values.size() == 1,"time uniform arity differs"); u.time = values[0]; return; }
	Check(values.size() == 3, "vector uniform arity differs");
	std::array<float,3>* output = name == "eyePos" ? &u.eye : name == "offset" ? &u.offset : name == "sundir" ? &u.sunDirection : name == "suncolor" ? &u.sunColor : nullptr;
	Check(output != nullptr, "unknown original cloud uniform"); std::copy_n(values.begin(),3,output->begin());
}
void NativeVolumetricCloudContext::UniformMatrix(const std::array<float,16>& inverse)
{
	Check(activeProgram && !activeProgram->deleted, "matrix requires the active original cloud program");
	Check(std::all_of(inverse.begin(),inverse.end(),[](float x) { return std::isfinite(x); }), "inverse projection contains nonfinite values");
	activeProgram->uniforms.viewProjectionInverse = inverse;
}
void NativeVolumetricCloudContext::BindDepth(std::string_view name)
{
	auto candidate = GetTexture(name);
	Check(candidate->image->format == NativePostprocessFormat::Depth24Unorm, "shader unit0 requires the actual depth24 copy image");
	depthBinding = std::move(candidate);
}
void NativeVolumetricCloudContext::BindVolume(std::shared_ptr<const NativeVolumetricCloudVolume> resource)
{
	Check(resource != nullptr, "shader unit2 requires the actual original Worley volume"); volume = std::move(resource);
}
void NativeVolumetricCloudContext::DisableTexture(std::uint32_t unit) const
{
	Check(unit <= 2, "shader texture unit differs from the original cloud family");
	// Deliberately retain the actual shader sampler binding.
}
void NativeVolumetricCloudContext::CopyDepth(std::string_view name, NativePostprocessRect source, NativePostprocessRect destination, NativePostprocessStage stage)
{
	Check(stage == NativePostprocessStage::PreUnit, "cloud depth copy must observe its actual PreUnit position");
	auto target = GetTexture(name);
	Check(target->image->format == NativePostprocessFormat::Depth24Unorm, "copy destination is not the original depth24 image");
	images.Copy(std::move(target),NativePostprocessOperationKind::CopyDepth,source,destination,0,stage,NextNativeWorldDrawSequence());
}
void NativeVolumetricCloudContext::BeginTarget(std::string_view name)
{
	auto target = GetTexture(name);
	Check(target->image->format == NativePostprocessFormat::RGBA8 && target->image->renderToTexture, "target is not the original color FBO");
	targets.push_back(std::move(target));
}
void NativeVolumetricCloudContext::EndTarget()
{
	Check(!targets.empty(), "render-to-texture stack underflow"); targets.pop_back();
}
void NativeVolumetricCloudContext::TexRect(std::array<float,4> clip, std::array<float,4> uv, NativePostprocessStage stage)
{
	Check(stage == NativePostprocessStage::PreUnit && !targets.empty(), "cloud shader quad requires its original PreUnit FBO scope");
	Check(activeProgram && !activeProgram->deleted && depthBinding && depthBinding->current && volume, "original cloud shader inputs are incomplete");
	Check(clip == std::array<float,4>{-1,-1,1,1} && uv == std::array<float,4>{0,0,1,1}, "cloud shader requires its original fullscreen TexRect");
	ValidateNativeVolumetricCloudProgram(activeProgram->program); ValidateNativeVolumetricCloudUniforms(activeProgram->uniforms);
	NativeVolumetricCloudDraw draw;
	draw.sequence = NextNativeWorldDrawSequence(); draw.depth = depthBinding->current;
	draw.previous = targets.back()->current; draw.volume = volume;
	draw.program = activeProgram->program; draw.uniforms = activeProgram->uniforms;
	const std::array inputs{draw.depth};
	images.PublishExternalOutput(targets.back(),stage,draw.sequence,NativeExternalImageFamily::VolumetricCloud,inputs,
		[&](std::shared_ptr<const NativePostprocessVersion> proposed) { draw.output = std::move(proposed); clouds.Append(std::move(draw)); });
}
} // namespace rendering
