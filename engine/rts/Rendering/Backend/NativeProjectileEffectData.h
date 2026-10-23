/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeBufferData.h"
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace rendering {
// Exact original programs; this is not a generic GLSL success handle.
enum class NativeProjectileProgram : std::uint8_t {
	Beam, BeamFlare, BeamGlow, BeamPulse,
	Plasma, PlasmaCrossSection, PlasmaGlow,
};
struct NativeProjectileTexture {
	std::string sourceName;
	std::uint32_t width=0,height=0;
	std::vector<std::uint8_t> rgba;
	// Original unqualified CNamedTextures: repeat, linear mag, trilinear min,
	// real generated mip chain; rows retain the original CBitmap decoder origin.
};
struct NativeProjectileEffectDraw {
	std::uint64_t sequence=0;
	NativeProjectileProgram program=NativeProjectileProgram::Beam;
	NativeBufferSnapshotRef vertices,indices,instances;
	std::uint32_t instanceCount=0, vertexCount=6;
	bool indexed=false; // Actual original helper closes over DrawArrays before index attachment.
	std::map<std::string,float> parameters;
	std::shared_ptr<const NativeProjectileTexture> texture;
};
struct NativeProjectileEffectFrame {
	std::array<float,16> cameraViewProjection{},cameraViewInverse{};
	// Exact engine UniformConstants timeInfo, including elapsed seconds at z.
	std::array<float,4> timeInfo{};
	std::vector<NativeProjectileEffectDraw> draws;
};
void ValidateNativeProjectileEffectDraw(const NativeProjectileEffectDraw& draw);
}
