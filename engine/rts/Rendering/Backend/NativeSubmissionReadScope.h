/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <stdexcept>
namespace rendering {
// Request-local identity. Zero means no source submission; it never inherits a
// renderer's previous request. A successful binding cannot be changed or cleared.
class NativeSubmissionReadIdentity {
 std::uint64_t submission=0;
public:
 NativeSubmissionReadIdentity()=default;
 NativeSubmissionReadIdentity(const NativeSubmissionReadIdentity&)=delete;
 NativeSubmissionReadIdentity& operator=(const NativeSubmissionReadIdentity&)=delete;
 void Bind(std::uint64_t value) {
  if(!value||submission)throw std::logic_error("Invalid or repeated source submission identity binding");
  submission=value;
 }
 std::uint64_t Current() const noexcept {return submission;}
};
// Serial renderer-owned binding, not GPU completion evidence or permission for
// concurrent renderer entry. Consumers capture Current() by value for encoding.
class NativeSubmissionReadBinding {
 std::uint64_t current=0;
public:
 std::uint64_t Current() const noexcept {return current;}
 class Scope {
  NativeSubmissionReadBinding& owner;
 public:
  Scope(NativeSubmissionReadBinding& owner,std::uint64_t submission):owner(owner) {
   if(!submission || owner.current)throw std::logic_error("Invalid or overlapping renderer submission read scope");
   owner.current=submission;
  }
  ~Scope() noexcept {owner.current=0;}
  Scope(const Scope&)=delete;
  Scope& operator=(const Scope&)=delete;
 };
};
}
