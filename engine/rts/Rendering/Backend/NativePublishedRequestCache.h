#pragma once
#include "NativeCUSPublishedData.h"
#include <unordered_map>
#include <stdexcept>
namespace rendering {
// One serialized request only. Keep both pointer and ownership identity; never
// share across mutable generations, contexts, or requests. No byte mutation.
template<class Value> class NativePublishedRequestCache {
 struct Entry { std::shared_ptr<const NativeCUSPublishedBuffer> owner; Value value; };
 std::unordered_map<const NativeCUSPublishedBuffer*,Entry> entries;
public:
 const Value* Find(const std::shared_ptr<const NativeCUSPublishedBuffer>& source) const {
  if(!source)throw std::invalid_argument("Published request cache has no source");
  auto it=entries.find(source.get());if(it==entries.end())return nullptr;
  if(it->second.owner.owner_before(source)||source.owner_before(it->second.owner))return nullptr;
  return &it->second.value;
 }
 void Insert(std::shared_ptr<const NativeCUSPublishedBuffer> source,Value value) {
  if(!source)throw std::invalid_argument("Published request cache has no source");
  auto key=source.get();entries.insert_or_assign(key,Entry{std::move(source),std::move(value)});
 }
 size_t Size() const {return entries.size();}
};
}
