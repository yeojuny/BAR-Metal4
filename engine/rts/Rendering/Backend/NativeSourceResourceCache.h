/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
#include <stdexcept>
#include <utility>
#include <type_traits>
namespace rendering {
// Serial-owner cache for a complete immutable-source resource version. Creation
// may fail at any stage; publication of source+resource is allocation-free.
// Retaining the returned resource pins old versions independently of the cache.
template<class Source,class Resource> class NativeSourceResourceCache {
 std::shared_ptr<const Source> source;
 std::shared_ptr<Resource> resource;
public:
 std::shared_ptr<const Resource> Find(const std::shared_ptr<const Source>& candidate) const noexcept {
  return source==candidate?resource:nullptr;
 }
 // Prepare may throw (for example allocation retention); the companion commit
 // must not throw. Both cache-hit and replacement paths honor this transaction.
 template<class Create,class Prepare,class Commit>
 std::shared_ptr<Resource> GetOrCreatePrepared(std::shared_ptr<const Source> next,Create&& create,Prepare&& prepare,Commit&& commit) {
  static_assert(std::is_nothrow_invocable_v<Commit>);
  if(!next)throw std::logic_error("Resource version has no immutable source");
  auto replacement=(source==next&&resource)?resource:std::forward<Create>(create)(next);
  if(!replacement)throw std::logic_error("Resource version factory returned no owner");
  std::forward<Prepare>(prepare)(replacement);
  std::forward<Commit>(commit)();
  source=std::move(next);resource=std::move(replacement);return resource;
 }
 template<class Create> std::shared_ptr<Resource> GetOrCreate(std::shared_ptr<const Source> next,Create&& create) {
  return GetOrCreatePrepared(std::move(next),std::forward<Create>(create),[](const auto&){},[]() noexcept {});
 }
 void Clear() noexcept {resource.reset();source.reset();}
};
}
