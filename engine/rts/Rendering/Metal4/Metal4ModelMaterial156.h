/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#import <Metal/Metal.h>
#include <array>
#include <cstdint>
#include <map>
#include <regex>
#include <string>
#include <stdexcept>

namespace rendering::metal4 {
// A single bounded policy for the original model color/material pair.
// The library contains both paths; only the existing PSO is specialized.
enum class ModelSamplerPolicy219 : uint8_t { Generic, CanonicalPair };
inline constexpr NSUInteger ModelSamplerConstant219 = 219;
inline constexpr bool ModelSamplerPair219(uint32_t mask) { return (mask & 3u) == 3u; }

inline constexpr std::array<const char*,12> ModelMaterialNames156={"texture1","texture2","normalTex","texture1w","texture2w","normalTexw","shadowTex","reflectTex","losMapTex","brdfLUT","noisetex3dcube","envLUT"};
inline const char* ModelMaterialType156(size_t slot){return slot==7?"texturecube<float>":slot==10?"texture3d<float>":"texture2d<float>";}

// Preserve the original function body, uniforms, texture types, sample calls
// and stage identity. The two canonical samplers are selected only by a PSO
// function constant after the actual retained sampler state is proven equal.
inline std::string ModelMaterialFragment156(const char* source,uint32_t mask) {
 std::string original(source);
 if(!mask)return original;
 const auto entry=original.find("\nfragment ");
 const auto body=original.find("\n{",entry);
 if(entry==std::string::npos||body==std::string::npos||original.find("NativeCUSFragment(",entry)>body)
  throw std::runtime_error("CUS156 material entry signature differs");
 const std::string signature=original.substr(entry,body-entry);
 const std::regex resource(R"(, (texture2d<float>|texturecube<float>|texture3d<float>|sampler) (\w+) \[\[(texture|sampler)\((\d+)\)\]\])");
 uint32_t textures=0,samplers=0;std::string locals;
 if(ModelSamplerPair219(mask))locals+=R"SAMPLER219(
    constexpr sampler NativeCUSLinearRepeat219(coord::normalized, min_filter::linear, mag_filter::linear,
        mip_filter::linear, s_address::repeat, t_address::repeat, r_address::clamp_to_edge, max_anisotropy(1));
)SAMPLER219";
 for(auto it=std::sregex_iterator(signature.begin(),signature.end(),resource);it!=std::sregex_iterator();++it){
  const auto& m=*it;const auto slot=std::stoul(m[4]);const bool sampler=m[3]=="sampler";
  if(slot>=12||!(mask&(1u<<slot))||m[1]!=(sampler?"sampler":ModelMaterialType156(slot))||
     m[2]!=std::string(ModelMaterialNames156[slot])+(sampler?"Smplr":""))
   throw std::runtime_error("CUS156 material resource identity differs");
  auto& seen=sampler?samplers:textures;if(seen&(1u<<slot))throw std::runtime_error("Repeated CUS156 material slot");seen|=1u<<slot;
  const auto resourceName=std::string("resources156.")+(sampler?"s":"t")+std::to_string(slot);
  const auto value=sampler&&slot<2&&ModelSamplerPair219(mask)
   ?"(NativeCUSCanonicalSampler219 ? NativeCUSLinearRepeat219 : "+resourceName+")":resourceName;
  locals+="\n    auto "+m[2].str()+" = "+value+";";
 }
 if(textures!=mask||samplers!=mask)throw std::runtime_error("CUS156 material mask differs from entry point");
 auto converted=std::regex_replace(signature,resource,"");
 const auto arguments=converted.find(" RECOIL_SAMPLE_ID_ARGUMENT");
 if(arguments==std::string::npos)throw std::runtime_error("CUS156 sample arguments changed");
 converted.insert(arguments,", constant NativeCUSMaterial156& resources156 [[buffer(8)]]");
 std::string declaration;
 if(ModelSamplerPair219(mask))declaration+="\nconstant bool NativeCUSCanonicalSampler219 [[function_constant("+std::to_string(ModelSamplerConstant219)+")]];\n";
 declaration+="\nstruct NativeCUSMaterial156 {\n";
 for(size_t i=0;i<12;++i)declaration+="    "+std::string(ModelMaterialType156(i))+" t"+std::to_string(i)+" [[id("+std::to_string(i)+")]];\n";
 for(size_t i=0;i<12;++i)declaration+="    sampler s"+std::to_string(i)+" [[id("+std::to_string(i+12)+")]];\n";
 declaration+="};\n";
 return original.substr(0,entry)+declaration+converted+"\n{"+locals+original.substr(body+2);
}

