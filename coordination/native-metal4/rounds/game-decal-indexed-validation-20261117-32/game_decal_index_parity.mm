// Standalone fixture adapted from the existing G01 Metal 4 parity helper.
// It uses the original GameDecal shader and frozen candidate index table.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "Metal4GameDecalGeometryIndices.h"
#include "NativeGameDecalShaders.h"

using namespace simd;
using namespace rendering::metal4;
namespace {
constexpr unsigned W = 64;
constexpr unsigned H = 64;
constexpr unsigned T = 8;
constexpr unsigned pixels = W * H;
constexpr float clearDepth = 0.9f;
struct Row { float4 dimensions, uv, decay, position, parameters; };
struct Uniform {
    float4x4 viewProjection, viewInverse, shadow;
    float4 time, map, sun, ambient, diffuse, waterAbsorb, waterMin, waterBase;
    uint4 options;
    float4 alpha;
};
static_assert(sizeof(Row) == 80 && sizeof(Uniform) == 352);
static_assert(gameDecalGeometry::Indices.size() == 96 && sizeof(gameDecalGeometry::Indices) == 192);

void check(bool ok, const char* message, NSError* error = nil) {
    if (ok) return;
    std::fprintf(stderr, "FAIL %s %s\n", message, error ? error.localizedDescription.UTF8String : "");
    std::exit(1);
}

id<MTLTexture> texture(id<MTLDevice> device, MTLPixelFormat format, unsigned width, unsigned height,
                       bool target = false) {
    auto desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];
    desc.storageMode = MTLStorageModeShared;
    desc.usage = target ? MTLTextureUsageRenderTarget : MTLTextureUsageShaderRead;
    auto result = [device newTextureWithDescriptor:desc];
    check(result != nil && result.sampleCount == 1, "single-sample texture");
    return result;
}

template <class TData> void upload(id<MTLTexture> destination, const TData* pixels, unsigned rowBytes) {
    [destination replaceRegion:MTLRegionMake2D(0, 0, destination.width, destination.height)
                    mipmapLevel:0 withBytes:pixels bytesPerRow:rowBytes];
}

struct Result {
    std::array<std::uint8_t, pixels * 4> color{};
    std::array<float, pixels> depth{};
    std::array<std::uint8_t, pixels> stencil{};

    bool operator==(const Result& other) const {
        return color == other.color && stencil == other.stencil &&
               std::memcmp(depth.data(), other.depth.data(), sizeof(depth)) == 0;
    }
};

