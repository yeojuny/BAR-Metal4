/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeVolumetricCloudData.h"
#include "Rendering/Backend/NativePostprocessRecorder.h"
#include <functional>
#include <map>

namespace rendering {
struct NativeVolumetricCloudTextureOptions {
	NativePostprocessFormat format = NativePostprocessFormat::RGBA8;
	NativePostprocessSampler sampler;
	bool renderToTexture = false;
};
struct NativeVolumetricCloudProgramState {
	NativeVolumetricCloudProgram program;
	NativeVolumetricCloudUniforms uniforms;
	bool deleted = false;
};
class NativeVolumetricCloudRecorder {
public:
	void BeginFrame(std::uint64_t sourceDrawFrame);
	std::shared_ptr<const NativeVolumetricCloudFrame> EndFrame();
	// Clouds stay open through DrawScreenPost. A screen readback must capture
	// their real earlier draws without ending or resetting this recorder.
	std::shared_ptr<const NativeVolumetricCloudFrame> SnapshotPrefix(std::uint64_t cutoff) const;
	void Append(NativeVolumetricCloudDraw draw);
	void AbortFrame() noexcept;
	void RetireCompletedFrame(std::uint64_t sourceDrawFrame);
	void Reset() noexcept;
private:
	std::shared_ptr<NativeVolumetricCloudFrame> recording;
	std::shared_ptr<const NativeVolumetricCloudFrame> pending;
};
class NativeVolumetricCloudContext {
public:
	NativeVolumetricCloudContext(NativePostprocessRecorder& images, NativeVolumetricCloudRecorder& clouds, std::uint32_t maximumDimension);
	std::string CreateTexture(double width, double height, const NativeVolumetricCloudTextureOptions& options);
	void AliasTexture(std::string_view oldName, std::string commonName);
	void DeleteTexture(std::string_view name);
	std::shared_ptr<NativePostprocessBinding> GetTexture(std::string_view name) const;
	std::shared_ptr<NativeVolumetricCloudProgramState> CreateProgram(std::string_view vertex, std::string_view fragment);
	void DeleteProgram(const std::shared_ptr<NativeVolumetricCloudProgramState>& program);
	void UseProgram(std::shared_ptr<NativeVolumetricCloudProgramState> program);
	std::shared_ptr<NativeVolumetricCloudProgramState> CurrentProgram() const { return activeProgram; }
	void Uniform(std::string_view name, std::span<const float> values);
	void UniformMatrix(const std::array<float,16>& inverse);
	void BindDepth(std::string_view name);
	void BindVolume(std::shared_ptr<const NativeVolumetricCloudVolume> volume);
	// Fixed-function gl.Texture(unit,false) does not erase shader bindings.
	void DisableTexture(std::uint32_t unit) const;
	void CopyDepth(std::string_view name, NativePostprocessRect source, NativePostprocessRect destination, NativePostprocessStage stage);
	void BeginTarget(std::string_view name);
	void EndTarget();
	std::size_t TargetDepth() const noexcept { return targets.size(); }
	void TexRect(std::array<float,4> clip, std::array<float,4> uv, NativePostprocessStage stage);
private:
	NativePostprocessRecorder& images;
	NativeVolumetricCloudRecorder& clouds;
	std::uint32_t maximumDimension;
	std::map<std::string,std::shared_ptr<NativePostprocessBinding>,std::less<>> textures;
	std::shared_ptr<NativeVolumetricCloudProgramState> activeProgram;
	std::shared_ptr<NativePostprocessBinding> depthBinding;
	std::shared_ptr<const NativeVolumetricCloudVolume> volume;
	std::vector<std::shared_ptr<NativePostprocessBinding>> targets;
};
} // namespace rendering
