/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeUnitStencilData.h"
#include "NativeUnitStencilPrograms.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <stdexcept>
namespace rendering {
namespace { void Check(bool v,const char* text){if(!v)throw std::runtime_error(text);} }
void NativeUnitStencilVersion::Fulfill(std::shared_ptr<const NativeUnitStencilResource> next)const
{
	Check(next&&(!resource||resource==next),"Unit Stencil immutable R8 resource replaced or missing");resource=std::move(next);
}
NativeUnitStencilFrame::~NativeUnitStencilFrame()
{
	if(!completed)for(auto i=rollback.rbegin();i!=rollback.rend();++i)try{(*i)();}catch(...){std::terminate();}
}
void ValidateNativeUnitStencilProgram(const NativeUnitStencilProgramSource& source)
{
	Check(source.vertex==unitStencilProgram::Vertex&&source.geometry==unitStencilProgram::Geometry&&source.fragment==unitStencilProgram::Fragment,
		"Original Unit Stencil complete VS/GS/FS or engine UBO definitions differ");
	for(const auto& [name,value]:source.initialFloat)Check((name=="addRadius"||name=="stencilColor")&&value.size()==1&&std::isfinite(value[0]),
		"Unknown original Unit Stencil uniform or scalar");
}
void ValidateNativeUnitStencilDraw(const NativeUnitStencilDraw& draw)
{
	Check(draw.sequence&&draw.program&&draw.program->pipelineLifetime&&draw.records&&draw.count,"Unit Stencil draw lacks actual original owners");
	const auto& records=*draw.records;const auto& layout=records.layout;
	Check(records.object&&records.revision&&layout.kind==NativeBufferKind::Vertex&&layout.stride==48&&layout.attributes.size()==3&&
		records.bytes.size()==CheckedNativeBufferByteSize(layout),"Unit Stencil original point record must be actual48-byte min4/max4/uint4");
	for(unsigned id=0;id<3;++id){
		const auto a=std::find_if(layout.attributes.begin(),layout.attributes.end(),[id](const auto& a){return a.id==id;});
		Check(a!=layout.attributes.end()&&a->declaredCount==4&&a->byteSize==16&&a->offset==16*id&&!a->normalized&&
			a->scalarType==(id==2?NativeScalarType::UInt32:NativeScalarType::Float32),"Unit Stencil original attribute ABI differs");
	}
	Check(draw.first<=layout.elements&&draw.count<=layout.elements-draw.first&&std::isfinite(draw.addRadius)&&std::isfinite(draw.stencilColor),
		"Unit Stencil original range or uniforms differ");
	Check(draw.engineUniforms&&draw.modelUniforms&&draw.modelUniforms->kind==NativeCUSPublishedKind::ModelUniforms&&draw.modelUniforms->stride==128,
		"Unit Stencil lacks original engine and object-ID model uniform publication");
	ValidateNativeCUSUniformPublication(*draw.engineUniforms);
	for(size_t row=draw.first;row<size_t(draw.first)+draw.count;++row){
		uint32_t index;std::memcpy(&index,records.bytes.data()+row*48+36,4);
		Check(NativeCUSPublishedElementObserved(*draw.modelUniforms,index),"Unit Stencil engine-generated instData.y references an unpublished object uniform");
	}
}
} // namespace rendering
