/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeUIRecorder.h"
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <vector>

namespace rendering {
struct NativeImageReadbackRequest {
	std::int32_t x=0,y=0;
	std::uint32_t width=0,height=0,readBuffer=0;
};
// Installed by the actual Game draw owner for its lifetime. The callback must
// return only GPU-retired RGBA8 rows in bottom-left glReadPixels order.
using NativeImageReadbackProvider=std::function<std::vector<std::uint8_t>(
	const ui::NativeUIReadbackPrefix&,const NativeImageReadbackRequest&)>;
namespace imageReadback {
struct State {std::uint64_t sourceFrame=0;NativeImageReadbackProvider provider;bool reading=false;};
inline State& Current(){static State state;return state;}
}
class NativeImageReadbackScope {
public:
	NativeImageReadbackScope(std::uint64_t sourceFrame,NativeImageReadbackProvider provider)
	{
		auto& state=imageReadback::Current();
		if(!sourceFrame || !provider || state.provider || state.reading)
			throw std::runtime_error("Native SaveImage frame scope missing or overlapping");
		state.provider=std::move(provider);state.sourceFrame=sourceFrame;
	}
	~NativeImageReadbackScope(){imageReadback::Current()={};}
	NativeImageReadbackScope(const NativeImageReadbackScope&)=delete;
	NativeImageReadbackScope& operator=(const NativeImageReadbackScope&)=delete;
};
inline std::vector<std::uint8_t> ReadNativeImage(std::uint64_t sourceFrame,
	const NativeImageReadbackRequest& request)
{
	auto& state=imageReadback::Current();
	if(!state.provider || state.sourceFrame!=sourceFrame || state.reading)
		throw std::runtime_error("Native SaveImage requires its actual active frame owner without re-entry");
	if(!request.width || !request.height || std::size_t(request.width)>std::numeric_limits<std::size_t>::max()/4 ||
		request.height>std::numeric_limits<std::size_t>::max()/(std::size_t(request.width)*4))
		throw std::runtime_error("Native SaveImage dimensions overflow RGBA8 storage");
	state.reading=true;
	try {
		auto prefix=ui::GetRecorder().CaptureReadbackPrefix();
		auto pixels=state.provider(prefix,request);
		if(pixels.size()!=std::size_t(request.width)*request.height*4)
			throw std::runtime_error("Native SaveImage provider returned an incomplete actual pixel rectangle");
		state.reading=false;
		return pixels;
	} catch(...) {state.reading=false;throw;}
}
} // namespace rendering
