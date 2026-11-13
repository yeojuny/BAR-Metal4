/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeRendererAccessGate.h"
#include "NativeSerialRenderWorker.h"
#include <memory>
#include <optional>
#include <utility>
namespace rendering {
// Synchronous source API result, executed by the same owner as render work.
// No RuntimeLease: allocation-only controls must not drain source callbacks.
// Caller owns source-thread serialization and must not hold an outer gate scope.
template<class Request,class F>
auto InvokeNativeRendererControl(std::unique_ptr<NativeSerialRenderWorker>& worker,
 NativeRendererAccessGate& gate,Request request,F&& execute) {
 std::optional<NativeRendererAccessGate::Scope> source;source.emplace(&gate);
 if(!worker)worker=std::make_unique<NativeSerialRenderWorker>();
 NativeRendererAccessGate::Handoff handoff(gate);
 source.reset();
 return worker->Invoke([request=std::move(request),execute=std::forward<F>(execute),&handoff]() mutable {
  NativeRendererAccessGate::Scope owner(handoff);
  return execute(request,owner);
 });
}
}
