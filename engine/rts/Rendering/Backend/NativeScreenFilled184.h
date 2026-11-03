/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering {
// Pass185 shares one filled fragment across the three eligible materials and
// alpha-enable states. Material and alpha selection use the existing uniform.
struct NativeScreenFilled184Input {
 unsigned material=0,samples=0,sampledDepthMask=0;
 bool rootScreen=false,attachmentClear=false,pointDisk=false,alphaToCoverage=false;
 bool secondaryTexture=false,ordinaryGeometry=false,sampleShading=false;
};
constexpr bool NativeScreenFilled184(const NativeScreenFilled184Input& in) {
 if(!in.rootScreen||in.samples!=1||in.sampledDepthMask||in.attachmentClear||in.pointDisk||
    in.alphaToCoverage||in.sampleShading||!in.ordinaryGeometry||in.material>2)return false;
 // FontCoverage ignores unit1 in the original common shader. Solid does not.
 return in.material==2||!in.secondaryTexture;
}
}