Result render(id<MTLDevice> device, id<MTL4CommandQueue> queue, id<MTLRenderPipelineState> pipeline,
              id<MTL4ArgumentTable> arguments, id<MTLResidencySet> inputs,
              id<MTLBuffer> indices, unsigned count) {
    NSError* error = nil;
    auto color = texture(device, MTLPixelFormatBGRA8Unorm, W, H, true);
    auto depth = texture(device, MTLPixelFormatDepth32Float, W, H, true);
    auto stencil = texture(device, MTLPixelFormatStencil8, W, H, true);
    auto ds = [MTLDepthStencilDescriptor new];
    ds.depthCompareFunction = MTLCompareFunctionLessEqual;
    ds.depthWriteEnabled = YES;
    auto face = [MTLStencilDescriptor new];
    face.stencilCompareFunction = MTLCompareFunctionAlways;
    face.depthStencilPassOperation = MTLStencilOperationReplace;
    face.readMask = face.writeMask = 255;
    ds.frontFaceStencil = ds.backFaceStencil = face;
    auto state = [device newDepthStencilStateWithDescriptor:ds];
    check(state != nil, "depth/stencil state");
    auto outputs = [device newResidencySetWithDescriptor:[MTLResidencySetDescriptor new] error:&error];
    check(outputs != nil, "attachment residency", error);
    for (auto t : {color, depth, stencil}) [outputs addAllocation:t];
    [outputs commit];
    auto allocator = [device newCommandAllocator];
    auto commands = [device newCommandBuffer];
    [commands beginCommandBufferWithAllocator:allocator];
    auto pass = [MTL4RenderPassDescriptor new];
    pass.renderTargetWidth = W;
    pass.renderTargetHeight = H;
    pass.defaultRasterSampleCount = 1;
    pass.colorAttachments[0].texture = color;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    pass.depthAttachment.texture = depth;
    pass.depthAttachment.loadAction = MTLLoadActionClear;
    pass.depthAttachment.storeAction = MTLStoreActionStore;
    pass.depthAttachment.clearDepth = clearDepth;
    pass.stencilAttachment.texture = stencil;
    pass.stencilAttachment.loadAction = MTLLoadActionClear;
    pass.stencilAttachment.storeAction = MTLStoreActionStore;
    pass.stencilAttachment.clearStencil = 0;
    auto e = [commands renderCommandEncoderWithDescriptor:pass];
    check(e != nil, "render encoder");
    [e setViewport:MTLViewport{0,0,W,H,0,1}];
    [e setScissorRect:MTLScissorRect{0,0,W,H}];
    [e setRenderPipelineState:pipeline];
    [e setDepthStencilState:state];
    [e setCullMode:MTLCullModeNone];
    [e setDepthClipMode:MTLDepthClipModeClip];
    [e setStencilFrontReferenceValue:7 backReferenceValue:7];
    [e setArgumentTable:arguments atStages:MTLRenderStageVertex|MTLRenderStageFragment];
    if (indices) {
        check(indices.length == 192 && indices.gpuAddress != 0 && (indices.gpuAddress & 1) == 0, "index buffer");
        [e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:gameDecalGeometry::Indices.size()
                       indexType:MTLIndexTypeUInt16 indexBuffer:indices.gpuAddress
                indexBufferLength:indices.length instanceCount:count];
    } else {
        [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0
              vertexCount:gameDecalGeometry::VertexSlots instanceCount:count];
    }
    [e endEncoding];
    [commands useResidencySet:inputs];
    [commands useResidencySet:outputs];
    [commands endCommandBuffer];
    auto event = [device newSharedEvent];
    auto options = [MTL4CommitOptions new];
    auto done = dispatch_semaphore_create(0);
    __block NSError* feedbackError = nil;
    [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback) {
        feedbackError = feedback.error;
        dispatch_semaphore_signal(done);
    }];
    id<MTL4CommandBuffer> batch[] = {commands};
    [queue commit:batch count:1 options:options];
    [queue signalEvent:event value:1];
    check([event waitUntilSignaledValue:1 timeoutMS:30000], "GPU event");
    check(dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 30*NSEC_PER_SEC)) == 0, "GPU feedback");
    check(feedbackError == nil, "Metal 4 validation/feedback", feedbackError);
    Result result;
    const auto region = MTLRegionMake2D(0, 0, W, H);
    [color getBytes:result.color.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
    [depth getBytes:result.depth.data() bytesPerRow:W*4 fromRegion:region mipmapLevel:0];
    [stencil getBytes:result.stencil.data() bytesPerRow:W fromRegion:region mipmapLevel:0];
    return result;
}

Row row(float width, float length, float x, float z, float u0 = 0, float u1 = 1, float alpha = .85f) {
    return {{width,length,.12f,1}, {u0,u1,0,1}, {alpha,0,0,0},
            {x,0,z,1520}, {0,0,0,20}};
}
} // namespace

