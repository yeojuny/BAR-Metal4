/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativeReclaimFieldData.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <limits>
namespace rendering {
namespace {
std::atomic<bool> available{false};
void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
std::array<float,4> Row(const std::vector<uint8_t>& bytes,size_t offset){
	Check(offset<=bytes.size()&&16<=bytes.size()-offset,"Reclaim field source vec4 exceeds retained bytes");
	std::array<float,4> v;std::memcpy(v.data(),bytes.data()+offset,16);for(float x:v)Check(std::isfinite(x),"Reclaim field source vec4 is nonfinite");return v;
}
}
uint64_t NativeReclaimFieldSourceHash(std::string_view source){uint64_t h=14695981039346656037ULL;for(unsigned char c:source)h=(h^c)*1099511628211ULL;return h;}
void SetNativeReclaimFieldConsumerAvailable(bool value){available=value;}
bool IsNativeReclaimFieldConsumerAvailable(){return available;}
void ValidateNativeReclaimFieldInput(const NativeReclaimFieldInput& d){
	Check(d.vertexHash==reclaimfield::VertexHash&&d.fragmentHash==reclaimfield::FragmentHash,"Reclaim field original31657 shader identity differs");
	Check(d.vertices&&d.vertices->object&&d.vertices->revision&&d.vertices->bytes.size()==CheckedNativeBufferByteSize(d.vertices->layout),"Reclaim field has no actual immutable vertex buffer");
	const auto& v=d.vertices->layout;
	Check(v.kind==NativeBufferKind::Vertex&&v.stride==16&&v.attributes.size()==1,"Reclaim field posKind buffer layout differs");
	const auto& a=v.attributes.front();Check(a.id==0&&a.name=="posKind"&&a.offset==0&&a.byteSize==16&&a.declaredCount==4&&a.scalarType==NativeScalarType::Float32&&!a.normalized,"Reclaim field actual location0 posKind differs");
	const auto& p=d.params;Check(p.snapshot&&p.snapshot->object&&p.snapshot->revision&&p.snapshot->bytes.size()==CheckedNativeBufferByteSize(p.snapshot->layout),"Reclaim field has no retained actual SSBO");
	const auto& pl=p.snapshot->layout;
	Check(pl.kind==NativeBufferKind::Storage&&pl.stride==16&&pl.attributes.size()==1&&p.binding==9&&p.offset%16==0&&p.offset<=p.snapshot->bytes.size()&&p.size<=p.snapshot->bytes.size()-p.offset,"Reclaim field SSBO9 range/layout differs");
	const auto& pa=pl.attributes.front();Check(pa.id==0&&pa.name=="params"&&pa.offset==0&&pa.byteSize==16&&pa.scalarType==NativeScalarType::Float32&&pa.valueShape==NativeValueShape::Vector4&&pa.declaredCount==1&&!pa.normalized,"Reclaim field original two-vec4 parameter layout differs");
	Check(d.first==0&&d.count&&d.first<=v.elements&&d.count<=v.elements-d.first&&d.uniforms.vertsPerChunk==(d.edges?64u:288u)&&d.count%d.uniforms.vertsPerChunk==0,"Reclaim field actual chunk draw interval differs");
	Check(size_t(d.count/d.uniforms.vertsPerChunk)<=p.size/32,"Reclaim field draw exceeds actual SSBO9 chunks");
	Check(d.original!=nullptr,"Reclaim field lost its original engine uniforms");ValidateNativeCUSUniformPublication(*d.original);
	Check(d.original->matrices->sourceDrawFrame<=d.sourceDrawFrame&&d.original->params->sourceDrawFrame<=d.sourceDrawFrame,"Reclaim field uniforms are from a future draw");
	const auto& u=d.uniforms;for(const auto& c:{u.fillMetal,u.fillEnergy,u.edgeMetal,u.edgeEnergy})for(float x:c)Check(std::isfinite(x),"Reclaim field source color is nonfinite");
	for(float x:{u.toggles[0],u.toggles[1],u.fadeDist[0],u.fadeDist[1],u.yOffset,u.gradientAlpha,u.energyOpacity})Check(std::isfinite(x),"Reclaim field uniform is nonfinite");
	Check(u.fadeDist[1]>u.fadeDist[0]&&u.yOffset==(d.edges?0.f:-1.f),"Reclaim field fade interval or actual draw offset differs");
}
std::vector<NativeWorldDrawVertex> EvaluateNativeReclaimFieldOutline(const NativeReclaimFieldInput& d){
	ValidateNativeReclaimFieldInput(d);Check(d.edges&&d.count<=4u*1024u*1024u,"Reclaim outline exceeds the existing common geometry capacity");
	std::array<float,16> vp,inv;std::memcpy(vp.data(),d.original->matrices->bytes.data()+reclaimfield::ViewProjectionByte,64);std::memcpy(inv.data(),d.original->matrices->bytes.data()+reclaimfield::ViewInverseByte,64);
	uint32_t caps;std::memcpy(&caps,d.original->params->bytes.data()+12,4);
	std::vector<NativeWorldDrawVertex> out(d.count);const auto& u=d.uniforms;
	for(uint32_t i=0;i<d.count;++i){
		auto pos=Row(d.vertices->bytes,size_t(i+d.first)*16);const size_t chunk=(i+d.first)/u.vertsPerChunk;
		const auto p0=Row(d.params.snapshot->bytes,d.params.offset+chunk*32),p1=Row(d.params.snapshot->bytes,d.params.offset+chunk*32+16);
		const bool energy=p1[1]>.5f;float alpha=p1[0]*(energy?u.toggles[1]:u.toggles[0]);
		pos[0]=p0[0]+(pos[0]-p0[0])*p0[3];pos[2]=p0[2]+(pos[2]-p0[2])*p0[3];pos[1]+=u.yOffset;
		const float dx=inv[12]-p0[0],dy=inv[13]-p0[1],dz=inv[14]-p0[2];
		const float fade=std::max(std::clamp(1.f-(std::sqrt(dx*dx+dy*dy+dz*dz)-u.fadeDist[0])/(u.fadeDist[1]-u.fadeDist[0]),0.f,1.f),p1[2]);
		const auto& fill=energy?u.fillEnergy:u.fillMetal;const auto& edge=energy?u.edgeEnergy:u.edgeMetal;
		const float kind=pos[3];const auto& rgb=kind<1.5f?fill:edge;
		alpha*= (kind<.5f?fill[3]:kind<1.5f?u.gradientAlpha:edge[3])*fade*(energy?u.energyOpacity:1.f);
		auto& v=out[i];v.color={rgb[0],rgb[1],rgb[2],alpha};v.uv={0,0,0,0};
		if(alpha<=.0005f)v.position={-2,-2,-2,1};
		else for(unsigned row=0;row<4;++row){float s=vp[12+row];for(unsigned col=0;col<3;++col)s+=vp[col*4+row]*pos[col];v.position[row]=s;}
		// Common world lines use canonical GL [-w,w] homogeneous depth. Preserve
		// the actual UBO clip-control convention before its shared raster expansion.
		if(caps&1u)v.position[2]=2*v.position[2]-v.position[3];
	}
	return out;
}
}
