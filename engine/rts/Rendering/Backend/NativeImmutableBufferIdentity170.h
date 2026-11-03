/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <memory>
#include <cstddef>
namespace rendering {
// Pointer equality alone is insufficient after an allocation/address is reused,
// or when an aliasing shared_ptr uses a different control block at the same key.
template<class Owner>
bool SameImmutableBufferOwner170(const std::weak_ptr<const void>& previous,const std::shared_ptr<Owner>& current) noexcept {
 return !previous.expired()&&!previous.owner_before(current)&&!current.owner_before(previous);
}
inline bool ImmutableBufferAdmission170(size_t cost,size_t limit) noexcept{return cost<=limit;}
inline bool ImmutableBufferNeedsEviction170(size_t count,size_t bytes,size_t cost,size_t entryLimit,size_t byteLimit) noexcept {
 return count>=entryLimit||cost>byteLimit||bytes>byteLimit-cost;
}
}
