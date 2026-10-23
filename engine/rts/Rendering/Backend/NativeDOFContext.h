/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativePostprocessRecorder.h"
#include <map>
#include <string>
#include <string_view>

namespace rendering {
struct NativeDOFTextureOptions {
	NativePostprocessFormat format = NativePostprocessFormat::RGBA8;
	NativePostprocessSampler sampler;
	bool renderToTexture = false;
};
struct NativeDOFProgram {
	NativeDOFUniforms uniforms;
	bool deleted = false;
};
struct NativeDOFFBO {
	std::vector<std::shared_ptr<NativePostprocessBinding>> attachments;
	bool deleted = false;
};
struct NativeDOFTextureEvents {
 std::function<void(const std::shared_ptr<NativePostprocessBinding>&,const std::string&)> created;
 std::function<void(const std::shared_ptr<NativePostprocessBinding>&)> deleted;
 std::function<unsigned(unsigned,const std::shared_ptr<NativePostprocessBinding>&)> bound;
 std::function<void(unsigned)> disabled;
};
class NativeDOFContext {
public:
	explicit NativeDOFContext(NativePostprocessRecorder& recorder, std::uint32_t maximumDimension = 32768): recorder(recorder), maximumDimension(maximumDimension) {}
	~NativeDOFContext();
 void SetTextureEvents(NativeDOFTextureEvents events);
	std::string CreateTexture(std::uint32_t width, std::uint32_t height, NativeDOFTextureOptions options);
	void DeleteTexture(std::string_view name);
	std::shared_ptr<NativePostprocessBinding> GetTexture(std::string_view name) const;
	std::shared_ptr<NativeDOFFBO> CreateFBO(std::span<const std::string> textures);
	void DeleteFBO(const std::shared_ptr<NativeDOFFBO>& fbo);
	std::shared_ptr<NativeDOFProgram> CreateProgram(std::string_view vertex, std::string_view fragment, bool zeroToOneDepth, std::int32_t pass, std::int32_t quality);
	void DeleteProgram(const std::shared_ptr<NativeDOFProgram>& program);
	void UseProgram(std::shared_ptr<NativeDOFProgram> program);
	std::shared_ptr<NativeDOFProgram> CurrentProgram() const { return activeProgram; }
	void Uniform(std::string_view name, std::span<const float> values);
	void Projection(std::array<float, 16> matrix);
	void Texture(std::uint32_t unit, std::string_view name);
	// Original gl.Texture(unit,false): disable without unbinding shader image.
	void Unbind(std::uint32_t unit);
	void Copy(std::string_view name, NativePostprocessRect source, NativePostprocessRect destination, std::uint32_t commandOffset);
	// Raw binding preserves viewport state and returns the actual previous
	// private target. A null target is the original root screen framebuffer.
	std::shared_ptr<NativeDOFFBO> BindFBO(std::shared_ptr<NativeDOFFBO> fbo);
	void Viewport(NativePostprocessRect rectangle);
	void TexRect(std::array<float, 4> bounds, std::array<float, 4> uv, std::uint32_t commandOffset,
		std::uint32_t screenWidth, std::uint32_t screenHeight);
	std::size_t TextureCount() const noexcept { return textures.size(); }
private:
	NativePostprocessRecorder& recorder;
 NativeDOFTextureEvents textureEvents;
	std::uint32_t maximumDimension;
	std::map<std::string, std::shared_ptr<NativePostprocessBinding>, std::less<>> textures;
	std::shared_ptr<NativeDOFProgram> activeProgram;
	std::array<std::shared_ptr<NativePostprocessBinding>, 6> bindings{};
	std::shared_ptr<NativeDOFFBO> activeFBO;
	std::optional<NativePostprocessRect> viewport;
};
} // namespace rendering
