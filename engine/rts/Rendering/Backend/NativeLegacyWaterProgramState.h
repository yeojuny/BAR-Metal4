/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace rendering {
// Only the inherited registers actually read by waterDyn[NT].vp. Environment
// parameters belong to the context, not the water object or bound ARB program.
class NativeLegacyWaterProgramState {
public:
	struct Values {
		std::array<std::array<float,4>,3> env{}; // VP16, VP17, VP18
		std::array<float,16> matrix0{};
		bool complete=false;
	};
	void InitializeContext(uint64_t generation) {
		if (!generation || contextGeneration) throw std::logic_error("Legacy water program context already owned or invalid");
		contextGeneration=generation; values={}; values.complete=true; matrixStack.clear(); observedMatrixDepth=0; stackErrors=0;
		// ARB_vertex_program tables X.6 and X.10 define these initial values.
		values.matrix0={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
		matrixStack.push_back(values.matrix0);
	}
	void DestroyContext() noexcept { contextGeneration=0; values={}; matrixStack.clear(); observedMatrixDepth=0; stackErrors=0; }
	void ObserveVertexEnvironment(unsigned target,unsigned index,const float* four) {
		if (target!=0x8620 || index<16 || index>18) return;
		if (!contextGeneration || !four) throw std::logic_error("Legacy water program write without source context");
		// Bit-preserving: the legacy ARB integer-vector wrappers cast int storage
		// to float storage rather than performing numeric conversion.
		std::memcpy(values.env[index-16].data(),four,4*sizeof(float));
	}
	// Only actual reference-context MAX_PROGRAM_MATRIX_STACK_DEPTH_ARB evidence
	// may establish this capability. The minimum is one, NOT the UI stack256.
	void ObserveProgramMatrix0StackDepth(unsigned depth) {
		RequireContext();
		if (!depth || (observedMatrixDepth && observedMatrixDepth!=depth))
			throw std::logic_error("Invalid or changed source program-matrix stack depth");
		observedMatrixDepth=depth;
	}
	const std::array<float,16>& ProgramMatrix0Top() const { RequireContext(); return matrixStack.back(); }
	void ProgramMatrix0Load(const std::array<float,16>& value) { RequireContext(); matrixStack.back()=value; }
	void ProgramMatrix0Push() {
		RequireMatrixDepth();
		// OpenGL errors leave the stack unchanged; the later pop still executes.
		if (matrixStack.size()>=observedMatrixDepth) { stackErrors|=1; return; }
		matrixStack.push_back(matrixStack.back());
	}
	void ProgramMatrix0Pop() {
		RequireMatrixDepth();
		if (matrixStack.size()==1) { stackErrors|=2; return; }
		matrixStack.pop_back();
	}
	unsigned ProgramMatrix0Errors() const { RequireContext(); return stackErrors; }
	void Invalidate() noexcept { values.complete=false; }
	Values Snapshot() const {
		if (!contextGeneration) throw std::logic_error("Legacy water program read without source context");
		auto result=values;result.matrix0=matrixStack.back();return result;
	}
private:
	void RequireContext() const { if (!contextGeneration) throw std::logic_error("Program matrix without source context"); }
	void RequireMatrixDepth() { RequireContext(); if (!observedMatrixDepth) { Invalidate(); throw std::logic_error("Source program-matrix stack depth unobserved"); } }
	std::vector<std::array<float,16>> matrixStack;
	unsigned observedMatrixDepth=0,stackErrors=0;
	uint64_t contextGeneration=0;
	Values values;
};
inline NativeLegacyWaterProgramState& OriginalNativeLegacyWaterProgramState() {
	static NativeLegacyWaterProgramState state; return state;
}
}
