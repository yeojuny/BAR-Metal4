/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once

#include "NativeCUSUniformData.h"

#include <functional>

struct UniformMatricesBuffer;
struct UniformParamsBuffer;

namespace rendering {

class NativeCUSUniformCapture {
public:
	NativeCUSUniformCapture();
	~NativeCUSUniformCapture();
	void Initialize();
	void Reset();
	bool Initialized() const;
	// Called only from the original UniformConstants upload sites. Each invokes
	// that site's original implementation exactly once, including camera/RNG work.
	void UpdateMatrices(uint64_t drawFrame, const std::function<void(UniformMatricesBuffer*)>&);
	void UpdateParams(uint64_t drawFrame, const std::function<void(UniformParamsBuffer*)>&);
	std::shared_ptr<const NativeCUSUniformPublication> Current() const;
private:
	struct State;
	std::unique_ptr<State> state;
};

NativeCUSUniformCapture& OriginalCUSUniformCapture();

} // namespace rendering
