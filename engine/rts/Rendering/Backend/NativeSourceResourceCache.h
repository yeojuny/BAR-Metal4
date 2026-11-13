/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
#include <stdexcept>
#include <utility>
namespace rendering {
// Serial-owner cache for a complete immutable-source resource version. Creation
// may fail at any stage; publication of source+resource is allocation-free.
// Retaining the returned resource pins old versions independently of the cache.
template<class Source,class Resource> class NativeSourceResourceCache {
 std::shared_ptr<const Source> source;
 std::shared_ptr<Resource> resource;
public:
 template<class Create> std::shared_ptr<Resource> GetOrCreate(std::shared_ptr<const Source> next,Create&& create) {
  if(!next)throw std::logic_error("Resource version has no immutable source");
  if(source==next&&resource)return resource;
  auto replacement=std::forward<Create>(create)(next);
  if(!replacement)throw std::logic_error("Resource version factory returned no owner");
  // shared_ptr moves/copies below cannot throw. Never expose half a pair.
  source=std::move(next);resource=std::move(replacement);return resource;
 }
 void Clear() noexcept {resource.reset();source.reset();}
};
}
