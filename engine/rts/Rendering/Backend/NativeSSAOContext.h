/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSSAOData.h"
#include <functional>
namespace rendering {
struct NativeSSAOProgramState {
 NativeSSAOProgram kind=NativeSSAOProgram::Sample;
 NativeSSAODefines defines;
 NativeSSAOUniforms uniforms;
 bool deleted=false;
};
struct NativeSSAOFBO {uint64_t identity=0;std::shared_ptr<NativeSSAOImageCell>color;bool deleted=false;};
struct NativeSSAOEvents {
 std::function<void(const std::string&,const std::shared_ptr<NativeSSAOImageCell>&)> created;
 std::function<void(const std::string&)> deleted;
 std::function<void(unsigned,const std::string&)> bind;
 std::function<void(unsigned)> disable;
 std::function<NativeCUSTextureBinding(unsigned)> binding;
 // Prepares a genuine immutable publication, returns allocation-free rollback.
 // It must not advertise completed pixels; GPU Encode+DidComplete does that.
 std::function<std::function<void()>(const std::string&,std::shared_ptr<const NativeSSAOImageVersion>)> publish;
 std::function<std::pair<ScreenDrawState,NativeWorldDrawState>()> state;
 std::function<uint64_t()> sourceDrawFrame;
};
class NativeSSAOContext {
public:
 NativeSSAOContext(NativeSSAORecorder&,uint32_t maxDimension,NativeSSAOEvents);
 ~NativeSSAOContext();
 std::string CreateTexture(uint32_t,uint32_t,NativeSSAOFormat,NativeTextureSampling);
 void DeleteTexture(const std::string&);
 std::shared_ptr<NativeSSAOFBO>CreateFBO(const std::string&);
 void DeleteFBO(const std::shared_ptr<NativeSSAOFBO>&);
 uint64_t BindFBO(std::shared_ptr<NativeSSAOFBO>);
 void RestoreFBO(uint64_t);
 std::shared_ptr<NativeSSAOProgramState>CreateProgram(NativeSSAOProgram,NativeSSAODefines);
 void Activate(std::shared_ptr<NativeSSAOProgramState>);
 void Deactivate(const std::shared_ptr<NativeSSAOProgramState>&);
 void Finalize(const std::shared_ptr<NativeSSAOProgramState>&);
 void Uniform(const std::shared_ptr<NativeSSAOProgramState>&,std::string_view,std::span<const float>);
 void Texture(unsigned,const std::string&);
 void DisableTexture(unsigned);
 void Draw(std::array<std::array<float,4>,6>,std::shared_ptr<const NativeSSAOTileGeometry> tiles={});
 void Clear(std::array<float,4>);
 void Abort()noexcept{active.reset();target.reset();}
private:
 NativeSSAORecorder&recorder;uint32_t maxDimension;NativeSSAOEvents events;
 std::map<std::string,std::shared_ptr<NativeSSAOImageCell>,std::less<>>images;
 std::map<uint64_t,std::shared_ptr<NativeSSAOFBO>>fbos;
 std::shared_ptr<NativeSSAOFBO>target;
 std::shared_ptr<NativeSSAOProgramState>active;
 void Commit(NativeSSAODraw);
};
}
