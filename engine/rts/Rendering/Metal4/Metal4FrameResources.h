/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Metal4PublishedRequestCache.h"
#include "Metal4DepthStateCache.h"
#include "Metal4ResidencyCommit.h"
#include <map>
#include <functional>
#include <new>
#include <vector>

namespace rendering::metal4 {
struct PostprocessTextureStorage181;

// Reusable command memory and residency have the same retirement boundary.
// Each slot is reset/rebound only after its event AND feedback have retired.
// The primary may own one spare for CPU recording while the other executes.
// The spare never owns the primary: there is no cycle or unbounded frame queue.
struct RuntimeFrameSlot {
	std::shared_ptr<RuntimeFrameSlot> spare;
	std::shared_ptr<RuntimeFrameSlot> third; // independent allocator/residency, no cycles
	__strong id<MTL4CommandAllocator> allocator = nil;
	__strong id<MTL4CommandBuffer> command = nil;
	__strong id<MTLResidencySet> residency = nil;
	__strong NSHashTable* residentAllocations = nil;
	DepthStateCache depthStates;
	dispatch_queue_t residencyWorker = dispatch_queue_create("org.beyondallreason.metal4.residency",
		dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL,QOS_CLASS_USER_INITIATED,0));
};

// One serialized render request may contain several submissions (e.g. a prefix
// followed by the main pass). Keep all its publication pins until the request
// ends AND every submission owner has retired; not just its first submission.
// This is a lifetime boundary, not a simulation-frame counter or an async queue.
struct RuntimeFrameResources {
	explicit RuntimeFrameResources(std::shared_ptr<RuntimeFrameSlot> selected): slot(std::move(selected)) {}
	const std::shared_ptr<RuntimeFrameSlot> slot;
	__strong NSMutableArray* retained = nil;
	__strong NSHashTable* requestedAllocations = nil;
	// Published containers stay immutable even if a later prefix in this lease
	// registers more resources. The frame's caller serializes these mutations.
	NSHashTable* PublishRequestedAllocations() {
		requestsPublished = true;
		return requestedAllocations;
	}
	void AddUniqueRequestedAllocation(id<MTLAllocation> allocation) {
		if (requestsPublished) {
			NSHashTable* next = [requestedAllocations copy];
			if (!next) throw std::bad_alloc();
			requestedAllocations = next;
			requestsPublished = false;
		}
		[requestedAllocations addObject:allocation];
	}
	NSArray* PublishDeferredGraph() {
		graphPublished = true;
		return retained;
	}
	NSMutableArray* WritableRetained() {
		if (graphPublished) {
			NSMutableArray* next = [retained mutableCopy];
			if (!next) throw std::bad_alloc();
			retained = next;
			graphPublished = false;
		}
		return retained;
	}
	bool requestsPublished = false;
	bool graphPublished = false;
	std::uint64_t allocationRequests148 = 0;
	PublishedRequestState publications;
	ResidencyCommitTask residencyCommit;
	// Receipt is applied exactly once, after actual retirement, never on encode.
 std::function<void()> previousRetired;
 // Immutable prior producers may be encoded as inputs now, but must actually
 // complete before this command buffer can be submitted. Keys are retained by
 // the closures; repeated consumers do not add repeated completion work.
 std::map<const void*,std::function<void()>,std::less<const void*>> priorReadChecks;
 bool recordedWhilePending = false;
 bool keepResidency = false;
	bool recording = false;
	bool pending = false;
	bool completionTransferred = false;
	// Append-only for the whole request, including synchronous prefixes. Every
	// RuntimeSubmissionResources owns this frame, so feedback/failure retains
	// these pins without per-version Objective-C holders or graph-array appends.
	std::vector<std::shared_ptr<PostprocessTextureStorage181>> postprocessTextureOwners181;
};

// The feedback handler receives its own immutable object graph and a shared
// frame owner. Retaining an MTLBuffer alone does NOT pin a mutable pool slot:
// publications.used also prevents that storage from being overwritten.
struct RuntimeSubmissionResources {
	explicit RuntimeSubmissionResources(const std::shared_ptr<RuntimeFrameResources>& frame):
		owner(frame), retained([frame->retained copy]) {}
	// The deferred path seals its own graph. A synchronous prefix still gets
	// an independent snapshot, including when its lease continues recording.
	struct DeferredPresentation {};
	RuntimeSubmissionResources(const std::shared_ptr<RuntimeFrameResources>& frame, DeferredPresentation):
		owner(frame), retained(frame->PublishDeferredGraph()) {}
	const std::shared_ptr<RuntimeFrameResources> owner;
	__strong NSArray* retained = nil;
};

} // namespace rendering::metal4
