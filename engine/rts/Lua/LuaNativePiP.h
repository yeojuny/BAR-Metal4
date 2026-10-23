/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Backend/NativePiPData.h"
#include <functional>
#include <memory>
struct lua_State;
struct S3DModel;
namespace rendering { struct WorldModel; namespace ui { class NativeUIRecorder; } }
namespace LuaNativePiP {
	using DrawSink = std::function<void(rendering::ui::NativeUIRecorder&, rendering::NativePiPDraw)>;
	using ModelProvider = std::function<std::shared_ptr<const rendering::WorldModel>(const S3DModel*)>;
	void SetDrawSink(DrawSink sink, bool consumerAvailable);
	void SetModelProvider(ModelProvider provider);
	bool Available();
	bool PushEntries(lua_State* L);
	void Reset();
}
