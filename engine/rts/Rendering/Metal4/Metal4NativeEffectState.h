/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "Rendering/Metal4/Metal4RuntimeNative.h"
#include "Rendering/Metal4/Metal4WorldViewport.h"
#include "Rendering/Backend/NativeWorldDrawData.h"
#include "Rendering/Backend/WorldFrameData.h"
#include <algorithm>
#include <cmath>
namespace rendering::metal4::effectstate {
inline MTLBlendFactor Factor(ScreenBlendFactor x)
{
	switch(x){case ScreenBlendFactor::Zero:return MTLBlendFactorZero;case ScreenBlendFactor::One:return MTLBlendFactorOne;case ScreenBlendFactor::SourceAlpha:return MTLBlendFactorSourceAlpha;case ScreenBlendFactor::OneMinusSourceAlpha:return MTLBlendFactorOneMinusSourceAlpha;case ScreenBlendFactor::DestinationAlpha:return MTLBlendFactorDestinationAlpha;case ScreenBlendFactor::OneMinusDestinationAlpha:return MTLBlendFactorOneMinusDestinationAlpha;case ScreenBlendFactor::SourceColor:return MTLBlendFactorSourceColor;case ScreenBlendFactor::OneMinusSourceColor:return MTLBlendFactorOneMinusSourceColor;case ScreenBlendFactor::DestinationColor:return MTLBlendFactorDestinationColor;case ScreenBlendFactor::OneMinusDestinationColor:return MTLBlendFactorOneMinusDestinationColor;case ScreenBlendFactor::SourceAlphaSaturate:return MTLBlendFactorSourceAlphaSaturated;}
	throw std::runtime_error("Native world blend factor unsupported");
}
inline MTLCompareFunction Compare(ScreenAlphaCompare value)
{
	switch(value){case ScreenAlphaCompare::Never:return MTLCompareFunctionNever;case ScreenAlphaCompare::Less:return MTLCompareFunctionLess;case ScreenAlphaCompare::Equal:return MTLCompareFunctionEqual;case ScreenAlphaCompare::LessEqual:return MTLCompareFunctionLessEqual;case ScreenAlphaCompare::Greater:return MTLCompareFunctionGreater;case ScreenAlphaCompare::NotEqual:return MTLCompareFunctionNotEqual;case ScreenAlphaCompare::GreaterEqual:return MTLCompareFunctionGreaterEqual;case ScreenAlphaCompare::Always:return MTLCompareFunctionAlways;}
	throw std::runtime_error("Native world comparison unsupported");
}
inline MTLStencilOperation Operation(NativeWorldStencilOp value)
{
	switch(value){case NativeWorldStencilOp::Keep:return MTLStencilOperationKeep;case NativeWorldStencilOp::Zero:return MTLStencilOperationZero;case NativeWorldStencilOp::Replace:return MTLStencilOperationReplace;case NativeWorldStencilOp::IncrementClamp:return MTLStencilOperationIncrementClamp;case NativeWorldStencilOp::DecrementClamp:return MTLStencilOperationDecrementClamp;case NativeWorldStencilOp::Invert:return MTLStencilOperationInvert;case NativeWorldStencilOp::IncrementWrap:return MTLStencilOperationIncrementWrap;case NativeWorldStencilOp::DecrementWrap:return MTLStencilOperationDecrementWrap;}
	throw std::runtime_error("Native world stencil operation unsupported");
}
struct Raster {MTLViewport viewport;MTLScissorRect scissor;};
inline std::optional<Raster> Viewport(const WorldCamera& camera,unsigned width,unsigned height,unsigned logicalWidth,unsigned logicalHeight,const std::optional<ScreenScissor>& requested)
{
 auto mapped=MapWorldViewport(camera.viewport,logicalWidth,logicalHeight,width,height);if(!mapped)return {};
 Raster result{{mapped->viewport[0],mapped->viewport[1],mapped->viewport[2],mapped->viewport[3],0,1},{mapped->scissor[0],mapped->scissor[1],mapped->scissor[2],mapped->scissor[3]}};
 if(requested){const auto& sc=*requested;const double sx=double(width)/logicalWidth,sy=double(height)/logicalHeight;
 const auto x0=NSUInteger(std::clamp(std::floor(double(sc.x)*sx),0.,double(width))),x1=NSUInteger(std::clamp(std::ceil((double(sc.x)+sc.width)*sx),0.,double(width)));
 const auto y0=NSUInteger(std::clamp(std::floor((double(logicalHeight)-sc.y-sc.height)*sy),0.,double(height))),y1=NSUInteger(std::clamp(std::ceil((double(logicalHeight)-sc.y)*sy),0.,double(height)));
 const auto left=std::max(result.scissor.x,x0),top=std::max(result.scissor.y,y0),right=std::min(result.scissor.x+result.scissor.width,x1),bottom=std::min(result.scissor.y+result.scissor.height,y1);if(right<=left||bottom<=top)return {};result.scissor={left,top,right-left,bottom-top};}return result;
}
inline id<MTLDepthStencilState> Depth(RuntimeLease& lease,const NativeWorldDrawState& state,MTLPixelFormat stencilFormat)
{
 // An original framebuffer without stencil makes the test and writes no-ops.
 const bool stencilEnabled=state.stencilTest&&stencilFormat!=MTLPixelFormatInvalid;
 if(stencilEnabled)Require(stencilFormat==MTLPixelFormatStencil8||stencilFormat==MTLPixelFormatDepth32Float_Stencil8,"Original effect stencil needs a real paired attachment");
 auto descriptor=[MTLDepthStencilDescriptor new];descriptor.depthCompareFunction=state.depthTest?Compare(state.depthCompare):MTLCompareFunctionAlways;descriptor.depthWriteEnabled=state.depthTest&&state.depthWrite;
 if(stencilEnabled)for(bool front:{true,false}){const auto& source=front?state.frontStencil:state.backStencil;auto face=[MTLStencilDescriptor new];face.stencilCompareFunction=Compare(source.compare);face.stencilFailureOperation=Operation(source.fail);face.depthFailureOperation=Operation(source.depthFail);face.depthStencilPassOperation=Operation(source.pass);face.readMask=source.readMask;face.writeMask=source.writeMask;if(front)descriptor.frontFaceStencil=face;else descriptor.backFaceStencil=face;}
 auto result=[lease.Device()newDepthStencilStateWithDescriptor:descriptor];Require(result!=nil,"Cannot initialize original effect depth/stencil");lease.Retain(result);return result;
}
inline void RasterState(id<MTL4RenderCommandEncoder> encoder,const NativeWorldDrawState& state,const Raster& raster)
{
 [encoder setViewport:raster.viewport];[encoder setScissorRect:raster.scissor];[encoder setDepthClipMode:state.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];[encoder setFrontFacingWinding:MTLWindingCounterClockwise];[encoder setCullMode:MTLCullModeNone];[encoder setTriangleFillMode:MTLTriangleFillModeFill];[encoder setDepthBias:state.polygonOffset?state.offsetUnits:0 slopeScale:state.polygonOffset?state.offsetFactor:0 clamp:0];[encoder setStencilFrontReferenceValue:uint32_t(std::clamp(state.frontStencil.reference,0,255)) backReferenceValue:uint32_t(std::clamp(state.backStencil.reference,0,255))];
}
} // namespace rendering::metal4::effectstate
