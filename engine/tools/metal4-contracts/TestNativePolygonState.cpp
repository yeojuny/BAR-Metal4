/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeUIRecorder.h"
#include "Rendering/Backend/NativeWorldDrawGeometry.h"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace rendering;using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value){++checks;if(!value)throw std::runtime_error("polygon check "+std::to_string(checks));}
template<class F>void Reject(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}Check(rejected);}
std::vector<NativeWorldDrawVertex> Quad(bool front=true){std::vector<NativeWorldDrawVertex> v;for(auto p:std::array<ScreenTexCoord,4>{{{4.5f,4.5f},{27.5f,4.5f},{27.5f,27.5f},{4.5f,27.5f}}})v.push_back({{p[0]/16-1,p[1]/16-1,0,1},{1,1,1,1},{0,0}});if(!front)std::reverse(v.begin(),v.end());return v;}
std::set<std::pair<int,int>> Pixels(const NativeWorldRasterMesh& mesh){std::set<std::pair<int,int>> out;for(size_t i=0;i<mesh.vertices.size();i+=4){const auto& v=mesh.vertices[i].position;out.emplace(int(std::lround((v[0]+1)*16)),int(std::lround((v[1]+1)*16)));}return out;}
}
int main(){try{
 NativeWorldDrawState state;state.frontPolygonMode=state.backPolygonMode=NativePolygonMode::Line;
 auto command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(),{},state);
 Check(command.indices==std::vector<uint32_t>({0,1,2,0,2,3}));
 auto mesh=BuildNativeWorldDrawGeometry(command,{0,0,32,32},32,32);auto pixels=Pixels(mesh);
 std::set<std::pair<int,int>> expected;for(int i=4;i<=27;++i){expected.emplace(i,4);expected.emplace(i,27);expected.emplace(4,i);expected.emplace(27,i);}
 Check(pixels==expected);Check(pixels.size()==92);Check(!pixels.contains({16,16}));
 state.backPolygonMode=NativePolygonMode::Point;state.pointSize=1;
 command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(false),{},state);
 mesh=BuildNativeWorldDrawGeometry(command,{0,0,32,32},32,32);
 Check(Pixels(mesh)==std::set<std::pair<int,int>>({{4,4},{27,4},{27,27},{4,27}}));
 // Back-facing generated point quads preserve their original face for stencil.
 const auto a=mesh.vertices[mesh.indices[0]].position,b=mesh.vertices[mesh.indices[1]].position,c=mesh.vertices[mesh.indices[2]].position;
 Check((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0])<0);
 state.cullEnabled=true;state.cull=NativeWorldCull::Back;
 command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(false),{},state);Check(BuildNativeWorldDrawGeometry(command,{0,0,32,32},32,32).indices.empty());
 state.cullEnabled=false;state.backPolygonMode=NativePolygonMode::Fill;
 command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(false),{},state);Check(BuildNativeWorldDrawGeometry(command,{0,0,32,32},32,32).indices.size()==6);
 // Separate quads restart stippling, whereas one line strip keeps its counter.
 state.frontPolygonMode=state.backPolygonMode=NativePolygonMode::Line;state.lineStipple=true;state.stipplePattern=0x0001;state.stippleFactor=3;
 auto two=Quad();auto next=Quad();two.insert(two.end(),next.begin(),next.end());
 auto one=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(),{},state);
 command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,two,{},state);
 Check(BuildNativeWorldDrawGeometry(command,{0,0,32,32},32,32).indices.size()==2*BuildNativeWorldDrawGeometry(one,{0,0,32,32},32,32).indices.size());
 NativeUIRecorder r;r.BeginList();r.UpdateWorldState([](auto& s){s.frontPolygonMode=NativePolygonMode::Line;s.lineStipple=true;s.stipplePattern=0x5555;s.stippleFactor=2;},false);auto list=r.EndList();
 r.BeginFrame(32,32);r.BeginScreenStage();r.UpdateWorldState([](auto& s){s.backPolygonMode=NativePolygonMode::Point;},false);r.CallList(list);
 Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Line);Check(r.CurrentWorldState().backPolygonMode==NativePolygonMode::Point);Check(r.CurrentWorldState().stipplePattern==0x5555);
 r.PushAttrib(Polygon|Line);r.UpdateWorldState([](auto& s){s.frontPolygonMode=NativePolygonMode::Fill;s.backPolygonMode=NativePolygonMode::Fill;s.lineStipple=false;s.stippleFactor=9;},false);r.PopAttrib();
 Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Line);Check(r.CurrentWorldState().backPolygonMode==NativePolygonMode::Point);Check(r.CurrentWorldState().lineStipple&&r.CurrentWorldState().stippleFactor==2);
 r.UpdateWorldState([](auto& s){s.lineStipple=false;s.frontPolygonMode=s.backPolygonMode=NativePolygonMode::Line;},false);
 r.BeginLegacyPrimitive(NativeDrawPrimitive::Quads);for(auto p:std::array<ScreenTexCoord,4>{{{4.5f,4.5f},{27.5f,4.5f},{27.5f,27.5f},{4.5f,27.5f}}})r.LegacyVertex(p[0],p[1]);r.EndLegacyPrimitive();r.EndScreenStage();auto frame=r.EndFrame();
 Check(frame.drawList->Vertices().size()==92*4);Check(frame.drawList->Commands().size()==1);
 r.BeginFrame(32,32);r.BeginScreenStage();r.UpdateWorldState([](auto& s){s.lineStipple=true;},false);r.ResetDrawingState();Check(!r.CurrentWorldState().lineStipple);Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Fill);r.EndScreenStage();r.EndFrame();
 r.BeginFrame(32,32);r.BeginScreenStage();r.UpdateWorldState([](auto& s){s.frontPolygonMode=NativePolygonMode::Point;s.lineWidth=3;s.pointSize=4;s.stippleFactor=7;s.stipplePattern=0x1357;},false);
 auto target=MakeRenderTargetTexture(32,32,"polygon-stage-fixture");target->generation=1;r.BeginTarget(target,{});r.BeginMiniMapStage(true);Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Fill);Check(r.CurrentWorldState().lineWidth==1);r.UpdateWorldState([](auto& s){s.frontPolygonMode=NativePolygonMode::Line;s.stipplePattern=1;s.stippleFactor=2;},false);r.EndMiniMapStage(true,true);Check(r.CurrentWorldState().frontPolygonMode==NativePolygonMode::Point);Check(r.CurrentWorldState().stipplePattern==0x1357&&r.CurrentWorldState().stippleFactor==7);Check(r.CurrentWorldState().lineWidth==3&&r.CurrentWorldState().pointSize==4);r.EndTarget();r.EndScreenStage();r.EndFrame();r.RetireCompletedFrame();
 // Independent dyadic near-plane clipping golden. GL clips the planar
 // source polygon before POINT/LINE mode; both generated intersections and
 // their boundary edge must survive (GL2.1 2.12), without fill diagonals.
 std::vector<NativeWorldDrawVertex> clipVertices;
 for(auto p:std::array<std::array<float,3>,4>{{{{-.515625f,-.515625f,-1.5f}},{{.484375f,-.515625f,-.5f}},{{.484375f,.484375f,.5f}},{{-.515625f,.484375f,-.5f}}}})
  clipVertices.push_back({{p[0],p[1],p[2],1},{1,1,1,1},{0,0}});
 NativeWorldDrawState clipState;clipState.frontPolygonMode=clipState.backPolygonMode=NativePolygonMode::Point;
 auto clipCommand=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,clipVertices,{},clipState);
 auto clipMesh=BuildNativeWorldDrawGeometry(clipCommand,{0,0,64,64},64,64);
 const auto clipPixels=[](const auto& value){std::set<std::pair<int,int>> out;for(size_t i=0;i<value.vertices.size();i+=4){const auto& p=value.vertices[i].position;out.emplace(int(std::lround((p[0]+1)*32)),int(std::lround((p[1]+1)*32)));}return out;};
 Check(clipPixels(clipMesh)==std::set<std::pair<int,int>>({{15,31},{31,15},{47,15},{47,47},{15,47}}));
 Check(clipMesh.vertices.size()==5*4);
 clipState.frontPolygonMode=clipState.backPolygonMode=NativePolygonMode::Line;
 clipCommand=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,clipVertices,{},clipState);
 clipMesh=BuildNativeWorldDrawGeometry(clipCommand,{0,0,64,64},64,64);
 std::set<std::pair<int,int>> clipExpected;
 for(int y=32;y<=47;++y)clipExpected.emplace(15,y);
 for(int x=15;x<=30;++x)clipExpected.emplace(x,46-x);
 for(int x=31;x<=46;++x)clipExpected.emplace(x,15);
 for(int y=15;y<=46;++y)clipExpected.emplace(47,y);
 for(int x=16;x<=47;++x)clipExpected.emplace(x,47);
 Check(clipPixels(clipMesh)==clipExpected);Check(clipExpected.size()==112);
 Check(clipPixels(clipMesh).contains({23,23}));Check(!clipPixels(clipMesh).contains({32,32}));
 // Unsupported original depth-plane offset remains a visible failure.
 state.polygonOffset=true;command=MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(),{},state);Reject([&]{BuildNativeWorldDrawGeometry(command,{0,0,32,32},32,32);});
 state.polygonOffset=false;state.frontPolygonMode=NativePolygonMode(99);Reject([&]{MakeNativeWorldDrawCommand(NativeWorldDrawStage::World,NativeDrawPrimitive::Quads,Quad(),{},state);});
 std::cout<<checks<<" polygon/state CPU checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
