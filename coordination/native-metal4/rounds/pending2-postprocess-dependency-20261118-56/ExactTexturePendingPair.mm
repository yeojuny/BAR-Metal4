/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "Rendering/Backend/NativePostprocessTextureReceipt.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>

using namespace rendering;

static void Check(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

struct Feedback {
	__strong NSError* error = nil;
	dispatch_semaphore_t available = dispatch_semaphore_create(0);
};

struct Slot {
	__strong id<MTL4CommandAllocator> allocator = nil;
	__strong id<MTL4CommandBuffer> command = nil;
	__strong id<MTLResidencySet> residency = nil;
	std::shared_ptr<Feedback> feedback = std::make_shared<Feedback>();
};

static Slot MakeSlot(id<MTLDevice> device, id<MTLAllocation> first, id<MTLAllocation> second)
{
	Slot slot;
	slot.allocator = [device newCommandAllocator];
	slot.command = [device newCommandBuffer];
	auto descriptor = [MTLResidencySetDescriptor new];
	descriptor.initialCapacity = 2;
	NSError* error = nil;
	slot.residency = [device newResidencySetWithDescriptor:descriptor error:&error];
	Check(slot.allocator && slot.command && slot.residency, "Cannot create independent Metal4 slot");
	[slot.residency addAllocation:first];
	[slot.residency addAllocation:second];
	[slot.residency commit];
	[slot.command beginCommandBufferWithAllocator:slot.allocator];
	[slot.command useResidencySet:slot.residency];
	return slot;
}

static void Commit(id<MTL4CommandQueue> queue, const Slot& slot)
{
	auto feedback = slot.feedback;
	auto options = [MTL4CommitOptions new];
	[options addFeedbackHandler:^(id<MTL4CommitFeedback> result) {
		feedback->error = result.error;
		dispatch_semaphore_signal(feedback->available);
	}];
	id<MTL4CommandBuffer> command = slot.command;
	[queue commit:&command count:1 options:options];
}

static bool FeedbackArrived(const Slot& slot)
{
	return dispatch_semaphore_wait(slot.feedback->available,
		dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) == 0;
}

int main()
{
	@autoreleasepool {
		try {
			constexpr std::array<std::uint32_t, 4> expected{0x1234abcd, 0x87654321, 0x42, 0xf00dface};
			id<MTLDevice> device = MTLCreateSystemDefaultDevice();
			Check(device && [device supportsFamily:MTLGPUFamilyMetal4], "Metal4 device unavailable");
			auto queue = [device newMTL4CommandQueue];
			auto start = [device newSharedEvent];
			auto completed = [device newSharedEvent];
			auto descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Uint
				width:4 height:1 mipmapped:NO];
			descriptor.storageMode = MTLStorageModePrivate;
			descriptor.usage = MTLTextureUsageShaderRead;
			auto image = [device newTextureWithDescriptor:descriptor];
			auto input = [device newBufferWithBytes:expected.data() length:sizeof(expected) options:MTLResourceStorageModeShared];
			auto output = [device newBufferWithLength:sizeof(expected) options:MTLResourceStorageModeShared];
			Check(queue && start && completed && image && input && output, "Cannot create fixture resources");
			std::memset(output.contents, 0, sizeof(expected));
			Slot producer = MakeSlot(device, input, image);
			Slot consumer = MakeSlot(device, image, output);
			Check(producer.allocator != consumer.allocator && producer.residency != consumer.residency, "Slots alias");
			const MTLOrigin origin = MTLOriginMake(0, 0, 0);
			const MTLSize size = MTLSizeMake(4, 1, 1);
			auto write = [producer.command computeCommandEncoder];
			Check(write != nil, "No producer encoder");
			[write copyFromBuffer:input sourceOffset:0 sourceBytesPerRow:sizeof(expected) sourceBytesPerImage:0
				sourceSize:size toTexture:image destinationSlice:0 destinationLevel:0 destinationOrigin:origin];
			[write endEncoding];
			[producer.command endCommandBuffer];

			int version = 0;
			const NativePostprocessTextureReceipt dependency{&version, (__bridge const void*)image, 1, 1, 11, 1};
			Check(dependency.Valid(), "Producer receipt absent");
			auto read = [consumer.command computeCommandEncoder];
			Check(read != nil, "No consumer encoder");
			[read copyFromTexture:image sourceSlice:dependency.slice sourceLevel:dependency.level sourceOrigin:origin
				sourceSize:size toBuffer:output destinationOffset:0 destinationBytesPerRow:sizeof(expected)
				destinationBytesPerImage:0];
			[read endEncoding];
			[consumer.command endCommandBuffer];

			// Gate just this isolated test so post-second-commit observation is
			// deterministic. Do not use the gate or these timings in an FPS test.
			unsigned committed = 0;
			unsigned peak = 0;
			bool submitException = false;
			@try {
				@try {
					[queue waitForEvent:start value:1];
					Commit(queue, producer);
					committed = 1;
					[queue signalEvent:completed value:dependency.event];
					[queue waitForEvent:completed value:dependency.event];
					Commit(queue, consumer);
					committed = 2;
					[queue signalEvent:completed value:2];
					peak = completed.signaledValue == 0 ? 2 : 0;
					std::printf("observed_pending_after_second_commit=%u producer_event=%llu\n",
						peak, static_cast<unsigned long long>(completed.signaledValue));
				} @finally {
					start.signaledValue = 1;
				}
			} @catch (NSException* error) {
				std::fprintf(stderr, "%s\n", error.reason.UTF8String);
				submitException = true;
			}
			const bool drained = !committed || [completed waitUntilSignaledValue:committed timeoutMS:8000];
			const bool producerArrived = committed < 1 || FeedbackArrived(producer);
			const bool consumerArrived = committed < 2 || FeedbackArrived(consumer);
			Check(drained && producerArrived && consumerArrived, "Metal fixture did not drain");
			Check(!submitException && peak == 2, "Two outstanding submissions were not verified");
			Check(!producer.feedback->error && !consumer.feedback->error, "Metal fixture GPU error");
			Check(std::memcmp(output.contents, expected.data(), sizeof(expected)) == 0, "Ordered texture readback mismatched");
			[producer.allocator reset];
			[consumer.allocator reset];
			std::puts("PASS mip=0 slice=0 words=4 feedback=2 retired=2 drawables=0");
			return 0;
		} catch (const std::exception& error) {
			std::fprintf(stderr, "%s\n", error.what());
			return 1;
		}
	}
}
