/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBloomData.h"
#include <functional>
namespace rendering {
struct NativeBloomProgramState {
 NativeBloomProgram kind=NativeBloomProgram::Bright;
 NativeBloomDefines defines;
 NativeBloomUniforms uniforms;
 std::map<int,std::string> uniformLocations;
 bool deleted=false;
};
struct NativeBloomFBO {uint64_t identity=0;std::shared_ptr<NativeBloomImageCell>color;bool deleted=false;};
struct NativeBloomEvents {
 std::function<void(const std::string&,const std::shared_ptr<NativeBloomImageCell>&)> created;
 std::function<void(const std::string&)> deleted;
 std::function<void(unsigned,const std::string&)> bind;
 std::function<void(unsigned)> disable;
 std::function<NativeCUSTextureBinding(unsigned)> binding;
 // Prepares a genuine immutable publication, returns allocation-free rollback.
 // It must not advertise completed pixels; GPU Encode+DidComplete does that.
 std::function<std::function<void()>(const std::string&,std::shared_ptr<const NativeBloomImageVersion>)> publish;
 std::function<std::pair<ScreenDrawState,NativeWorldDrawState>()> state;
 std::function<uint64_t()> sourceDrawFrame;
};
class NativeBloomContext {
public:
 NativeBloomContext(NativeBloomRecorder&,uint32_t maxDimension,NativeBloomEvents);
 ~NativeBloomContext();
 std::string CreateTexture(uint32_t,uint32_t,NativeBloomFormat,NativeTextureSampling);
 void DeleteTexture(const std::string&);
 std::shared_ptr<NativeBloomFBO>CreateFBO(const std::string&);
 void DeleteFBO(const std::shared_ptr<NativeBloomFBO>&);
 uint64_t BindFBO(std::shared_ptr<NativeBloomFBO>);
 void RestoreFBO(uint64_t);
 std::shared_ptr<NativeBloomProgramState>CreateProgram(NativeBloomProgram,NativeBloomDefines);
 void Activate(std::shared_ptr<NativeBloomProgramState>);
 void Deactivate(const std::shared_ptr<NativeBloomProgramState>&);
 void Finalize(const std::shared_ptr<NativeBloomProgramState>&);
 void Uniform(const std::shared_ptr<NativeBloomProgramState>&,std::string_view,std::span<const float>);
 int GetUniformLocation(const std::shared_ptr<NativeBloomProgramState>&,std::string_view)const;
 void UniformCurrent(int,std::span<const float>);
 void Texture(unsigned,const std::string&);
 void DisableTexture(unsigned);
 void Draw(std::array<std::array<float,4>,6>);
 std::shared_ptr<const NativeBloomImage> Image(const std::string&)const;
 void Abort()noexcept{active.reset();target.reset();}
private:
 NativeBloomRecorder&recorder;uint32_t maxDimension;NativeBloomEvents events;
 std::map<std::string,std::shared_ptr<NativeBloomImageCell>,std::less<>>images;
 std::map<uint64_t,std::shared_ptr<NativeBloomFBO>>fbos;
 std::shared_ptr<NativeBloomFBO>target;
 std::shared_ptr<NativeBloomProgramState>active;
 void Commit(NativeBloomDraw);
};
}