struct ModelMaterialStats156 {size_t hits=0,created=0,encodedBytes=0;};
class ModelMaterials156 {
 using Key=std::array<uint64_t,24>;
 struct Entry {__strong id<MTLBuffer> buffer=nil;uint64_t lastSeen=0;};
 std::map<Key,Entry> entries;
 __strong id<MTLDevice> device=nil;
 __strong id<MTLArgumentEncoder> encoder=nil;
 const void* context=nullptr;uint64_t generation=0;
 ModelMaterialStats156 stats;
public:
 void Clear(){entries.clear();encoder=nil;device=nil;context=nullptr;}
 template<class Lease>void BeginBatch(Lease& lease){
  if(device!=lease.Device()||context!=lease.ContextIdentity()){Clear();device=lease.Device();context=lease.ContextIdentity();}
  if(generation==UINT64_MAX)throw std::runtime_error("Material cache generation exhausted");
  ++generation;stats={};
 }
 template<class Lease>id<MTLBuffer> Get(Lease& lease,const std::array<id<MTLTexture>,12>& textures,const std::array<id<MTLSamplerState>,12>& samplers){
  if(device!=lease.Device()||context!=lease.ContextIdentity()||!generation)throw std::runtime_error("Material cache context differs");
  Key key{};for(size_t i=0;i<12;++i){key[i]=textures[i].gpuResourceID._impl;key[i+12]=samplers[i].gpuResourceID._impl;}
  if(auto it=entries.find(key);it!=entries.end()){it->second.lastSeen=generation;++stats.hits;lease.AddAllocation(it->second.buffer);return it->second.buffer;}
  if(!encoder){
   auto descriptors=[NSMutableArray new];
   for(NSUInteger i=0;i<24;++i){auto d=[MTLArgumentDescriptor argumentDescriptor];d.index=i;d.arrayLength=1;d.access=MTLBindingAccessReadOnly;d.dataType=i<12?MTLDataTypeTexture:MTLDataTypeSampler;if(i<12)d.textureType=i==7?MTLTextureTypeCube:i==10?MTLTextureType3D:MTLTextureType2D;[descriptors addObject:d];}
   encoder=[device newArgumentEncoderWithArguments:descriptors];
   if(!encoder||!encoder.encodedLength||encoder.encodedLength>4096)throw std::runtime_error("CUS156 material argument layout unavailable");
  }
  auto buffer=[device newBufferWithLength:encoder.encodedLength options:MTLResourceStorageModeShared];
  if(!buffer)throw std::runtime_error("Cannot allocate CUS156 material bindings");
  [encoder setArgumentBuffer:buffer offset:0];
  for(NSUInteger i=0;i<12;++i){[encoder setTexture:textures[i] atIndex:i];[encoder setSamplerState:samplers[i] atIndex:i+12];}
  [encoder setArgumentBuffer:nil offset:0];
  if(entries.size()==128){auto oldest=entries.begin();for(auto it=entries.begin();it!=entries.end();++it)if(it->second.lastSeen<oldest->second.lastSeen)oldest=it;entries.erase(oldest);}
  // Prepared Draw and its lease retain the buffer independently of this cache.
  // Textures/samplers retain their existing current-draw source ownership.
  entries.emplace(key,Entry{buffer,generation});++stats.created;stats.encodedBytes+=encoder.encodedLength;
  lease.AddAllocation(buffer);return buffer;
 }
 const ModelMaterialStats156& Stats()const{return stats;}
 size_t Entries()const{return entries.size();}
 size_t Bytes()const{size_t n=0;for(const auto& [key,e]:entries)n+=e.buffer.length;return n;}
 size_t AllocatedBytes()const{size_t n=0;for(const auto& [key,e]:entries)n+=e.buffer.allocatedSize;return n;}
};
}
