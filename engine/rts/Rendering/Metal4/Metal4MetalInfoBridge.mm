/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4MetalInfoBridge.h"
#include "Metal4MetalInfo.h"
#include "../Backend/NativeMetalInfoCapture.h"

namespace rendering {
using namespace metal4;

void InitializeMetalInfoMaintenance()
{
	{
		RuntimeLease lease;
		double compileMilliseconds = 0;
		SharedMetalInfoGPU().Initialize(lease, true, compileMilliseconds);
	}
	SetNativeMetalInfoConsumer([](std::shared_ptr<const NativeMetalInfoFrame> frame) {
		RuntimeLease lease;
		double compileMilliseconds = 0;
		auto prepared = SharedMetalInfoGPU().Prepare(lease, std::move(frame), true, compileMilliseconds);
		Require(prepared != nullptr, "Native metal maintenance has no prepared original producer");
		if (prepared->HasUpdates()) {
			lease.CommitResources();
			auto command = lease.Begin("Original info-texture update cadence, independent of window presentation");
			prepared->EncodeDependencies(command);
			lease.SubmitAndWait(Clock::now(), prepared->CompletionSerial());
			prepared->DidComplete();
		}
		return prepared->CompletionSerial();
	});
}

void ResetMetalInfoResources()
{
	Require(SharedRuntime().CanSafelyDetach(), "Cannot reset original metal info before GPU retirement");
	ResetNativeMetalInfo();
	SharedMetalInfoGPU().ClearResources();
}

void ShutdownMetalInfoMaintenance()
{
	SetNativeMetalInfoConsumer({});
	ResetMetalInfoResources();
}
}