int main() {
    @autoreleasepool {
        auto device = MTLCreateSystemDefaultDevice();
        check(device != nil, "Metal device");
        auto queue = [device newMTL4CommandQueue];
        check(queue != nil, "Metal 4 queue");
        NSError* error = nil;
        auto compiler = [device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&error];
        check(compiler != nil, "Metal 4 compiler", error);
        auto ld = [MTL4LibraryDescriptor new];
        ld.source = [NSString stringWithUTF8String:NativeGameDecalShaderSource];
        auto opts = [MTLCompileOptions new];
        opts.mathMode = MTLMathModeSafe;
        opts.mathFloatingPointFunctions = MTLMathFloatingPointFunctionsPrecise;
        ld.options = opts;
        auto library = [compiler newLibraryWithDescriptor:ld error:&error];
        check(library != nil, "original GameDecal shader", error);
        auto pd = [MTL4RenderPipelineDescriptor new];
        auto vertex = [MTL4LibraryFunctionDescriptor new], fragment = [MTL4LibraryFunctionDescriptor new];
        vertex.library = fragment.library = library;
        vertex.name = @"GameDecalGeometryVertex";
        fragment.name = @"GameDecalFragment";
        pd.vertexFunctionDescriptor = vertex;
        pd.fragmentFunctionDescriptor = fragment;
        pd.rasterSampleCount = 1;
        pd.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
        auto a = pd.colorAttachments[0];
        a.pixelFormat = MTLPixelFormatBGRA8Unorm;
        a.blendingState = MTL4BlendStateEnabled;
        a.rgbBlendOperation = a.alphaBlendOperation = MTLBlendOperationAdd;
        a.sourceRGBBlendFactor = a.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
        a.destinationRGBBlendFactor = a.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
        auto validation = [MTL4PipelineOptions new];
        validation.shaderValidation = MTLShaderValidationEnabled;
        pd.options = validation;
        auto pipeline = [compiler newRenderPipelineStateWithDescriptor:pd compilerTaskOptions:nil error:&error];
        check(pipeline != nil, "GameDecal pipeline", error);

        auto height = texture(device, MTLPixelFormatR32Float, T, T);
        auto mini = texture(device, MTLPixelFormatRGBA8Unorm, T, T);
        auto info = texture(device, MTLPixelFormatRGBA8Unorm, T, T);
        auto shadow = texture(device, MTLPixelFormatDepth32Float, T, T);
        auto groundNormal = texture(device, MTLPixelFormatRG32Float, T, T);
        auto atlas = texture(device, MTLPixelFormatRGBA8Unorm, T, T);
        auto atlasNormal = texture(device, MTLPixelFormatRGBA8Unorm, T, T);
        std::array<float, T*T> heights{}, shadows{};
        shadows.fill(1.f);
        std::array<float, T*T*2> ground{};
        std::array<std::uint32_t, T*T> lighting{}, colors{}, normals{};
        lighting.fill(0xffb0a090);
        normals.fill(0xffff8080);
        for (unsigned i = 0; i < T*T; ++i)
            colors[i] = i % T < T/2 ? 0xc02b3dcc : 0xc0db5124;
        upload(height, heights.data(), T*4); upload(shadow, shadows.data(), T*4);
        upload(groundNormal, ground.data(), T*8); upload(mini, lighting.data(), T*4);
        upload(info, lighting.data(), T*4); upload(atlas, colors.data(), T*4);
        upload(atlasNormal, normals.data(), T*4);
        auto samplerDesc = [MTLSamplerDescriptor new];
        samplerDesc.supportArgumentBuffers = YES;
        auto sampler = [device newSamplerStateWithDescriptor:samplerDesc];
        samplerDesc.compareFunction = MTLCompareFunctionLessEqual;
        auto shadowSampler = [device newSamplerStateWithDescriptor:samplerDesc];
        check(sampler && shadowSampler, "samplers");
        auto index = [device newBufferWithBytes:gameDecalGeometry::Indices.data()
                                        length:sizeof(gameDecalGeometry::Indices)
                                       options:MTLResourceStorageModeShared];
        check(index != nil, "candidate index buffer");
        const Row tiny = row(20,19,128,128), regular = row(80,66,128,128);
        const Row left = row(80,66,110,128,0,.45f), right = row(80,66,145,128,.55f,1);
        auto compare = [&](const char* name, const std::vector<Row>& rows, unsigned first, unsigned count,
                           bool visible) {
            check(first+count <= rows.size() && count > 0, "fixture row range");
            Uniform u{};
            u.viewProjection.columns[0] = {1.f/128,0,0,0};
            u.viewProjection.columns[2] = {0,1.f/128,0,0};
            u.viewProjection.columns[3] = {-1,-1,512.4f,1};
            u.viewInverse = matrix_identity_float4x4;
            u.viewInverse.columns[3] = {128,180,128,1};
            u.shadow.columns[3] = {0,0,.4f,1};
            u.time = {1590,0,53,0}; u.map = {256,256,256,256}; u.sun = {0,1,0,0};
            u.ambient = {.6f,.6f,.6f,1}; u.diffuse = {.3f,.3f,.3f,1};
            u.options = {0,0,first,0};
            auto rb = [device newBufferWithBytes:rows.data() length:rows.size()*sizeof(Row)
                                        options:MTLResourceStorageModeShared];
            auto ub = [device newBufferWithBytes:&u length:sizeof(u) options:MTLResourceStorageModeShared];
            check(rb && ub, "fixture buffers");
            auto at = [MTL4ArgumentTableDescriptor new];
            at.maxBufferBindCount = 3; at.maxTextureBindCount = 7; at.maxSamplerStateBindCount = 7;
            at.initializeBindings = YES;
            auto args = [device newArgumentTableWithDescriptor:at error:&error];
            check(args != nil, "argument table", error);
            [args setAddress:rb.gpuAddress atIndex:0]; [args setAddress:ub.gpuAddress atIndex:2];
            auto inputs = [device newResidencySetWithDescriptor:[MTLResidencySetDescriptor new] error:&error];
            check(inputs != nil, "input residency", error);
            unsigned slot = 0;
            for (auto t : {height, mini, info, shadow, groundNormal, atlas, atlasNormal}) {
                [args setTexture:t.gpuResourceID atIndex:slot];
                [args setSamplerState:(slot == 3 ? shadowSampler : sampler).gpuResourceID atIndex:slot];
                [inputs addAllocation:t];
                ++slot;
            }
            for (auto b : {rb, ub, index}) [inputs addAllocation:b];
            [inputs commit];
            const auto off = render(device, queue, pipeline, args, inputs, nil, count);
            const auto on = render(device, queue, pipeline, args, inputs, index, count);
            check(off == on, name);
            unsigned coverage = 0;
            for (unsigned i = 0; i < pixels; ++i) {
                const bool covered = off.stencil[i] == 7;
                check(covered == (off.depth[i] < clearDepth), "nonvacuous depth/stencil coverage");
                check(covered == (off.color[4*i+3] != 0), "nonvacuous color coverage");
                coverage += covered;
            }
            check((coverage > 0) == visible, "expected visible/rejected fixture");
            std::printf("PASS %s modeOFF=96vertices modeON=96UInt16 indexBytes=192 first=%u count=%u samples=1 covered=%u\n",
                        name, first, count, coverage);
            return off;
        };
        compare("tiny", {tiny}, 0, 1, true);
        const auto regularOutput = compare("regular", {regular}, 0, 1, true);
        const auto overlap = compare("overlap_blend", {left,right}, 0, 2, true);
        const auto leftOnly = compare("left_control", {left}, 0, 1, true);
        const auto rightOnly = compare("right_control", {right}, 0, 1, true);
        unsigned blended = 0;
        for (unsigned i = 0; i < pixels; ++i) {
            if (leftOnly.stencil[i] == 7 && rightOnly.stencil[i] == 7 &&
                !std::equal(overlap.color.data()+4*i, overlap.color.data()+4*i+4, rightOnly.color.data()+4*i))
                ++blended;
        }
        check(blended > 0, "observable overlap blending");
        const auto clip = compare("partial_clip", {row(80,66,248,128)}, 0, 1, true);
        check(std::count(clip.stencil.begin(), clip.stencil.end(), 7) <
              std::count(regularOutput.stencil.begin(), regularOutput.stencil.end(), 7), "clip coverage reduction");
        compare("sphere_reject", {row(80,66,960,128)}, 0, 1, false);
        compare("alpha_reject", {row(80,66,128,128,0,1,0)}, 0, 1, false);
        const auto offset = compare("nonzero_first", {tiny,regular,left}, 1, 1, true);
        check(offset == regularOutput, "nonzero-first selected the original row");
        std::printf("PASS GameDecal shader fixture, sample1, overlap_pixels=%u\n", blended);
    }
}
