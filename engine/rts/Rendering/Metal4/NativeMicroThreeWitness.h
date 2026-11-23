/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rendering::metal4::microthree {
inline bool Enabled(const char* name) noexcept {
    const char* value=std::getenv(name);
    return value&&std::strcmp(value,"1")==0;
}
inline bool Witness() noexcept {
    static const bool enabled=Enabled("BAR_NATIVE_MICRO_THREE_WITNESS");
    return enabled;
}
inline uint64_t Hash(const void* data,size_t size,uint64_t hash=14695981039346656037ull) noexcept {
    const auto* bytes=static_cast<const unsigned char*>(data);
    for(size_t i=0;i<size;++i)hash=(hash^bytes[i])*1099511628211ull;
    return hash;
}
struct Meter {
    const char* lane;
    const char* route;
    std::atomic<uint64_t> events=0, calls=0, bytes=0, bindings=0, predicates=0;
    bool Record(uint64_t callCount,uint64_t byteCount,uint64_t bindingCount,uint64_t predicateCount=0) noexcept {
        calls.fetch_add(callCount,std::memory_order_relaxed);
        bytes.fetch_add(byteCount,std::memory_order_relaxed);
        bindings.fetch_add(bindingCount,std::memory_order_relaxed);
        predicates.fetch_add(predicateCount,std::memory_order_relaxed);
        return events.fetch_add(1,std::memory_order_relaxed)<4;
    }
    ~Meter() {
        if(!events)return;
        std::fprintf(stderr,"[NativeMicroThree] event=total lane=%s route=%s events=%llu calls=%llu bytes=%llu bindings=%llu predicates=%llu\n",
            lane,route,static_cast<unsigned long long>(events.load()),static_cast<unsigned long long>(calls.load()),
            static_cast<unsigned long long>(bytes.load()),static_cast<unsigned long long>(bindings.load()),
            static_cast<unsigned long long>(predicates.load()));
    }
};
}
