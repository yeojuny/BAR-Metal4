#pragma once
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <chrono>
#include <stdexcept>
#include <string>
namespace rendering::metal4 {
using Clock=std::chrono::steady_clock;
inline double Milliseconds(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
inline NSString* String(const std::string& s){return [NSString stringWithUTF8String:s.c_str()];}
inline std::string Error(NSError* e){return e?e.description.UTF8String:"";}
inline void Require(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
inline MTLShaderValidation ShaderValidationMode(bool v){return v?MTLShaderValidationEnabled:MTLShaderValidationDisabled;}
// Only the runtime interface is supplied by this harness. Product packing,
// pool tokens, arguments, pipeline descriptors and barriers remain unmodified.
struct RuntimeLease {
 __strong id<MTLDevice> device;__strong id<MTL4Compiler> compiler;
 __strong NSMutableArray* retained=[NSMutableArray new];__strong NSMutableArray* allocations=[NSMutableArray new];
 bool shade=false;
 id<MTLDevice> Device(){return device;}id<MTL4Compiler> Compiler(){return compiler;}const void* ContextIdentity(){return (__bridge void*)device;}
 unsigned SampleShadingKey(unsigned samples){return samples|(unsigned(shade)<<8);}
 void Retain(id value){[retained addObject:value];}void AddAllocation(id<MTLAllocation> a){[allocations addObject:a];Retain(a);}
 id<MTLLibrary> NewLibraryWithSource(NSString* source,MTLCompileOptions* options,NSError** error){auto macros=[options.preprocessorMacros mutableCopy];if(!macros)macros=[NSMutableDictionary new];macros[@"RECOIL_NATIVE_SAMPLE_SHADING"]=@(shade);options.preprocessorMacros=macros;return [device newLibraryWithSource:source options:options error:error];}
 id<MTLRenderPipelineState> NewRenderPipeline(MTL4RenderPipelineDescriptor* d,NSError** error){return [compiler newRenderPipelineStateWithDescriptor:d compilerTaskOptions:nil error:error];}
};
}
