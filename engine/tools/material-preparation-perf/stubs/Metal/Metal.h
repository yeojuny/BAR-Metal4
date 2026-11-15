// Declaration-only test double. Never linked and not an Apple SDK substitute.
#pragma once
#include <cstdint>
using NSUInteger=unsigned long;
using MTLResourceOptions=NSUInteger;
struct MTLResourceID {std::uint64_t _impl;};
inline constexpr MTLResourceOptions MTLResourceStorageModeShared=0;
enum MTLTextureType {MTLTextureType2D,MTLTextureTypeCube,MTLTextureType3D};
enum MTLBindingAccess {MTLBindingAccessReadOnly};
enum MTLDataType {MTLDataTypeTexture,MTLDataTypeSampler};
#ifndef nil
#define nil ((id)0)
#endif
@protocol NSObject @end
__attribute__((objc_root_class)) @interface NSObject <NSObject>
+ (instancetype)new;
@end
@interface NSMutableArray : NSObject
- (void)addObject:(id)object;
@end
@protocol MTLTexture <NSObject>
@property(readonly) MTLResourceID gpuResourceID;
@end
@protocol MTLSamplerState <NSObject>
@property(readonly) MTLResourceID gpuResourceID;
@end
@protocol MTLBuffer <NSObject>
@property(readonly) NSUInteger length;
@property(readonly) NSUInteger allocatedSize;
@end
@interface MTLArgumentDescriptor : NSObject
+ (instancetype)argumentDescriptor;
@property NSUInteger index;
@property NSUInteger arrayLength;
@property MTLBindingAccess access;
@property MTLDataType dataType;
@property MTLTextureType textureType;
@end
@protocol MTLArgumentEncoder <NSObject>
@property(readonly) NSUInteger encodedLength;
- (void)setArgumentBuffer:(id<MTLBuffer>)buffer offset:(NSUInteger)offset;
- (void)setTexture:(id<MTLTexture>)texture atIndex:(NSUInteger)index;
- (void)setSamplerState:(id<MTLSamplerState>)sampler atIndex:(NSUInteger)index;
@end
@protocol MTLDevice <NSObject>
- (id<MTLArgumentEncoder>)newArgumentEncoderWithArguments:(NSMutableArray*)descriptors;
- (id<MTLBuffer>)newBufferWithLength:(NSUInteger)length options:(MTLResourceOptions)options;
@end
