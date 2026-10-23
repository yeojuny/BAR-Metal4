/* Exact selected31657 Fire GL4 identities; unchanged original VS/FS and UBO ABI. */
#pragma once
#include "NativeFlamethrowerPrograms.h"
namespace rendering::fireGL4Program {
inline constexpr std::uint64_t GadgetHash=0x0e91c9448cb1e410ULL;
inline constexpr std::uint64_t VertexHash=0x0581a2a4fc63060fULL;
inline constexpr std::uint64_t FragmentHash=0xed41810ce2a06a82ULL;
inline constexpr std::uint64_t InstanceHelperHash=0x8cef105d28038367ULL;
inline constexpr std::uint64_t ShaderHelperHash=0xd291eb46066d7b4aULL;
inline constexpr std::array<std::string_view,13> ParameterNames{"FIRE_BUOY","SMOKE_BUOY","EMBER_BUOY","WIND_FLAME_MULT","WIND_SMOKE_MULT","WIND_EMBER_MULT","FIRE_WOBBLE_MIN","FIRE_WOBBLE_MAX","SMOKE_WOBBLE","EMBER_WOBBLE","FIRE_GROWTH","SMOKE_GROWTH","EMBER_GROWTH"};
inline constexpr auto EnginePrefix=flamethrowerProgram::EnginePrefix;
}
