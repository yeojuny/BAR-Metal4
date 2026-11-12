/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativeWaterGroundSourceData.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Shader {struct IProgramObject;}
namespace rendering {
struct NativeTerrainProgramRequest {
	NativeTerrainProgramIdentity identity;
	std::string vertexSource,fragmentSource,rawDefinitions,flagDefinitions;
	std::vector<std::pair<std::string,uint32_t>> attributeBindings,outputBindings;
};
struct NativeTerrainProgramLink {
	std::shared_ptr<const NativeTerrainProgramConsumer> consumer;
	// A recognized but currently unsupported source is distinct from invalid.
	// A real compiler/library/PSO failure MUST throw, never return this reason.
	std::string unsupportedReason;
	// Successful known-source ABI publication only. GLSL initializes active
	// uniforms to zero at link; subsequent original named writes take priority.
	// Unsupported programs do not publish invented successful-link defaults.
	std::map<std::string,NativeTerrainUniformValue> linkDefaults;
};
struct NativeTerrainLuaProgramImport {
	enum class Status {Observed,Unsupported};
	Status status=Status::Unsupported;
	NativeTerrainProgramRequest request;
	std::shared_ptr<const NativeTerrainUniformSnapshot> uniforms;
	std::string unsupportedReason;
};
struct NativeTerrainProgramServices {
	std::function<uint64_t()> contextGeneration;
	std::function<NativeTerrainProgramLink(const NativeTerrainProgramRequest&)> compile;
	// Called only for an actual nonzero original Lua handle. An unobserved or
	// unsupported handle is NOT declared invalid and cannot select STD instead.
	std::function<NativeTerrainLuaProgramImport(uint32_t)> resolveLua;
};
// Shared source definition, distinct from per-write sealed uniform content.
// Not a GPU readiness certificate: existing validation still runs at its
// original boundaries and target-specific pipeline checks remain in prepare.
struct NativeTerrainProgramDefinition {
 const NativeTerrainProgramIdentity identity;
 const std::shared_ptr<const NativeTerrainProgramConsumer> consumer;
 NativeTerrainProgramDefinition(NativeTerrainProgramIdentity id,
  std::shared_ptr<const NativeTerrainProgramConsumer> owner):identity(std::move(id)),consumer(std::move(owner)){}
};
struct NativeTerrainProgramPublication {
	enum class State {Unlinked,Invalid,Linked,Unsupported};
	State state=State::Unlinked;
	std::shared_ptr<const NativeTerrainProgramDefinition> definition;
	std::shared_ptr<const NativeTerrainUniformSnapshot> uniforms;
	std::string unsupportedReason;
};

#ifdef BAR_NATIVE_TERRAIN_WRITE_STATS
// Diagnostic-only main/source-thread counters. Call Take at source draw frame
// boundaries; simulation frame IDs are not distinct draw IDs. No timers or
// counters are compiled into ordinary builds. Publications includes Enable;
// mapCopies/exactChanges refer only to setter work (not link defaults).
struct NativeTerrainUniformWriteStats {
 uint64_t calls=0,exactChanges=0,alignedNoOps=0,stalePublicationFallbacks=0,mapCopies=0,publications=0,definitionCopies=0;
};
NativeTerrainUniformWriteStats TakeNativeTerrainUniformWriteStats();
#endif

// Actual Session owns these services; no device/compiler/cache is created here.
// Configure before source program creation, clear after their real destruction.
void ConfigureNativeTerrainProgramSource(NativeTerrainProgramServices);
std::shared_ptr<Shader::IProgramObject> CreateNativeTerrainProgramSource(
	NativeTerrainProgramSelection,std::string originalName,std::string rawDefinitions);
void LoadNativeTerrainLuaProgram(Shader::IProgramObject*,uint32_t originalHandle);
// Called by the original ShaderHandler::ReloadAll lifecycle event.
void ReloadNativeTerrainPrograms();
bool RequireNativeTerrainProgramValid(const Shader::IProgramObject*);
NativeTerrainProgramSnapshot CaptureCurrentNativeTerrainProgram();
} // namespace rendering
