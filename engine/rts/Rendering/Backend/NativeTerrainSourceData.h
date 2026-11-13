/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeCUSTextureHistory.h"
#include "NativeTextureRegistry.h"
#include <array>
#include <stdexcept>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Shader { struct IProgramObject; }
namespace rendering {
struct WorldTerrainTiles;

// Selected by the original SMF owner, never inferred from configuration or
// WorldTerrainMaterial::advancedShading. Lua and deferred are distinct programs.
enum class NativeTerrainProgramSelection {
	ForwardStandard, ForwardAdvanced, DeferredAdvanced, LuaForward, LuaDeferred,
	CombinedAdvanced, // Append: preserve the existing 0..4 source identities.
	Border, // The actual original SMFBorder program, never a forward mesh substitute.
	Composite // Original SMFComposite full-screen source, with color and depth outputs.
};
struct NativeTerrainProgramIdentity {
	uint64_t contextGeneration=0, originalProgramGeneration=0, sourceRevision=0;
	const Shader::IProgramObject* originalProgram=nullptr; // Provenance, never dereferenced here.
	uint32_t originalGLName=0; // Provenance only, never a native logical identity.
	std::string originalName;
	NativeTerrainProgramSelection selection=NativeTerrainProgramSelection::ForwardStandard;
	uint64_t vertexSourceHash=0, fragmentSourceHash=0;
	std::vector<std::pair<std::string,std::string>> defines; // Exact original compile variant.
	bool operator==(const NativeTerrainProgramIdentity&) const = default;
};

// Implemented by the actual renderer library/PSO owner. This type cannot be
// instantiated as a readiness token. A model/CUS owner cannot be substituted.
// Each consumer instance owns ONE immutable library generation; recompilation
// creates another consumer, never retargets an already retained instance.
// The owner retains its library generation through the enclosing source frame
// and common lease. Matches identifies the exact original program lifetime and
// source/defines; Supports proves this compiled consumer implements that variant.
class NativeTerrainProgramConsumer {
public:
	virtual ~NativeTerrainProgramConsumer()=default;
	virtual uint64_t ContextGeneration() const noexcept=0;
	virtual uint64_t LibraryGeneration() const noexcept=0;
	virtual bool Matches(const NativeTerrainProgramIdentity&) const noexcept=0;
	virtual bool Supports(const NativeTerrainProgramIdentity&) const noexcept=0;
 // Opt-in only for owners whose Matches/Supports cannot change within a sealed
 // library generation. Generic/imported owners retain full live validation.
 virtual bool HasImmutableDefinition() const noexcept {return false;}
};
void ValidateNativeTerrainProgram(const NativeTerrainProgramIdentity&,
	const std::shared_ptr<const NativeTerrainProgramConsumer>&,uint64_t expectedContext);

// Shared immutable definition. Only the validating factory can issue a link
// certificate. Public construction remains unvalidated for unsupported states.
struct NativeTerrainProgramDefinition {
 const NativeTerrainProgramIdentity identity;
 const std::shared_ptr<const NativeTerrainProgramConsumer> consumer;
 NativeTerrainProgramDefinition(NativeTerrainProgramIdentity id,
  std::shared_ptr<const NativeTerrainProgramConsumer> owner):identity(std::move(id)),consumer(std::move(owner)){}
private:
 const uint64_t validatedLibraryGeneration=0;
 NativeTerrainProgramDefinition(NativeTerrainProgramIdentity id,
  std::shared_ptr<const NativeTerrainProgramConsumer> owner,uint64_t library):
  identity(std::move(id)),consumer(std::move(owner)),validatedLibraryGeneration(library){}
 friend std::shared_ptr<const NativeTerrainProgramDefinition> BuildValidatedNativeTerrainDefinition(
  NativeTerrainProgramIdentity,std::shared_ptr<const NativeTerrainProgramConsumer>);
 friend void ValidateNativeTerrainDefinition(const NativeTerrainProgramDefinition&,uint64_t);
};
inline std::shared_ptr<const NativeTerrainProgramDefinition> BuildValidatedNativeTerrainDefinition(
 NativeTerrainProgramIdentity identity,std::shared_ptr<const NativeTerrainProgramConsumer> consumer)
{
 ValidateNativeTerrainProgram(identity,consumer,identity.contextGeneration);
 const auto library=consumer->HasImmutableDefinition()?consumer->LibraryGeneration():0;
 return std::shared_ptr<const NativeTerrainProgramDefinition>(
  new NativeTerrainProgramDefinition(std::move(identity),std::move(consumer),library));
}
inline void ValidateNativeTerrainDefinition(const NativeTerrainProgramDefinition& definition,uint64_t context)
{
 if(!definition.validatedLibraryGeneration){
  ValidateNativeTerrainProgram(definition.identity,definition.consumer,context);return;
 }
 // The consumer contract pins immutable identity/support to one library
 // generation. Target formats/MSAA/PSO specialization still validate in Prepare.
 if(!context||definition.identity.contextGeneration!=context||!definition.consumer||
  definition.consumer->ContextGeneration()!=context||
  definition.consumer->LibraryGeneration()!=definition.validatedLibraryGeneration)
  throw std::runtime_error("Original terrain certified definition crossed context/library lifetime");
}

// Full source state: terrain uses units0..19, including Cube on9. Do not use
// the smaller model/CUS submission sampler array to represent this history.
struct NativeTerrainImageSnapshot {
	uint64_t contextGeneration=0, sourceDrawFrame=0, eventSequence=0;
	std::optional<unsigned> activeUnit;
	std::array<std::array<NativeCUSTextureBinding,std::size_t(NativeCUSTextureTarget::Count)>,
		NativeCUSTextureHistory::Units> units;
};
NativeTerrainImageSnapshot CaptureNativeTerrainImages(const NativeTextureRegistry&,
	const NativeCUSTextureHistory&,uint64_t sourceDrawFrame);
void ValidateNativeTerrainImages(const NativeTerrainImageSnapshot&,uint64_t expectedContext,uint64_t expectedDrawFrame);
const NativeCUSTextureBinding& RequireNativeTerrainImage(const NativeTerrainImageSnapshot&,unsigned,NativeCUSTextureTarget);

// Retains existing original tile payload; there is no decoding, assembled copy,
// GPU allocation or independent cache here. The existing SMF array owner must
// resolve this square/mip interval before an actual consumer can sample it.
// Full tile payload/index validation stays at Metal4WorldRenderer::Impl::Map's
// map.owner transition before existing uploads; square checks here are O(1).
struct NativeTerrainSquareBacking final:NativeTextureBacking {
	std::shared_ptr<const WorldTerrainTiles> tiles;
	uint32_t squareX=0,squareZ=0,firstSourceMip=0,sourceMipCount=0;
	NativeTextureBackingKind Kind() const noexcept override {return NativeTextureBackingKind::Other;}
};
void ValidateNativeTerrainSquareBacking(const NativeTerrainSquareBacking&);
void ValidateNativeTerrainSampling(const NativeTextureSampling&);
} // namespace rendering
