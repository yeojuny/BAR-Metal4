/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeLightningEffectRecorder.h"
#include "Rendering/Backend/NativeLightningEffectPrograms.h"
#include "Rendering/Backend/NativeWorldDrawSequence.h"
#include <cmath>
#include <iostream>
#include <limits>
using namespace rendering;
namespace {
unsigned checks = 0;
void Check(bool value, const char* message) { ++checks; if (!value) throw std::runtime_error(message); }
template<class F> void Reject(F&& operation, const char* message) { bool rejected = false; try { operation(); } catch (const std::exception&) { rejected = true; } Check(rejected,message); }
NativeBufferSnapshotRef Capture(NativeBufferLayout layout, const void* bytes, std::size_t size)
{
	NativeBufferSnapshotSource source; source.Define(layout); return source.Capture({static_cast<const std::uint8_t*>(bytes),size});
}
NativeLightningEffectDraw Seed(NativeLightningProgram program)
{
	NativeLightningEffectDraw draw; draw.program = program;draw.world.depthTest=true;draw.surface.blend.sourceColor=draw.surface.blend.sourceAlpha=draw.surface.blend.destinationColor=draw.surface.blend.destinationAlpha=ScreenBlendFactor::One;
	const std::array<float,24> mesh{-1,-1,0,0, -1,1,0,1, 1,1,1,1, 1,1,1,1, 1,-1,1,0, -1,-1,0,0};
	draw.vertices = Capture({NativeBufferKind::Vertex,6,16,{{0,"position_xy_uv",NativeScalarType::Float32,NativeValueShape::Scalar,4,0,16,false}}},mesh.data(),sizeof(mesh));
	const std::array<std::uint16_t,6> indices{0,1,2,3,4,5};
	draw.indices = Capture({NativeBufferKind::Index,6,2,{{0,"",NativeScalarType::UInt16,NativeValueShape::Scalar,1,0,2,false}}},indices.data(),sizeof(indices));
	std::array<float,48> stream{}; for (unsigned i=0;i<stream.size();++i) stream[i]=float(i)+.25f;
	NativeBufferLayout layout{NativeBufferKind::Vertex,2,96,{}}; for (unsigned i=0;i<6;++i) layout.attributes.push_back({i+1,"",NativeScalarType::Float32,NativeValueShape::Scalar,4,i*16,16,false});
	draw.instances = Capture(layout,stream.data(),sizeof(stream)); draw.instanceCount=1;
	const auto kind=static_cast<unsigned>(program); for(unsigned i=0;i<NativeLightningParameterNames.size();++i) if(NativeLightningRequiredParameters[kind][i]) draw.parameters[std::string(NativeLightningParameterNames[i])]=1.f;
	if(!NativeLightningPrograms[kind].texture.empty()) { auto texture=std::make_shared<NativeProjectileTexture>();texture->sourceName=NativeLightningPrograms[kind].texture;texture->width=texture->height=512;texture->rgba.resize(512*512*4);draw.texture=texture; }
	return draw;
}
}
int main()
{
	try {
		for (unsigned kind=0;kind<5;++kind) {
			auto draw=Seed(static_cast<NativeLightningProgram>(kind));ValidateNativeLightningEffectDraw(draw);Check(draw.instances->layout.stride==96,"Original six-vec4 stride");
			for(unsigned attr=0;attr<6;++attr) for(unsigned scalar=0;scalar<4;++scalar) { float value;const std::size_t address=96+attr*16+scalar*4;std::memcpy(&value,draw.instances->bytes.data()+address,4);Check(value==float(24+attr*4+scalar)+.25f,"Independent seeded byte address in second original record"); }
			auto second=draw;second.firstInstance=1;ValidateNativeLightningEffectDraw(second);Check(second.instances->bytes==draw.instances->bytes,"Nonzero selected row preserves full original capacity");
			for(unsigned variant=0;variant<8;++variant){auto bad=draw;if(variant==0)bad.surface.blend.enabled=false;if(variant==1)bad.world.depthWrite=true;if(variant==2)bad.world.depthTest=false;if(variant==3)bad.world.cullEnabled=true;if(variant==4)bad.world.frontPolygonMode=NativePolygonMode::Line;if(variant==5)bad.world.depthCompare=ScreenAlphaCompare(255);if(variant==6)bad.surface.alphaTest.reference=std::numeric_limits<float>::infinity();if(variant==7)bad.world.frontStencil.pass=NativeWorldStencilOp(255);Reject([&]{ValidateNativeLightningEffectDraw(bad);},"Original state or explicit unsupported inherited state rejected");}
			auto indexed=draw;indexed.indexed=true;Reject([&]{ValidateNativeLightningEffectDraw(indexed);},"Later attached indices never change original DrawArrays");
			auto bounds=draw;bounds.firstInstance=2;Reject([&]{ValidateNativeLightningEffectDraw(bounds);},"Selected interval exceeds capacity");
			auto parameters=draw;parameters.parameters["NOT_AN_ORIGINAL_DEFINE"]=1;Reject([&]{ValidateNativeLightningEffectDraw(parameters);},"Unexpected program define rejected");
			auto state=draw;NativeLightningEffectRecorder recorder;recorder.Begin();recorder.Record(draw);const auto between=NextNativeWorldDrawSequence();recorder.Record(draw);
			auto data=std::make_shared<NativeBufferSnapshot>(*draw.instances);float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(data->bytes.data()+96,&nan,4);state.instances=data;ValidateNativeLightningEffectDraw(state);Check(true,"Unselected capacity tail may be nonfinite");state.firstInstance=1;Reject([&]{recorder.Record(state);},"Selected nonfinite row is rejected atomically");Check(recorder.Active(),"Rejected draw leaves original frame active");auto frame=recorder.End();Check(frame->draws.size()==2&&frame->draws[0].sequence<between&&frame->draws[1].sequence>between,"Actual shared call order and earlier draws survive rejection");
			std::memset(data->bytes.data(),0,data->bytes.size());Check(frame->draws[0].instances->bytes==draw.instances->bytes,"Published immutable snapshot remains owned");
			for(unsigned variant=0;variant<4;++variant) {auto bad=draw;auto snapshot=std::make_shared<NativeBufferSnapshot>(*draw.indices);auto& a=snapshot->layout.attributes.front();if(variant==0)a.id=1;if(variant==1)a.normalized=true;if(variant==2)a.declaredCount=2;if(variant==3)a.valueShape=NativeValueShape::Vector4;bad.indices=snapshot;Reject([&]{ValidateNativeLightningEffectDraw(bad);},"Wrong original scalar index metadata rejected");}
		}
		NativeLightningEffectRecorder aborted;aborted.Begin();aborted.Abort();Reject([&]{aborted.End();},"Explicit whole-frame abort remains distinct");
		std::cout<<"PASS "<<checks<<" original lightning typed-buffer/range/lifetime/order checks\n";return 0;
	} catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
