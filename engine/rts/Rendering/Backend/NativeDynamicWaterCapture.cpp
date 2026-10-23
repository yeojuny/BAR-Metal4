/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeDynamicWaterCapture.h"
#include "NativeLegacyWaterProgramState.h"
#include "NativeOriginalModelContext.h"
#include "NativeTextureContext.h"
#include "NativeUIRecorder.h"
#include "Game/Camera.h"
#include "Game/CameraHandler.h"
#include "Game/GlobalUnsynced.h"
#include "Map/ReadMap.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/ShadowHandler.h"
#include "Rendering/Env/WaterRendering.h"
#include "Rendering/Env/ISky.h"
#include "Rendering/Env/SunLighting.h"
#include "Rendering/Units/UnitDrawer.h"
#include "Rendering/Textures/Bitmap.h"
#include "Sim/MoveTypes/MoveDefHandler.h"
#include "Sim/Units/Unit.h"
#include "Sim/Misc/GlobalSynced.h"
#include "System/FileSystem/FileHandler.h"
#include "System/SpringMath.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace rendering {
namespace {
constexpr int W_SIZE=5,WF_SIZE=5120,WH_SIZE=2560,WSQUARE_SIZE=5,LOD_SIZE_FACT=160;
struct Sink {
 DynamicWaterDraw draw;size_t stripStart=0;
 void AddVertexQTN(const float3& p,float u,float v,const float3& n){draw.vertices.push_back({{p.x,p.y,p.z},{u,v},{n.x,n.y,n.z}});}
 void AddVertexQT(const float3& p,float u,float v){AddVertexQTN(p,u,v,{});}
 void AddVertexQ0(float x,float y,float z){AddVertexQT(float3(x,y,z),0,0);}
 void EndStrip(){for(size_t i=stripStart+2;i<draw.vertices.size();++i){if((i-stripStart)&1)draw.indices.insert(draw.indices.end(),{uint32_t(i-1),uint32_t(i-2),uint32_t(i)});else draw.indices.insert(draw.indices.end(),{uint32_t(i-2),uint32_t(i-1),uint32_t(i)});}stripStart=draw.vertices.size();}
 void Quads(bool alternate=false){for(size_t i=0;i+3<draw.vertices.size();i+=4){if(alternate)draw.indices.insert(draw.indices.end(),{uint32_t(i+3),uint32_t(i),uint32_t(i+1),uint32_t(i+3),uint32_t(i+1),uint32_t(i+2)});else draw.indices.insert(draw.indices.end(),{uint32_t(i),uint32_t(i+1),uint32_t(i+2),uint32_t(i),uint32_t(i+2),uint32_t(i+3)});}}
};
NativeCUSTextureBinding Shading(){NativeCUSTextureBinding b;b.object=OriginalNativeTextureContext().Registry().Find(readMap->GetShadingTexture());if(!b.object)throw std::runtime_error("Dynamic water missing actual shading owner");b.knowledge=NativeCUSBindingKnowledge::BoundObject;b.enabled=b.enableObserved=true;return b;}
std::shared_ptr<const WorldTexture> Bitmap(const CBitmap& bm,const std::string& path,bool boat){
 if(bm.compressed||bm.xsize<=0||bm.ysize<=0||bm.GetMemSize()!=size_t(bm.xsize)*bm.ysize*bm.channels||(boat?bm.channels!=4:bm.channels!=1))throw std::runtime_error("Dynamic bitmap source requires actual uncompressed byte payload");
 auto image=std::make_shared<WorldTexture>();image->sourcePath=path;image->originalInternalFormat=boat?0x8058:0x8229;image->generateMipmaps=true;image->mipmappedSampling=true;
 WorldTextureMip mip;mip.width=bm.xsize;mip.height=bm.ysize;mip.bytesPerRow=mip.width*4;mip.bytes.resize(mip.bytesPerRow*mip.height);
 for(size_t i=0;i<size_t(mip.width)*mip.height;++i){if(boat){std::copy_n(bm.GetRawMem()+i*4,4,mip.bytes.begin()+i*4);mip.bytes[i*4+2]=mip.bytes[i*4+3]=mip.bytes[i*4];}else{mip.bytes[i*4]=bm.GetRawMem()[i];mip.bytes[i*4+1]=mip.bytes[i*4+2]=0;mip.bytes[i*4+3]=255;}}
 image->mips.push_back(std::move(mip));return image;
}
void VerifyPrograms(DynamicWaterSurfaceProgram selected){
 {CFileHandler file("shaders/ARB/dwAddSplash.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nOPTION ARB_fog_linear;\nTEMP temp,temp2,temp3;\n\nTEX temp, fragment.texcoord[0], texture[0], 2D;\n\nMUL result.color, temp, fragment.texcoord[1];\n\nEND")throw std::runtime_error("Dynamic runtime program differs: dwAddSplash.fp");}
 {CFileHandler file("shaders/ARB/dwAddSplash.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nATTRIB ipos = vertex.position;\nOUTPUT opos = result.position;\nTEMP temp,pos;\n\nMAD pos, ipos.xzyw, program.env[10], program.env[11];\nMOV result.texcoord[1], vertex.normal;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nMOV result.texcoord[0], vertex.texcoord[0];\nMOV result.texcoord[1].w, vertex.normal.z;\n\nEND\n")throw std::runtime_error("Dynamic runtime program differs: dwAddSplash.vp");}
 {CFileHandler file("shaders/ARB/dwDetailNormal.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nOPTION ARB_fog_linear;\nTEMP temp,temp2;\n\nTEX temp, fragment.texcoord[0], texture[0], 2D;\nMUL temp2, temp, program.env[0];\nTEX temp, fragment.texcoord[1], texture[1], 2D;\nMAD temp2, temp, program.env[1],temp2;\nTEX temp, fragment.texcoord[2], texture[2], 2D;\nMAD temp2, temp, program.env[2],temp2;\nTEX temp, fragment.texcoord[3], texture[3], 2D;\nMAD temp2, temp, program.env[3],temp2;\nTEX temp, fragment.texcoord[4], texture[4], 2D;\nMAD temp2, temp, program.env[4],temp2;\nTEX temp, fragment.texcoord[5], texture[5], 2D;\nMAD temp2, temp, program.env[5],temp2;\nTEX temp, fragment.texcoord[6], texture[6], 2D;\nMAD temp2, temp, program.env[6],temp2;\nTEX temp, fragment.texcoord[7], texture[7], 2D;\nMAD temp2, temp, program.env[7],temp2;\n\nMOV result.color, temp2;\n\nEND")throw std::runtime_error("Dynamic runtime program differs: dwDetailNormal.fp");}
 {CFileHandler file("shaders/ARB/dwDetailNormal.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nATTRIB pos = vertex.position;\nOUTPUT opos = result.position;\nTEMP temp;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nDP3 temp, vertex.texcoord, program.env[10];\nMAD result.texcoord[0], program.env[10].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[11];\nMAD result.texcoord[1], program.env[11].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[12];\nMAD result.texcoord[2], program.env[12].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[13];\nMAD result.texcoord[3], program.env[13].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[14];\nMAD result.texcoord[4], program.env[14].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[15];\nMAD result.texcoord[5], program.env[15].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[16];\nMAD result.texcoord[6], program.env[16].w, program.env[9].x, temp;\nDP3 temp, vertex.texcoord, program.env[17];\nMAD result.texcoord[7], program.env[17].w, program.env[9].x, temp;\n\nEND\n")throw std::runtime_error("Dynamic runtime program differs: dwDetailNormal.vp");}
 {CFileHandler file("shaders/ARB/waterDyn.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nOPTION ARB_fog_linear;\nOPTION ARB_fragment_program_shadow;\nTEMP temp,camdir,reflectdir,waterNormal,angle,reflectColor;\nTEMP refractdir,refractColor,clearcolor,specularvalue;\nTEMP groundHeight,refractMul,foamColor,lightColor,foamIntensity;\nTEMP detailNormal,shadow,camdist,waterNormal2,angle2,reflectdir2;\n\nTEX shadow, fragment.texcoord[1], texture[7], SHADOW2D;\n\nSUB camdir, fragment.texcoord, program.env[1];\nDP3 temp.x, camdir, camdir;\nRSQ temp.x, temp.x;\nMUL camdir, camdir, temp.x;\nRCP camdist.x, temp.x;\n\nTEX waterNormal, fragment.texcoord[2], texture[0], 2D;\nMAD_SAT foamIntensity.x, waterNormal.w, 5, -1;\nTEX groundHeight, fragment.texcoord[4], texture[4], 2D;\nMAD groundHeight.x, groundHeight.a, 0.4, -0.4;\nTEX temp, fragment.texcoord[3], texture[5], 2D;\nMUL foamIntensity.x , foamIntensity.x, temp.x;\nTEX detailNormal, fragment.texcoord[3], texture[6], 2D;\n\nMAD waterNormal, detailNormal.xzyw, {1,0,1,0}, waterNormal;\nDP3 temp.x, waterNormal, waterNormal;\nRSQ temp.x, temp.x;\nMUL waterNormal, waterNormal, temp.x;\n\nDP3 angle.x, camdir, waterNormal;\nMUL temp.x, angle.x, -2;\nMAD reflectdir, waterNormal, temp.x, camdir;\n\nDP3_SAT specularvalue.x, reflectdir, program.env[9];\nPOW temp, specularvalue.x, {16,0,0,0}.x;\nPOW specularvalue, specularvalue.x, {4096,0,0,0}.x;\nMUL specularvalue, specularvalue, 10;\nMUL specularvalue, specularvalue, shadow.x;\nLRP shadow.x, 0.3, 1, shadow.x;\nMUL temp, temp, shadow.x;\nMAD specularvalue, temp, 0.5, specularvalue;\n\nMAD waterNormal2, camdist.x, {0,0.003,0,0}, waterNormal;\nDP3 temp.x, waterNormal2, waterNormal2;\nRSQ temp.x, temp.x;\nMUL waterNormal2, waterNormal2, temp.x;\n\nDP3 angle2.x, camdir, waterNormal2;\nMUL temp.x, angle2.x, -2;\nMAD reflectdir2, waterNormal2, temp.x, camdir;\nMAX reflectdir2.y, reflectdir2.y, 0;\n\nDP3 temp.x, reflectdir2, program.env[5];\nRCP temp.x, temp.x;\nMUL reflectdir2, reflectdir2, temp.x;\n\nDP3 temp.x, reflectdir2, program.env[2];\nDP3 temp.y, reflectdir2, program.env[3];\nMAD temp, temp, program.env[4], {0.5, 0.5, 0, 0};\nTEX reflectColor, temp, texture[1], 2D;\nMAD reflectColor, specularvalue, program.env[10], reflectColor;\n\nMUL refractMul, groundHeight.x, {1,0,1,0};\nMAD refractdir,waterNormal2, refractMul,camdir;\nDP3 temp.x, refractdir, program.env[15];\nRCP temp.x, temp.x;\nMUL refractdir, refractdir, temp.x;\n\nDP3 temp.x, refractdir, program.env[12];\nDP3 temp.y, refractdir, program.env[13];\nMAD temp, temp, program.env[14], {0.5, 0.5, 0, 0};\nTEX refractColor, temp, texture[3], 2D;\n\nADD temp.x, 1, angle.x;\nMUL temp.y, temp.x, temp.x;\nMUL temp.y, temp.y, temp.y;\nMUL temp.x, temp.x, temp.y;\nMAD temp.x, temp.x, program.env[6].y, program.env[6].x; \nLRP clearcolor, temp.x, reflectColor, refractColor;\n\nDP3_SAT temp.x, waterNormal, program.env[9];\nMUL temp.x, temp.x, shadow.x;\nMAD lightColor, temp.x, program.env[10], program.env[11];\nMUL temp, lightColor, program.env[8];\nLRP temp, program.env[7].x, temp, clearcolor;\n\nLRP result.color, foamIntensity.x, lightColor, temp;\n\n#MUL temp, fragment.texcoord[2],1;\n#TEX temp, temp, texture[0], 2D;\n#MAD result.color, temp, 0.5, 0.5;\n\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDyn.fp");}
 if(selected==DynamicWaterSurfaceProgram::VertexTexture){CFileHandler file("shaders/ARB/waterDyn.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\nOPTION NV_vertex_program3;\n\nPARAM mat[4] = { state.matrix.mvp };\nPARAM mat3[4] = { state.matrix.program[0] };\nOUTPUT opos = result.position;\nTEMP temp,temp2,pos;\n\nMAD temp, vertex.position.xzyw, program.env[10], program.env[11];\nTEX temp2, temp, texture[2], 2D;\n\nMAD result.texcoord[2], vertex.position.xzyw, program.env[12], program.env[13];\nMUL result.texcoord[3], vertex.position.xzyw, {0.004,0.004,0,0};\nMUL result.texcoord[4], vertex.position.xzyw, program.env[14];\n\nMOV pos, vertex.position;\nMOV result.color, vertex.color;\n\nADD pos.y, pos.y, temp2.x;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nDP4 result.fogcoord.x, pos, mat[2];\n\n#tex coords for shadow texture\nDP4 temp.x, pos, mat3[0];\nDP4 temp.y, pos, mat3[1];\n\nABS temp2,temp;\nADD temp2,temp2,program.env[17];\nRSQ temp2.x, temp2.x;\nRSQ temp2.y, temp2.y;\nADD temp2,temp2,program.env[18];\nMAD result.texcoord[1], temp, temp2,program.env[16];\n\nDP4 result.texcoord[1].z, pos, mat3[2];\nDP4 result.texcoord[1].w, pos, mat3[3];\n\nMOV result.texcoord, pos;\n\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDyn.vp");}
 if(selected==DynamicWaterSurfaceProgram::NoVertexTexture){CFileHandler file("shaders/ARB/waterDynNT.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nPARAM mat3[4] = { state.matrix.program[0] };\nOUTPUT opos = result.position;\nTEMP temp,temp2,pos;\n\nMAD result.texcoord[2], vertex.position.xzyw, program.env[12], program.env[13];\nMUL result.texcoord[3], vertex.position.xzyw, {0.017,0.017,0,0};\nMUL result.texcoord[4], vertex.position.xzyw, program.env[14];\n\nMOV pos, vertex.position;\nMOV result.color, vertex.color;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nDP4 result.fogcoord.x, pos, mat[2];\n\n#tex coords for shadow texture\nDP4 temp.x, pos, mat3[0];\nDP4 temp.y, pos, mat3[1];\n\nABS temp2,temp;\nADD temp2,temp2,program.env[17];\nRSQ temp2.x, temp2.x;\nRSQ temp2.y, temp2.y;\nADD temp2,temp2,program.env[18];\nMAD result.texcoord[1], temp, temp2,program.env[16];\n\nDP4 result.texcoord[1].z, pos, mat3[2];\nDP4 result.texcoord[1].w, pos, mat3[3];\n\nMOV result.texcoord, pos;\n\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDynNT.vp");}
 {CFileHandler file("shaders/ARB/waterDynNormal.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nTEMP cross1,cross2,normal,temp;\nTEMP height1,height2,height3,height4,foam;\n\nTEX height1, fragment.texcoord[0], texture[0], 2D;\nTEX height2, fragment.texcoord[1], texture[1], 2D;\nTEX height3, fragment.texcoord[2], texture[2], 2D;\nTEX height4, fragment.texcoord[3], texture[3], 2D;\nADD foam.x, height1.y, height2.y;\n\nMOV cross1,program.env[0];\nSUB cross1.y, height1.x, height2.x;\nMOV cross2,program.env[1];\nSUB cross2.y, height3.x, height4.x;\n\nXPD normal, cross1,cross2;\nDP3 temp.x, normal, normal;\nRSQ temp.x, temp.x;\nMUL normal, normal, temp.x;\n\nMOV result.color, normal;\nMUL result.color.w, foam.x, 0.5;\n\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDynNormal.fp");}
 {CFileHandler file("shaders/ARB/waterDynNormal.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nATTRIB pos = vertex.position;\nOUTPUT opos = result.position;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nMAD result.texcoord[0],  program.env[9],  1, vertex.texcoord;\nMAD result.texcoord[1],  program.env[9], -1, vertex.texcoord;\nMAD result.texcoord[2],  program.env[11], 1, vertex.texcoord;\nMAD result.texcoord[3],  program.env[11],-1, vertex.texcoord;\n\nEND\n")throw std::runtime_error("Dynamic runtime program differs: waterDynNormal.vp");}
 {CFileHandler file("shaders/ARB/waterDynWave.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nTEMP temp;\nTEMP old,midFlow,flow1,flow2,flow3,flow4,flow,backgroundWave;\n\nTEX old, fragment.texcoord[0], texture[0], 2D;\nTEX midFlow, fragment.texcoord[1], texture[1], 2D;\nTEX flow1, fragment.texcoord[2], texture[2], 2D;\nTEX flow2, fragment.texcoord[3], texture[3], 2D;\nTEX flow3, fragment.texcoord[4], texture[4], 2D;\nTEX flow4, fragment.texcoord[5], texture[5], 2D;\nTEX backgroundWave, fragment.texcoord[6], texture[6], 2D;\n\nSUB old.x, old.x, old.z;\n\nDP4 temp.x, midFlow,{1,1,1,1};\nSUB old.x, old.x, temp.x;\n\nMOV flow.x, flow1.x;\nMOV flow.y, flow2.y;\nMOV flow.z, flow3.z;\nMOV flow.w, flow4.w;\n\nDP4 temp.x, flow,{1,1,1,1};\nADD old.x, old.x, temp.x;\n\nSUB temp, midFlow, flow;\nDP4 temp.x, temp, temp;\n\nMAD old.y, temp.x, 0.2, old.y;\nMUL_SAT old.y, old.y, 0.99;\n\nMIN temp.x, old.w, 2;\nMUL_SAT temp.x, temp.x, 0.2;\n\nLRP old.x, 0.995, old.x, backgroundWave.z;\nLRP old.x, temp.x, -old.w, old.x;\n\nMUL result.color, old, {1,1,0,0};\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDynWave.fp");}
 {CFileHandler file("shaders/ARB/waterDynWave.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nATTRIB pos = vertex.position;\nOUTPUT opos = result.position;\nTEMP temp;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\n\nADD result.texcoord[0], vertex.texcoord, program.env[14];\n\nMOV result.texcoord[1], vertex.texcoord;\nADD result.texcoord[2], vertex.texcoord, program.env[8];\nADD result.texcoord[3], vertex.texcoord, program.env[9];\nADD result.texcoord[4], vertex.texcoord, program.env[10];\nADD result.texcoord[5], vertex.texcoord, program.env[11];\nMAD result.texcoord[6], vertex.texcoord, 4, program.env[15];\n\nEND\n")throw std::runtime_error("Dynamic runtime program differs: waterDynWave.vp");}
 {CFileHandler file("shaders/ARB/waterDynWave2.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nTEMP temp;\nTEMP old,midHeight,height1,height2,height3,height4,gheight;\n\nTEX old, fragment.texcoord[0], texture[0], 2D;\nTEX midHeight, fragment.texcoord[0], texture[1], 2D;\nTEX height1, fragment.texcoord[1], texture[2], 2D;\nTEX height2, fragment.texcoord[2], texture[3], 2D;\nTEX height3, fragment.texcoord[3], texture[4], 2D;\nTEX height4, fragment.texcoord[4], texture[5], 2D;\nTEX gheight, fragment.texcoord[5], texture[6], 2D;\n\nSUB temp.x, midHeight.x, height1.x;\nMAD old.x, temp.x, 0.02, old.x;\nSUB temp.x, midHeight.x, height2.x;\nMAD old.y, temp.x, 0.02, old.y;\nSUB temp.x, midHeight.x, height3.x;\nMAD old.z, temp.x, 0.02, old.z;\nSUB temp.x, midHeight.x, height4.x;\nMAD old.w, temp.x, 0.02, old.w;\n\nLRP temp.x, gheight.w, 0.008, 0.998;\nMUL old, old, temp.x;\n\nMOV result.color, old;\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDynWave2.fp");}
 {CFileHandler file("shaders/ARB/waterDynWave2.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nATTRIB pos = vertex.position;\nOUTPUT opos = result.position;\nTEMP temp;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nADD temp, vertex.texcoord, program.env[14];\n\nMOV result.texcoord[0], temp;\n\nSUB result.texcoord[1], temp, program.env[8];\nSUB result.texcoord[2], temp, program.env[9];\nSUB result.texcoord[3], temp, program.env[10];\nSUB result.texcoord[4], temp, program.env[11];\nMAD result.texcoord[5], vertex.texcoord, program.env[12], program.env[13];\n\nEND\n")throw std::runtime_error("Dynamic runtime program differs: waterDynWave2.vp");}
 {CFileHandler file("shaders/ARB/waterDynWave3.fp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBfp1.0\nTEMP height;\n\nTEX height, fragment.texcoord[0], texture[0], 2D;\n\nMOV result.color, height.x;\nEND")throw std::runtime_error("Dynamic runtime program differs: waterDynWave3.fp");}
 {CFileHandler file("shaders/ARB/waterDynWave3.vp");std::string bytes;if(!file.FileExists()||!file.LoadStringData(bytes)||bytes!="!!ARBvp1.0\n\nPARAM mat[4] = { state.matrix.mvp };\nATTRIB pos = vertex.position;\nOUTPUT opos = result.position;\n\nDP4 opos.x, pos, mat[0];\nDP4 opos.y, pos, mat[1];\nDP4 opos.z, pos, mat[2];\nDP4 opos.w, pos, mat[3];\n\nMOV result.texcoord[0], vertex.texcoord;\n\nEND\n")throw std::runtime_error("Dynamic runtime program differs: waterDynWave3.vp");}
}
// CDynWater::Update explicitly disables depth/alpha tests, depth writes and
// blending; AddShipWakes also disables culling before every update pass.
// UpdateWater's derived passes disable tests but inherit culling. Neither
// entrypoint resets the following actual inherited states. The private FBO
// has color only and no multisample storage: stencil/depth tests and sample
// coverage cannot affect these writes. These non-position-invariant ARB VPs
// ignore user clipping (ARB_vertex_program section2.11). All positions have
// z=0, so depth clamp/offset also cannot change color coverage.
void RequireFloatRaster(bool sourceDisablesCull) {
 const auto w=ui::GetRecorder().CurrentWorldState();
 const auto d=ui::GetRecorder().CurrentState().draw;
 if ((!sourceDisablesCull&&w.cullEnabled)||
  w.frontPolygonMode!=NativePolygonMode::Fill||w.backPolygonMode!=NativePolygonMode::Fill||d.scissor||
  w.colorWrite!=std::array<bool,4>{true,true,true,true}||d.colorWrite!=std::array<bool,4>{true,true,true,true}||
  d.blend.colorEquation!=ScreenBlendEquation::Add||d.blend.alphaEquation!=ScreenBlendEquation::Add)
  throw std::runtime_error("Dynamic float pass has unsupported actual inherited raster state");
}
void Detail(DynamicWaterDraw& draw,int frame){
 draw.pass=DynamicWaterPass::DetailNormal;const float swh=.05f,lwh=1.f;
 draw.vertexEnv[9]={float(frame), 0, 0, 0};
 draw.vertexEnv[10]={5, 0, 0, 1.0f/120};
 draw.vertexEnv[11]={14, 0, 0, 1.0f/90};
 draw.vertexEnv[12]={29, 0, 0, 1.0f/55};
 draw.vertexEnv[13]={9, 4, 0, 1.0f/100};
 draw.vertexEnv[14]={-5, 14, 0, 1.0f/90};
 draw.vertexEnv[15]={27, 27, 0, 1.0f/75};
 draw.vertexEnv[16]={-3, -5, 0, 1.0f/100};
 draw.vertexEnv[17]={-10, 24, 0, 1.0f/60};
 auto& programState=OriginalNativeLegacyWaterProgramState();
 programState.ObserveVertexEnvironment(0x8620,16,draw.vertexEnv[16].data());
 programState.ObserveVertexEnvironment(0x8620,17,draw.vertexEnv[17].data());
 draw.fragmentEnv[0]={0.2f*swh, 0.0f*swh, 0.7f*lwh, 0};
 draw.fragmentEnv[1]={0.2f*swh, 0.0f*swh, 0.7f*lwh, 0};
 draw.fragmentEnv[2]={0.2f*swh, 0.0f*swh, 0.7f*lwh, 0};
 draw.fragmentEnv[3]={0.2f*swh, 0.01f*swh, 0.4f*lwh, 0};
 draw.fragmentEnv[4]={0.07f*swh, 0.2f*swh, 0.7f*lwh, 0};
 draw.fragmentEnv[5]={0.2f*swh, 0.2f*swh, 0.7f*lwh, 0};
 draw.fragmentEnv[6]={0.12f*swh, 0.2f*swh, 0.7f*lwh, 0};
 draw.fragmentEnv[7]={0.08f*swh, 0.2f*swh, 0.7f*lwh, 0};
 Sink s;s.AddVertexQT({0,0,0},0,0);s.AddVertexQT({0,1,0},0,1);s.AddVertexQT({1,1,0},1,1);s.AddVertexQT({1,0,0},1,0);s.Quads();draw.vertices=std::move(s.draw.vertices);draw.indices=std::move(s.draw.indices);
}
void Square(std::vector<DynamicWaterDraw>& draws,const DynamicWaterDraw& prototype,float x0,float y0,float x1,float y1,uint32_t zero){
 Sink s;s.draw=prototype;s.draw.zeroTextureMask=zero;const float start=.1f/1024,end=1023.9f/1024,diff=end-start;
 s.AddVertexQT({x0,y0,0},start+x0*diff,start+y0*diff);s.AddVertexQT({x0,y1,0},start+x0*diff,start+y1*diff);s.AddVertexQT({x1,y1,0},start+x1*diff,start+y1*diff);s.AddVertexQT({x1,y0,0},start+x1*diff,start+y0*diff);s.Quads();draws.push_back(std::move(s.draw));
}
void DomainSquares(std::vector<DynamicWaterDraw>& draws,const DynamicWaterDraw& prototype,float dx,float dy,uint32_t zero){
 const float x0=std::max(0.f,-dx/WF_SIZE),y0=std::max(0.f,-dy/WF_SIZE),x1=std::min(1.f,1-dx/WF_SIZE),y1=std::min(1.f,1-dy/WF_SIZE);
 Square(draws,prototype,x0,y0,x1,y1,0);
 if(x0>0)Square(draws,prototype,0,0,x0,1,zero);else if(x1<1)Square(draws,prototype,x1,0,1,1,zero);
 if(y0>0)Square(draws,prototype,x0,0,x1,y0,zero);else if(y1<1)Square(draws,prototype,x0,y1,x1,1,zero);
}
}
struct NativeDynamicWaterCapture::Impl {
 struct Explosion{float3 pos;float strength,radius;};
 std::shared_ptr<const NativeDynamicWaterMaterial> material;
 DynamicWaterSourceProfile profile;
 float3 camPosBig{2048,0,2048};
 std::vector<Explosion> explosions;
 std::deque<std::shared_ptr<const NativeDynamicWaterUpdate>> pending;
 uint64_t sequence=0,retired=0;bool firstDraw=true;
 std::shared_ptr<const NativeDynamicWaterFrame> reserved,finalized;
};
NativeDynamicWaterCapture::NativeDynamicWaterCapture():impl(std::make_unique<Impl>()){}
NativeDynamicWaterCapture::~NativeDynamicWaterCapture()=default;
bool NativeDynamicWaterCapture::Initialized()const{return bool(impl->material);}
void NativeDynamicWaterCapture::Reset(){impl=std::make_unique<Impl>();}
void NativeDynamicWaterCapture::Initialize(const DynamicWaterSourceProfile& profile){
 if(Initialized())throw std::logic_error("Dynamic source already initialized");
 if(profile.surfaceProgram==DynamicWaterSurfaceProgram::Unobserved)throw std::runtime_error("Dynamic original surface-program capability is unobserved");
 VerifyPrograms(profile.surfaceProgram);static uint64_t generation=0;if(generation==UINT64_MAX)throw std::overflow_error("Dynamic material generation overflow");
 auto material=std::make_shared<NativeDynamicWaterMaterial>();material->generation=++generation;material->refractionExtent=globalRendering->viewSizeY>=1024?1024:512;
 float* temp=material->rawBump.data();
	for (int y = 0; y < 64; ++y) {
		for (int x = 0; x < 64; ++x) {
			temp[(y*64 + x)*4 + 0] = std::sin(x*math::TWOPI/64.0f) + ((x < 32) ? -1 : 1)*0.3f;
			temp[(y*64 + x)*4 + 1] = temp[(y*64 + x)*4 + 0];
			temp[(y*64 + x)*4 + 2] = std::cos(x*math::TWOPI/64.0f) + ((x < 32) ? (16 - x) : (x - 48))/16.0f*0.3f;
			temp[(y*64 + x)*4 + 3] = 0;
		}
	}
 temp=material->splash.data();
	for (int y = 0; y < 64; ++y) {
		const float dy = y - 31.5f;
		for (int x = 0; x < 64; ++x) {
			const float dx = x-31.5f;
			const float dist = std::sqrt(dx*dx + dy*dy);
			temp[(y*64 + x)*4 + 0] = std::max(0.0f, 1 - dist/30.f) * std::max(0.0f, 1 - dist/30.f);
			temp[(y*64 + x)*4 + 1] = std::max(0.0f, 1 - dist/30.f);
			temp[(y*64 + x)*4 + 2] = std::max(0.0f, 1 - dist/30.f) * std::max(0.0f, 1 - dist/30.f);
			temp[(y*64 + x)*4 + 3] = 0;
		}
	}

 CBitmap boat,hover,foam;if(!boat.Load("bitmaps/boatshape.bmp"))throw std::runtime_error("Dynamic boat bitmap missing");
 if(!hover.Load("bitmaps/hovershape.bmp"))throw std::runtime_error("Original hover load failed"); // Source still creates empty bm2 => texture0.
 if(!foam.LoadGrayscale(waterRendering->foamTexture))throw std::runtime_error("Dynamic foam fallback source is outside validated coverage");
 material->boat=Bitmap(boat,"bitmaps/boatshape.bmp",true);material->foam=Bitmap(foam,waterRendering->foamTexture,false);
 impl->profile=profile;impl->material=std::move(material);
}
void NativeDynamicWaterCapture::AddExplosion(const float3& pos,float strength,float size){
 if(!Initialized())throw std::logic_error("Dynamic explosion without source owner");
 if(pos.y>size||size<8)return;impl->explosions.push_back({pos,std::min(size*20,strength),size});
}
void NativeDynamicWaterCapture::Update(){
 if(!Initialized()||!camera)throw std::logic_error("Dynamic update missing source owner/camera");
 if(impl->reserved)throw std::logic_error("Dynamic Update cannot overtake unretired draw reservation");
 if(!waterRendering->forceRendering&&!readMap->HasVisibleWater())return;
 if(impl->sequence==UINT64_MAX)throw std::overflow_error("Dynamic update sequence overflow");
 const float3 oldCamPosBig=impl->camPosBig;float3 camPosBig=oldCamPosBig;
 camPosBig.x=std::floor(std::max(float(WH_SIZE),std::min(float(mapDims.mapx*SQUARE_SIZE-WH_SIZE),camera->GetPos().x))/(W_SIZE*16))*(W_SIZE*16);
 camPosBig.z=std::floor(std::max(float(WH_SIZE),std::min(float(mapDims.mapy*SQUARE_SIZE-WH_SIZE),camera->GetPos().z))/(W_SIZE*16))*(W_SIZE*16);
 auto update=std::make_shared<NativeDynamicWaterUpdate>();update->sequence=impl->sequence+1;update->materialGeneration=impl->material->generation;
 update->sourceDrawFrame=globalRendering->drawFrame;update->simulationFrame=gs->frameNum;update->profile=impl->profile;
 RequireFloatRaster(true);update->profile.inheritedRasterObserved=true;
 update->oldDomain={oldCamPosBig.x,oldCamPosBig.y,oldCamPosBig.z};update->domain={camPosBig.x,camPosBig.y,camPosBig.z};update->shading=Shading();
 update->fogColor=OriginalNativeModelContext().fog.color;update->fogParameters=OriginalNativeModelContext().fog.parameters;
 auto& history=OriginalNativeTextureContext().History();history.PushAttributes(true,false);history.BindDefault(0,NativeCUSTextureTarget::Image2D);update->hoverDefault=history.Binding(0,NativeCUSTextureTarget::Image2D);history.PopAttributes();
 Sink boats,hovers;Sink* va=&boats;Sink* va2=&hovers;
	{
		const auto& units = CUnitDrawer::GetUnsortedUnits();


		for (const CUnit* unit: units) {
			const MoveDef* moveDef = unit->moveDef;

			if (moveDef == NULL)
				continue;

			if (moveDef->speedModClass == MoveDef::Hover) {
				// hovercraft
				const float3& pos = unit->pos;

				if ((std::fabs(pos.x - camPosBig.x) > (WH_SIZE - 50)) ||
					(std::fabs(pos.z - camPosBig.z) > (WH_SIZE - 50)))
				{
					continue;
				}
				if (!(unit->losStatus[gu->myAllyTeam] & LOS_INLOS) && !gu->spectatingFullView)
					continue;

				if ((pos.y > -4.0f) && (pos.y < 4.0f)) {
					const float3 frontAdd = unit->frontdir * unit->radius * 0.75f;
					const float3 sideAdd = unit->rightdir * unit->radius * 0.75f;
					const float depth = std::sqrt(std::sqrt(unit->mass)) * 0.4f;
					const float3 n(depth, 0.05f * depth, depth);

					va2->AddVertexQTN(pos + frontAdd + sideAdd, 0, 0, n);
					va2->AddVertexQTN(pos + frontAdd - sideAdd, 1, 0, n);
					va2->AddVertexQTN(pos - frontAdd - sideAdd, 1, 1, n);
					va2->AddVertexQTN(pos - frontAdd + sideAdd, 0, 1, n);
				}
			} else if (moveDef->speedModClass == MoveDef::Ship) {
				// surface ship
				const float3& pos = unit->pos;

				if ((std::fabs(pos.x - camPosBig.x) > (WH_SIZE - 50)) ||
					(std::fabs(pos.z - camPosBig.z) > (WH_SIZE - 50)))
				{
					continue;
				}
				if (!(unit->losStatus[gu->myAllyTeam] & LOS_INLOS) && !gu->spectatingFullView)
					continue;

				// skip submarines (which have deep waterlines)
				if (unit->IsUnderWater() || !unit->IsInWater())
					continue;

				const float3 frontAdd = unit->frontdir * unit->radius * 0.75f;
				const float3 sideAdd = unit->rightdir * unit->radius * 0.18f;
				const float depth = std::sqrt(std::sqrt(unit->mass));
				const float3 n(depth, 0.04f * unit->speed.Length2D() * depth, depth);

				va->AddVertexQTN(pos + frontAdd + sideAdd, 0, 0, n);
				va->AddVertexQTN(pos + frontAdd - sideAdd, 1, 0, n);
				va->AddVertexQTN(pos - frontAdd - sideAdd, 1, 1, n);
				va->AddVertexQTN(pos - frontAdd + sideAdd, 0, 1, n);
			}
		}
	}
 boats.draw.pass=DynamicWaterPass::BoatWake;hovers.draw.pass=DynamicWaterPass::HoverWake;
 for(auto* s:{&boats,&hovers}){s->draw.vertexEnv[10]={1.f/WF_SIZE,1.f/WF_SIZE,0,1};s->draw.vertexEnv[11]={-(oldCamPosBig.x-WH_SIZE)/WF_SIZE,-(oldCamPosBig.z-WH_SIZE)/WF_SIZE,0,0};s->Quads();update->draws.push_back(std::move(s->draw));}
 Sink splashes;va=&splashes;auto explosions=impl->explosions;using Explosion=Impl::Explosion;
	for (std::vector<Explosion>::iterator ei = explosions.begin(); ei != explosions.end(); ++ei) {
		Explosion& explo = *ei;
		float3 pos = explo.pos;
		if ((std::fabs(pos.x - camPosBig.x) > (WH_SIZE - 50))
				|| (std::fabs(pos.z - camPosBig.z) > (WH_SIZE - 50)))
		{
			continue;
		}
		float inv = 1.01f;
		if (pos.y < 0) {
			if (pos.y < -explo.radius*0.5f) {
				inv = 0.99f;
			}
			pos.y = pos.y*-0.5f;
		}

		float size = explo.radius - pos.y;
		if (size < 8) {
			continue;
		}
		float strength = explo.strength * (size / explo.radius)*0.5f;

		const float3 n(strength, strength*0.005f, strength*inv);

		va->AddVertexQTN(pos + (float3(1.0f,  0.0f, 1.0f) * size),  0, 0, n);
		va->AddVertexQTN(pos + (float3(-1.0f, 0.0f, 1.0f) * size),  1, 0, n);
		va->AddVertexQTN(pos + (float3(-1.0f, 0.0f, -1.0f) * size), 1, 1, n);
		va->AddVertexQTN(pos + (float3(1.0f,  0.0f, -1.0f) * size), 0, 1, n);
	}
 splashes.draw.pass=DynamicWaterPass::Explosion;splashes.draw.vertexEnv[10]={1.f/WF_SIZE,1.f/WF_SIZE,0,1};splashes.draw.vertexEnv[11]={-(oldCamPosBig.x-WH_SIZE)/WF_SIZE,-(oldCamPosBig.z-WH_SIZE)/WF_SIZE,0,0};splashes.Quads();update->draws.push_back(std::move(splashes.draw));
 DynamicWaterDraw detail;Detail(detail,gs->frameNum);update->draws.push_back(std::move(detail));
 DynamicWaterDraw flow;flow.pass=DynamicWaterPass::Flow;const float dx=camPosBig.x-oldCamPosBig.x,dy=camPosBig.z-oldCamPosBig.z;
 flow.vertexEnv[8]={-1.0f/1024, 1.0f/1024, 0,0};
 flow.vertexEnv[9]={0,          1.0f/1024, 0,0};
 flow.vertexEnv[10]={1.0f/1024,  1.0f/1024, 0,0};
 flow.vertexEnv[11]={1.0f/1024,  0, 0,0};
 flow.vertexEnv[12]={float(WF_SIZE)/(mapDims.mapxp1*SQUARE_SIZE), float(WF_SIZE)/(mapDims.mapyp1*SQUARE_SIZE), 0, 0};
 flow.vertexEnv[13]={(camPosBig.x-WH_SIZE)/(mapDims.pwr2mapx*SQUARE_SIZE), (camPosBig.z-WH_SIZE)/(mapDims.pwr2mapy*SQUARE_SIZE), 0, 0};
 flow.vertexEnv[14]={dx/WF_SIZE, dy/WF_SIZE, 0, 0};
 flow.vertexEnv[15]={(camPosBig.x-WH_SIZE)/WF_SIZE*4, (camPosBig.x-WH_SIZE)/WF_SIZE*4, 0, 0};
 DomainSquares(update->draws,flow,dx,dy,0x3f);DynamicWaterDraw height=flow;height.pass=DynamicWaterPass::HeightFoam;DomainSquares(update->draws,height,dx,dy,1);
 Sink normal;normal.draw=flow;normal.draw.pass=DynamicWaterPass::Normal;normal.draw.fragmentEnv[0]={0,0,W_SIZE*2,0};normal.draw.fragmentEnv[1]={W_SIZE*2,0,0,0};
 const float start=.1f/1024,end=1023.9f/1024;normal.AddVertexQT({0,0,0},start,start);normal.AddVertexQT({0,1,0},start,end);normal.AddVertexQT({1,1,0},end,end);normal.AddVertexQT({1,0,0},end,start);normal.Quads();update->draws.push_back(std::move(normal.draw));
 // Commit source ownership only after full immutable capture succeeds. GPU retries
 // reuse this update; they never re-enumerate units or consume explosions again.
 impl->pending.push_back(update);impl->sequence=update->sequence;impl->camPosBig=camPosBig;impl->explosions.clear();
 auto& recorder=ui::GetRecorder();recorder.UpdateWorldState([](NativeWorldDrawState& w){w.depthTest=true;w.depthWrite=true;w.cullEnabled=false;},false);
 recorder.UpdateBlend([](ScreenBlendState& b){b.enabled=false;b.sourceColor=b.sourceAlpha=ScreenBlendFactor::SourceAlpha;b.destinationColor=b.destinationAlpha=ScreenBlendFactor::OneMinusSourceAlpha;});
 auto alpha=recorder.CurrentState().draw.alphaTest;alpha.enabled=false;recorder.SetAlphaTest(alpha);
}
std::shared_ptr<const NativeDynamicWaterFrame> NativeDynamicWaterCapture::CaptureDerived(uint64_t sourceDrawFrame){
 if(!Initialized()||!camera)throw std::logic_error("Dynamic draw missing actual source owner");
 if(impl->reserved){if(impl->reserved->sourceDrawFrame!=sourceDrawFrame)throw std::logic_error("Dynamic draw cannot replace an unretired reservation");return impl->reserved;}
 auto frame=std::make_shared<NativeDynamicWaterFrame>();frame->sourceDrawFrame=sourceDrawFrame;frame->throughUpdate=impl->sequence;frame->material=impl->material;frame->profile=impl->profile;
 RequireFloatRaster(false);frame->profile.inheritedRasterObserved=true;
 frame->updates.assign(impl->pending.begin(),impl->pending.end());frame->shading=Shading();
 const auto& camPosBig=impl->camPosBig;const int camPosX=int(camera->GetPos().x/W_SIZE),camPosZ=int(camera->GetPos().z/W_SIZE);
 frame->derivedCameraCell={camPosX,camPosZ};
 const float startx=(camPosX-120)/1024.f-(camPosBig.x-WH_SIZE)/WF_SIZE,startz=(camPosZ-120)/1024.f-(camPosBig.z-WH_SIZE)/WF_SIZE;
 const float endx=(camPosX+120)/1024.f-(camPosBig.x-WH_SIZE)/WF_SIZE,endz=(camPosZ+120)/1024.f-(camPosBig.z-WH_SIZE)/WF_SIZE;
 Sink height;height.draw.pass=DynamicWaterPass::DerivedHeight;constexpr float first=8.f/256,last=248.f/256;
 height.AddVertexQT({first,first,0},startx,startz);height.AddVertexQT({first,last,0},startx,endz);height.AddVertexQT({last,last,0},endx,endz);height.AddVertexQT({last,first,0},endx,startz);height.Quads();frame->derived.push_back(std::move(height.draw));
 if(impl->firstDraw){DynamicWaterDraw detail;Detail(detail,gs->frameNum);frame->derived.push_back(std::move(detail));}
 frame->fogColor=OriginalNativeModelContext().fog.color;frame->fogParameters=OriginalNativeModelContext().fog.parameters;
 impl->reserved=frame;return frame;
}
std::shared_ptr<const NativeDynamicWaterFrame> NativeDynamicWaterCapture::FinalizeSurface(std::shared_ptr<const NativeDynamicWaterFrame> source,const WorldCamera& actual,const std::array<float,9>& reflectionBasis,const std::array<float,9>& refractionBasis,bool wireframe){
 if(!source||source->material!=impl->material||!camera)throw std::logic_error("Dynamic surface lost actual source owner");
 if(impl->finalized){if(source!=impl->reserved)throw std::logic_error("Dynamic finalized draw differs from reservation");return impl->finalized;}
 if(source!=impl->reserved)throw std::logic_error("Dynamic surface missing source reservation");
 auto out=std::make_shared<NativeDynamicWaterFrame>(*source);out->captureCamera=actual;out->viewProjection=std::to_array(camera->GetViewProjectionMatrix().m);out->wireframe=wireframe;
 out->raster=ui::GetRecorder().CurrentWorldState();out->blend=ui::GetRecorder().CurrentState().draw.blend;out->blend.enabled=true;
 out->fogColor=OriginalNativeModelContext().fog.color;out->fogParameters=OriginalNativeModelContext().fog.parameters;
 out->shading=Shading();if(!shadowHandler.HasNativeTextureOwner())throw std::runtime_error("Dynamic source shadow sampler has no native owner");shadowHandler.SetupShadowTexSampler(0x84C7);OriginalNativeTextureContext().History().ActiveTexture(0);out->shadow=OriginalNativeTextureContext().History().Binding(7,NativeCUSTextureTarget::Image2D);
 const float3 reflectRight(reflectionBasis[0],reflectionBasis[1],reflectionBasis[2]),reflectUp(reflectionBasis[3],reflectionBasis[4],reflectionBasis[5]),reflectForward(reflectionBasis[6],reflectionBasis[7],reflectionBasis[8]);
 const float3 refractRight(refractionBasis[0],refractionBasis[1],refractionBasis[2]),refractUp(refractionBasis[3],refractionBasis[4],refractionBasis[5]),refractForward(refractionBasis[6],refractionBasis[7],refractionBasis[8]);
 const auto& camPosBig=impl->camPosBig;float3 camPosBig2;const int camPosX=source->derivedCameraCell[0],camPosZ=source->derivedCameraCell[1];
 const auto inheritedProgram=OriginalNativeLegacyWaterProgramState().Snapshot();
 out->shadowMatrix=inheritedProgram.matrix0;out->profile.shadowProgramStateObserved=inheritedProgram.complete;
 DynamicWaterDraw constants;
 for(unsigned index=16;index<=18;++index)constants.vertexEnv[index]=inheritedProgram.env[index-16];
 const float dx=float(globalRendering->viewSizeX)/globalRendering->viewSizeY*camera->GetTanHalfFov(),dy=camera->GetTanHalfFov();const auto& sky=ISky::GetSky();const float3& L=sky->GetLight()->GetLightDir();
 constants.vertexEnv[10]={1.0f/(W_SIZE*256), 1.0f/(W_SIZE*256), 0, 0};
 constants.vertexEnv[11]={-camPosX/256.0f + 0.5f, -camPosZ/256.0f + 0.5f, 0, 0};
 constants.vertexEnv[12]={1.0f/WF_SIZE, 1.0f/WF_SIZE, 0, 0};
 constants.vertexEnv[13]={-(camPosBig.x - WH_SIZE)/WF_SIZE, -(camPosBig.z - WH_SIZE)/WF_SIZE, 0, 0};
 constants.vertexEnv[14]={1.0f/(mapDims.mapxp1 * SQUARE_SIZE), 1.0f/(mapDims.mapyp1 * SQUARE_SIZE), 0, 0};
 constants.fragmentEnv[1]={camera->GetPos().x, camera->GetPos().y, camera->GetPos().z, 0};
 constants.fragmentEnv[2]={reflectRight.x, reflectRight.y, reflectRight.z, 0};
 constants.fragmentEnv[3]={reflectUp.x, reflectUp.y, reflectUp.z, 0};
 constants.fragmentEnv[4]={0.5f/dx, 0.5f/dy, 1, 1};
 constants.fragmentEnv[5]={reflectForward.x, reflectForward.y, reflectForward.z, 0};
 constants.fragmentEnv[6]={0.05f, 1 - 0.05f, 0, 0};
 constants.fragmentEnv[7]={0.2f, 0, 0, 0};
 constants.fragmentEnv[8]={0.5f, 0.6f, 0.8f, 0};
 constants.fragmentEnv[9]={L.x, L.y, L.z, 0.0f};
 constants.fragmentEnv[10]={sunLighting->groundDiffuseColor.x, sunLighting->groundDiffuseColor.y, sunLighting->groundDiffuseColor.z, 0};
 constants.fragmentEnv[11]={sunLighting->groundAmbientColor.x, sunLighting->groundAmbientColor.y, sunLighting->groundAmbientColor.z, 0};
 constants.fragmentEnv[12]={refractRight.x, refractRight.y, refractRight.z, 0};
 constants.fragmentEnv[13]={refractUp.x,refractUp.y, refractUp.z, 0};
 constants.fragmentEnv[14]={0.5f/dx, 0.5f/dy, 1, 1};
 constants.fragmentEnv[15]={refractForward.x, refractForward.y, refractForward.z, 0};
 Sink surface;surface.draw=constants;surface.draw.pass=DynamicWaterPass::Surface;Sink* va=&surface;
 auto DrawVertexAQ=[&](int x,int y){va->AddVertexQ0(x*WSQUARE_SIZE,0,y*WSQUARE_SIZE);};


	bool inStrip = false;




	CCamera* cam = CCameraHandler::GetActiveCamera();
	cam->CalcFrustumLines(readMap->GetCurrMinHeight() - 100.0f, readMap->GetCurrMaxHeight() + 100.0f, SQUARE_SIZE);

	camPosBig2.x = std::floor(std::max((float)WH_SIZE, std::min((float)mapDims.mapx*SQUARE_SIZE - WH_SIZE, cam->GetPos().x))/(W_SIZE*16))*(W_SIZE*16);
	camPosBig2.z = std::floor(std::max((float)WH_SIZE, std::min((float)mapDims.mapy*SQUARE_SIZE - WH_SIZE, cam->GetPos().z))/(W_SIZE*16))*(W_SIZE*16);

	const CCamera::FrustumLine* negLines = cam->GetNegFrustumLines();
	const CCamera::FrustumLine* posLines = cam->GetPosFrustumLines();

	for (int lod = 1; lod < (2 << 5); lod *= 2) {
		int cx = (int)(cam->GetPos().x / WSQUARE_SIZE);
		int cy = (int)(cam->GetPos().z / WSQUARE_SIZE);

		cx = (cx / lod) * lod;
		cy = (cy / lod) * lod;

		const int hlod = lod >> 1;
		const int ysquaremod = (cy % (2 * lod)) / lod;
		const int xsquaremod = (cx % (2 * lod)) / lod;

		const int minty = int(camPosBig2.z / WSQUARE_SIZE - 512);
		const int maxty = int(camPosBig2.z / WSQUARE_SIZE + 512);
		const int mintx = int(camPosBig2.x / WSQUARE_SIZE - 512);
		const int maxtx = int(camPosBig2.x / WSQUARE_SIZE + 512);

		const int minly = cy + (-LOD_SIZE_FACT + 2 - ysquaremod) * lod;
		const int maxly = cy + ( LOD_SIZE_FACT     - ysquaremod) * lod;
		const int minlx = cx + (-LOD_SIZE_FACT + 2 - xsquaremod) * lod;
		const int maxlx = cx + ( LOD_SIZE_FACT     - xsquaremod) * lod;

		const int xstart = std::max(minlx, mintx);
		const int xend   = std::min(maxlx, maxtx);
		const int ystart = std::max(minly, minty);
		const int yend   = std::min(maxly, maxty);

		const int vrhlod = LOD_SIZE_FACT * hlod;

		// TODO: split drawing from (duplicated) GridVisibility code below
		for (int y = ystart; y < yend; y += lod) {
			int xs = xstart;
			int xe = xend;
			int xtest,xtest2;

			for (int idx = 0, cnt = negLines[4].sign * 0; idx < cnt; idx++) {
				const CCamera::FrustumLine& fl = negLines[idx];
				const float xtf = fl.base / WSQUARE_SIZE + fl.dir * y;

				xtest  = ((int) xtf                ) / lod * lod - lod;
				xtest2 = ((int)(xtf + fl.dir * lod)) / lod * lod - lod;

				xtest = std::max(xtest, xtest2);
				xs = std::max(xs, xtest);
			}
			for (int idx = 0, cnt = posLines[4].sign * 0; idx < cnt; idx++) {
				const CCamera::FrustumLine& fl = posLines[idx];
				const float xtf = fl.base / WSQUARE_SIZE + fl.dir * y;

				xtest  = ((int) xtf                ) / lod * lod - lod;
				xtest2 = ((int)(xtf + fl.dir * lod)) / lod * lod - lod;

				xtest = std::min(xtest, xtest2);
				xe = std::min(xe, xtest);
			}

			const int ylod = y + lod;
			const int yhlod = y + hlod;

			for (int x = xs; x < xe; x += lod) {
				const int xlod = x + lod;
				const int xhlod = x + hlod;

				if ((lod == 1) || (x > (cx + vrhlod)) || (x < (cx - vrhlod)) || (y > (cy + vrhlod)) || (y < (cy - vrhlod))) { // normal terrain
					if (!inStrip) {
						DrawVertexAQ(x, y);
						DrawVertexAQ(x, ylod);
						inStrip = true;
					}
					DrawVertexAQ(xlod, y);
					DrawVertexAQ(xlod, ylod);
				} else { // inre begr?sning mot f?eg?nde lod FIXME
					if (x >= (cx + vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(x,y);
						DrawVertexAQ(x,yhlod);
						DrawVertexAQ(xhlod,y);
						DrawVertexAQ(xhlod,yhlod);
						va->EndStrip();
						DrawVertexAQ(x,yhlod);
						DrawVertexAQ(x,ylod);
						DrawVertexAQ(xhlod,yhlod);
						DrawVertexAQ(xhlod,ylod);
						va->EndStrip();
						DrawVertexAQ(xhlod,ylod);
						DrawVertexAQ(xlod,ylod);
						DrawVertexAQ(xhlod,yhlod);
						DrawVertexAQ(xlod,y);
						DrawVertexAQ(xhlod,y);
						va->EndStrip();
					} else if (x <= (cx - vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xlod,  y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xhlod, y);
						va->EndStrip();
						DrawVertexAQ(xlod,  ylod);
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xhlod, ylod);
						DrawVertexAQ(xhlod, yhlod);
						va->EndStrip();
						DrawVertexAQ(xhlod, y);
						DrawVertexAQ(x,     y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(x,     ylod);
						DrawVertexAQ(xhlod, ylod);
						va->EndStrip();
					} else if (y >= (cy + vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(x,     y);
						DrawVertexAQ(x,     yhlod);
						DrawVertexAQ(xhlod, y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xlod,  y);
						DrawVertexAQ(xlod,  yhlod);
						va->EndStrip();
						DrawVertexAQ(x,     yhlod);
						DrawVertexAQ(x,     ylod);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xlod,  ylod);
						DrawVertexAQ(xlod,  yhlod);
						va->EndStrip();
					} else if (y <= (cy - vrhlod)) {
						if (inStrip) {
							va->EndStrip();
							inStrip = false;
						}
						DrawVertexAQ(x,     yhlod);
						DrawVertexAQ(x,     ylod);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(xhlod, ylod);
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xlod,  ylod);
						va->EndStrip();
						DrawVertexAQ(xlod,  yhlod);
						DrawVertexAQ(xlod,  y);
						DrawVertexAQ(xhlod, yhlod);
						DrawVertexAQ(x,     y);
						DrawVertexAQ(x,     yhlod);
						va->EndStrip();
					}
				}
			}
			if (inStrip) {
				va->EndStrip();
				inStrip = false;
			}
		}
	}

 out->surface.push_back(std::move(surface.draw));Sink outer;outer.draw=constants;outer.draw.pass=DynamicWaterPass::OuterSurface;va=&outer;


	float posx = camPosBig2.x - WH_SIZE - WF_SIZE;
	float posy = camPosBig2.z - WH_SIZE - WF_SIZE;

	float ys = posy;
	for (int y = -1; y <= 1; ++y, ys += WF_SIZE) { //! CAUTION: loop count must match EnlargeArrays above
		float xs = posx;
		for (int x = -1; x <= 1; ++x, xs += WF_SIZE) {
			if ((x == 0) && (y == 0)) {
				continue;
			}
			float pys = ys;
			for (int y2 = 0; y2 < 16; ++y2) { //! CAUTION: loop count must match EnlargeArrays above
				float pxs = xs;
				const float pys1 = pys + WF_SIZE/16;
				for (int x2 = 0; x2 < 16; ++x2) {
					const float pxs1 = pxs + WF_SIZE/16;
					va->AddVertexQ0(pxs,  0, pys);
					va->AddVertexQ0(pxs1, 0, pys);
					va->AddVertexQ0(pxs1, 0, pys1);
					va->AddVertexQ0(pxs,  0, pys1);
					pxs = pxs1;
				}
				pys = pys1;
			}
		}
	}

 outer.Quads(impl->profile.outerQuadDiagonal301);out->surface.push_back(std::move(outer.draw));impl->finalized=out;return out;
}
void NativeDynamicWaterCapture::DidComplete(uint64_t generation,uint64_t through,uint64_t sourceDrawFrame){
 if(!impl->material||!impl->reserved||!impl->finalized||impl->material->generation!=generation||impl->reserved->throughUpdate!=through||impl->finalized->sourceDrawFrame!=sourceDrawFrame||through<impl->retired||through>impl->sequence)throw std::logic_error("Dynamic source completion differs from actual generation/sequence");
 while(!impl->pending.empty()&&impl->pending.front()->sequence<=through)impl->pending.pop_front();impl->retired=through;impl->firstDraw=false;impl->reserved.reset();impl->finalized.reset();
}
}
