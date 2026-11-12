#include <cstdlib>
#include <cstring>
/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4TerrainSourceGPU.h"
#include "Metal4InterventionMath.h"
#include "Metal4RetiredArguments.h"
#include "Metal4SharedUploadPool.h"
#include "Metal4InterventionPools.h"
#include "Metal4TerrainSampling211.h"
#include "Metal4TerrainGeometryCache.inc"
#include "Rendering/Backend/NativePreparationTrace.h"
#include "Metal4TerrainUniformBatch.inc"
#include "NativeSMFCompositeOriginalSource.h"
#include "Metal4NativeEffectState.h"
#include "NativeSMFTerrainOriginalSource.h"
#include "NativeSMFTerrainShaders.h"
#include <simd/simd.h>
#include <algorithm>
#include <array>
#include <optional>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>

namespace rendering::metal4 {
namespace {
struct UnsupportedSource : std::runtime_error { using std::runtime_error::runtime_error; };
uint64_t SourceHash(std::string_view bytes)
{
	uint64_t hash=14695981039346656037ULL;
	for(unsigned char byte:bytes)hash=(hash^byte)*1099511628211ULL;
	return hash;
}
using Definitions=std::map<std::string,std::string>;
Definitions ParseDefinitions(const NativeTerrainProgramRequest& request)
{
	Definitions result;
	std::istringstream input(request.rawDefinitions+"\n"+request.flagDefinitions);
	std::string line;
	while(std::getline(input,line)) {
		if(line.empty())continue;
		Require(line.starts_with("#define "),"Original terrain definition syntax is invalid");
		const auto split=line.find_first_of(" \t",8);
		const auto name=line.substr(8,split==std::string::npos?split:split-8);
		std::string value;
		if(split!=std::string::npos) {
			const auto start=line.find_first_not_of(" \t",split);
			if(start!=std::string::npos)value=line.substr(start);
		}
		Require(result.emplace(name,value).second,"Original terrain definitions contain a duplicate name");
	}
	const std::vector<std::pair<std::string,std::string>> actual(result.begin(),result.end());
	Require(actual==request.identity.defines,"Original terrain definition identity differs from actual source request");
	return result;
}
enum class ImageKind { Color2D, Cube, Shadow };
struct SamplerABI {std::string name;ImageKind kind=ImageKind::Color2D;};
struct Profile {
	bool border=false, deferred=false, combined=false, composite=false;
	bool MRT() const { return deferred||combined; }
	Definitions defines;
	std::vector<SamplerABI> fragment;
	std::map<std::string,NativeTerrainUniformValue> defaults;
	bool Has(const char* name)const{return defines.contains(name);}
	void Float(const char* name,unsigned rows,unsigned columns=1) {
		NativeTerrainUniformValue v;v.rows=rows;v.columns=columns;
		v.values=std::vector<float>(rows*columns,0.0f);defaults.emplace(name,std::move(v));
	}
	void Int(const char* name,unsigned rows=1) {
		NativeTerrainUniformValue v;v.rows=rows;v.values=std::vector<int32_t>(rows,0);defaults.emplace(name,std::move(v));
	}
	void Image(const char* name,ImageKind kind=ImageKind::Color2D) {fragment.push_back({name,kind});Int(name);}
};
Profile OriginalProfile(const NativeTerrainProgramRequest& request)
{
	const auto& identity=request.identity;
	Require(identity.contextGeneration&&identity.originalProgram&&identity.originalProgramGeneration&&identity.sourceRevision,
		"Original terrain compile request lacks its source lifetime");
	Require(identity.vertexSourceHash==SourceHash(request.vertexSource)&&identity.fragmentSourceHash==SourceHash(request.fragmentSource),
		"Original terrain stage hash differs from retained source bytes");
	switch(identity.selection) {
		case NativeTerrainProgramSelection::ForwardStandard:case NativeTerrainProgramSelection::ForwardAdvanced:
		case NativeTerrainProgramSelection::DeferredAdvanced:case NativeTerrainProgramSelection::Border:break;
		case NativeTerrainProgramSelection::CombinedAdvanced:case NativeTerrainProgramSelection::Composite:break;
		case NativeTerrainProgramSelection::LuaForward:case NativeTerrainProgramSelection::LuaDeferred:
			throw UnsupportedSource("Original Lua terrain program requires its own observed source translation and compiled ABI");
		default:throw std::runtime_error("Original terrain program selection is invalid");
	}
	if(identity.selection==NativeTerrainProgramSelection::Composite){
		if(request.vertexSource!=smforiginal::COMPOSITE_VERTEX_SOURCE||request.fragmentSource!=smforiginal::COMPOSITE_FRAGMENT_SOURCE)
			throw UnsupportedSource("Original SMFComposite VFS source differs from the exact color/depth translation");
		if(!request.attributeBindings.empty()||(!request.outputBindings.empty()&&request.outputBindings!=std::vector<std::pair<std::string,uint32_t>>{{"fragColor",0}}))
			throw UnsupportedSource("Original SMFComposite built-in quad/output ABI differs");
		Profile p;p.composite=true;p.defines=ParseDefinitions(request);if(!p.defines.empty())throw UnsupportedSource("Modified original composite definitions require their actual source translation");
		p.Image("colorTex");p.Image("depthTex");return p;
	}
	if(identity.selection==NativeTerrainProgramSelection::Border) {
		if(request.vertexSource!=smforiginal::BORDER_VERTEX_SOURCE||request.fragmentSource!=smforiginal::BORDER_FRAGMENT_SOURCE)
			throw UnsupportedSource("Original SMF border VFS source differs from its exact supported translation");
		if(request.attributeBindings!=std::vector<std::pair<std::string,uint32_t>>{{"vertexCol",1},{"vertexPos",0}})
			throw UnsupportedSource("Original SMF border position/color attribute bindings differ from its actual ABI");
		if(!request.outputBindings.empty()&&request.outputBindings!=std::vector<std::pair<std::string,uint32_t>>{{"fragColor",0}})
			throw UnsupportedSource("Original SMF border output requires another actual target ABI");
		Profile border;border.border=true;border.defines=ParseDefinitions(request);
		if(!border.defines.empty())
			throw UnsupportedSource("Original SMF border has modified compile definitions requiring their own exact translation");
		border.Int("texSquare",2);border.Int("heightMapTex");border.Float("borderMinHeight",1);border.Float("mapSize",4);
		border.Image("diffuseTex");border.Image("detailsTex");
		return border;
	}
	if(request.vertexSource!=smforiginal::VERTEX_SOURCE||request.fragmentSource!=smforiginal::FRAGMENT_SOURCE)
		throw UnsupportedSource("Original SMF VFS source differs from the exact supported source; modified shaders require their own translation");
	if(request.attributeBindings!=std::vector<std::pair<std::string,uint32_t>>{{"vertexPos",0}})
		throw UnsupportedSource("Original SMF vertexPos attribute binding differs from its supported original ABI");
	Profile p;p.deferred=identity.selection==NativeTerrainProgramSelection::DeferredAdvanced;p.combined=identity.selection==NativeTerrainProgramSelection::CombinedAdvanced;p.defines=ParseDefinitions(request);
	const std::vector<std::pair<std::string,uint32_t>> output={{p.MRT()?"fragData":"fragColor",0}};
	if(!request.outputBindings.empty()&&request.outputBindings!=output)
		throw UnsupportedSource("Original SMF fragment output binding requires another actual target ABI");
	// The target GLSL uses defined(SMF_COMBINED_PASS), including value0.
	// Preserve that original selection and require its sixth-color producer.
	if(p.Has("SMF_COMBINED_PASS")!=p.combined)
		throw UnsupportedSource("Original combined SMF program and actual six-color definition disagree");
	if(p.Has("DEFERRED_MODE")!=p.deferred)
		throw UnsupportedSource("Original SMF selected owner and actual deferred output definition disagree");
	// These are the exact target GeometryBuffer attachment indices. Depth is
	// raster depth, not a color; the combined path owns optional color5.
	const Definitions deferredIndices={{"GBUFFER_NORMTEX_IDX","0"},{"GBUFFER_DIFFTEX_IDX","1"},
		{"GBUFFER_SPECTEX_IDX","2"},{"GBUFFER_EMITTEX_IDX","3"},{"GBUFFER_MISCTEX_IDX","4"},
		{"GBUFFER_COLORTEX_IDX","5"},{"GBUFFER_ZVALTEX_IDX","6"}};
	if(p.MRT())for(const auto& [name,value]:deferredIndices) {
		const auto found=p.defines.find(name);
		if(found==p.defines.end()||found->second!=value)
			throw UnsupportedSource("Original SMF deferred attachment definition differs from its actual MRT ABI: "+name);
	}
	const std::set<std::string> flags={"SMF_ADV_SHADING","SMF_VOID_WATER","SMF_VOID_GROUND","SMF_SPECULAR_LIGHTING",
		"SMF_DETAIL_TEXTURE_SPLATTING","SMF_DETAIL_NORMAL_TEXTURE_SPLATTING","SMF_DETAIL_NORMAL_DIFFUSE_ALPHA",
		"SMF_WATER_ABSORPTION","SMF_SKY_REFLECTIONS","SMF_BLEND_NORMALS","SMF_LIGHT_EMISSION",
		"SMF_PARALLAX_MAPPING","HAVE_SHADOWS","HAVE_INFOTEX","DEFERRED_MODE","SMF_COMBINED_PASS"};
	for(const auto& [name,value]:p.defines) {
		if(p.MRT()&&deferredIndices.contains(name))continue;
		if(name=="SMF_TEXSQUARE_SIZE"||name=="SMF_INTENSITY_MULT") {
			size_t used=0;float numeric=0;
			try{numeric=std::stof(value,&used);}catch(const std::exception&){throw UnsupportedSource("Original SMF numeric definition is not a supported literal: "+name);}
			if(used!=value.size()||!std::isfinite(numeric)||numeric<=0||value.find_first_not_of("0123456789.eE+-")!=std::string::npos)
				throw UnsupportedSource("Original SMF numeric definition is not a supported positive literal: "+name);
		} else if(!flags.contains(name)||(value!=""&&value!="0"&&value!="1")) {
			throw UnsupportedSource("Original SMF compile definition has no supported translation: "+name);
		}
	}
	if(!p.Has("SMF_TEXSQUARE_SIZE")||!p.Has("SMF_INTENSITY_MULT"))throw UnsupportedSource("Original SMF required compile constants are missing");
	const bool advanced=p.MRT()||identity.selection==NativeTerrainProgramSelection::ForwardAdvanced;
	if(p.Has("SMF_ADV_SHADING")!=advanced)throw UnsupportedSource("Original SMF selected owner and actual advanced define disagree");
	if(!advanced)for(const auto& [name,value]:p.defines) {
		if(flags.contains(name)&&name!="HAVE_INFOTEX")throw UnsupportedSource("Original standard SMF source has a nonstandard feature definition: "+name);
	}
	p.Int("texSquare",2);p.Int("heightMapTex");p.Float("specularTexGen",2);p.Image("diffuseTex");
	if(advanced){p.Image("normalsTex");p.Float("normalTexGen",2);
		if(!p.deferred){p.Float("lightDir",4);p.Float("groundAmbientColor",3);p.Float("groundDiffuseColor",3);}
		if(!p.Has("SMF_SPECULAR_LIGHTING")){p.Float("groundSpecularColor",3);if(!p.deferred)p.Float("groundSpecularExponent",1);}
		if((!p.deferred&&p.Has("SMF_SKY_REFLECTIONS"))||p.Has("SMF_PARALLAX_MAPPING"))p.Float("cameraPos",3);
	}else p.Image("shadingTex");
	if(p.Has("SMF_DETAIL_NORMAL_TEXTURE_SPLATTING")) {
		p.Image("splatDetailNormalTex1");p.Image("splatDetailNormalTex2");p.Image("splatDetailNormalTex3");p.Image("splatDetailNormalTex4");p.Image("splatDistrTex");
	}else if(p.Has("SMF_DETAIL_TEXTURE_SPLATTING")){p.Image("splatDetailTex");p.Image("splatDistrTex");}
	else p.Image("detailTex");
	if(p.Has("SMF_DETAIL_NORMAL_TEXTURE_SPLATTING")||p.Has("SMF_DETAIL_TEXTURE_SPLATTING")){p.Float("splatTexScales",4);p.Float("splatTexMults",4);}
	if(!p.deferred&&p.Has("HAVE_INFOTEX")){p.Image("infoTex");p.Float("infoTexGen",2);p.Float("infoTexIntensityMul",1);}
	if(p.Has("SMF_SPECULAR_LIGHTING"))p.Image("specularTex");
	if(!p.deferred&&p.Has("HAVE_SHADOWS")){p.Image("shadowTex",ImageKind::Shadow);p.Image("shadowColorTex");p.Float("shadowMat",4,4);p.Float("groundShadowDensity",1);}
	if(!p.deferred&&p.Has("SMF_WATER_ABSORPTION")){p.Float("mapHeights",2);p.Float("waterMinColor",3);p.Float("waterBaseColor",3);p.Float("waterAbsorbColor",3);}
	if(!p.deferred&&p.Has("SMF_SKY_REFLECTIONS")){p.Image("skyReflectTex",ImageKind::Cube);p.Image("skyReflectModTex");}
	if(p.Has("SMF_BLEND_NORMALS"))p.Image("blendNormalsTex");
	if(p.Has("SMF_LIGHT_EMISSION"))p.Image("lightEmissionTex");
	if(p.Has("SMF_PARALLAX_MAPPING"))p.Image("parallaxHeightTex");
	Require(p.fragment.size()<=16,"Actual SMF fragment sampler ABI exceeds Metal's per-stage sampler limit");
	return p;
}
std::string ShaderSource(const Profile& profile,uint32_t imageDepthMask,bool alphaTest193,
	SMFSamplerPolicy211 sampling211=SMFSamplerPolicy211::Generic)
{
	std::ostringstream source;
	// Bit zero is the vertex height image; subsequent bits belong to the
	// fragment Color2D samplers. Cube and comparison samplers keep their types.
	uint32_t allowedDepthMask=profile.composite?0u:1u;
	for(size_t slot=0;slot<profile.fragment.size();++slot)
		if(profile.fragment[slot].kind==ImageKind::Color2D)allowedDepthMask|=uint32_t(1)<<(slot+1);
	Require((imageDepthMask&~allowedDepthMask)==0,"Original terrain image types exceed its sampler ABI");
	source<<"#define NATIVE_SMF_HEIGHT_DEPTH "<<(imageDepthMask&1u)<<'\n';
	source<<"#define NATIVE_SMF_ALPHA_TEST193 "<<unsigned(alphaTest193)<<'\n';
	if(profile.border)source<<"#define NATIVE_SMF_BORDER 1\n";
	if(profile.composite)source<<"#define NATIVE_SMF_COMPOSITE 1\n";
	for(const auto& [name,value]:profile.defines)source<<"#define "<<name<<' '<<value<<'\n';
	for(size_t slot=0;slot<profile.fragment.size();++slot){const auto& name=profile.fragment[slot].name;
		const bool depth=(imageDepthMask&(uint32_t(1)<<(slot+1)))!=0;
		source<<"#define SLOT_"<<name<<' '<<slot<<"\n#define TEX_"<<name<<' '<<slot*2<<'\n';
		source<<"#define TYPE_"<<name<<(depth?" depth2d<float>\n":" texture2d<float>\n")
			<<"#define IMAGE_"<<name<<' '<<slot*2+unsigned(depth)<<'\n';}
	source<<smforiginal::SHADERS;
	if(sampling211==SMFSamplerPolicy211::Generic)return source.str();
	std::string helpers;
	for(const auto& abi:profile.fragment) {
		const bool color=abi.kind==ImageKind::Color2D;
		const bool normals=color&&abi.name=="normalsTex";
		const char* helper=normals?"SMFSampleNormals211":
			(color&&sampling211==SMFSamplerPolicy211::Canonical?"SMFSampleIdentity211":"SMFSample2D");
		helpers+="#define NATIVE_SMF_SAMPLE211_"+abi.name+" "+helper+"\n";
	}
	auto specialized=NativeSMFSpecializeFragment211(source.str(),helpers,sampling211);
	Require(bool(specialized),"Original terrain sampler specialization source contract changed");
	return std::move(*specialized);
}
MTLBlendOperation Equation(ScreenBlendEquation value)
{
	switch(value){case ScreenBlendEquation::Add:return MTLBlendOperationAdd;case ScreenBlendEquation::Subtract:return MTLBlendOperationSubtract;
		case ScreenBlendEquation::ReverseSubtract:return MTLBlendOperationReverseSubtract;case ScreenBlendEquation::Minimum:return MTLBlendOperationMin;
		case ScreenBlendEquation::Maximum:return MTLBlendOperationMax;}
	throw std::runtime_error("Original terrain blend equation is unsupported");
}
// Returns the actual target family, never a requested forward substitution.
bool RequireTarget(const CUSSourceTarget& target)
{
	Require(target.samples>=1&&target.samples<=32&&(target.samples&(target.samples-1))==0,"Original terrain requires a valid actual target sample count");
	const bool deferred=target.colors[1]!=MTLPixelFormatInvalid;
	if(deferred) {
		Require(target.samples==1,"Original deferred terrain remains its actual single-sample MRT target");
		Require(std::all_of(target.colors.begin(),target.colors.begin()+5,[](auto format){return format==MTLPixelFormatRGBA8Unorm;})&&
			(target.combinedColor==MTLPixelFormatInvalid||target.combinedColor==MTLPixelFormatRGBA8Unorm)&&
			target.depth==MTLPixelFormatDepth32Float&&target.stencil==MTLPixelFormatInvalid,
			"Original deferred terrain requires the actual five RGBA8 Map attachments and D32 depth without stencil");
		return true;
	}
	Require(target.combinedColor==MTLPixelFormatInvalid&&target.colors[0]!=MTLPixelFormatInvalid&&
		std::all_of(target.colors.begin()+1,target.colors.end(),[](auto format){return format==MTLPixelFormatInvalid;}),
		"Original forward terrain requires the actual single-color target");
	Require(target.depth==MTLPixelFormatDepth32Float||target.depth==MTLPixelFormatDepth32Float_Stencil8,
		"Original terrain target depth has no supported paired consumer");
	Require(target.stencil==MTLPixelFormatInvalid||target.stencil==MTLPixelFormatStencil8||target.stencil==MTLPixelFormatDepth32Float_Stencil8,
		"Original terrain target stencil has no supported paired consumer");
	return false;
}
auto TerrainPipelineKey(MTLMathMode mathMode,const CUSSourceTarget& target,unsigned sampleShading,
	const ScreenDrawState& surface,uint32_t imageDepthMask,bool validation,const std::array<bool,4>& combinedMask,
	SMFSamplerPolicy211 sampling211=SMFSamplerPolicy211::Generic)
{
	const auto& blend=surface.blend;
	return std::tuple{mathMode,target.colors,target.combinedColor,target.depth,target.stencil,sampleShading,
		validation,imageDepthMask,blend.enabled,blend.sourceColor,blend.destinationColor,blend.sourceAlpha,
		blend.destinationAlpha,blend.colorEquation,blend.alphaEquation,surface.colorWrite,surface.alphaToCoverage,
		surface.alphaTest.enabled,combinedMask,sampling211};
}
using TerrainPipelineKeyType=decltype(TerrainPipelineKey(MTLMathModeSafe,std::declval<const CUSSourceTarget&>(),0,
	std::declval<const ScreenDrawState&>(),0,false,std::declval<const std::array<bool,4>&>()));

// Only the real compiler can establish this validation proof; input equality
// and live library availability are still checked at every use.
bool TerrainProgramValidation220Enabled() {
 static const bool enabled=[] { const char* value=std::getenv("BAR_NATIVE_TERRAIN_PROGRAM_VALIDATION220");const bool active=value&&std::strcmp(value,"1")==0;const char* counters=std::getenv("BAR_NATIVE_TERRAIN_PROGRAM_VALIDATION220_COUNTERS");if(counters&&std::strcmp(counters,"1")==0)std::fprintf(stderr,"[NativeTerrainProgramValidation220] enabled=%d exact_identity=1 current_library_check=1 original_fallback=1\n",int(active));return active; }();
 return enabled;
}
class OriginalProgram final:public NativeTerrainProgramConsumer {
public:
	// Set after the original validator succeeds, before this owner is published.
	bool identityValidated220=false;
	bool ReuseIdentityValidation220(const NativeTerrainProgramIdentity& requested,uint64_t context)const noexcept {
		return identityValidated220&&context&&ContextGeneration()==context&&LibraryGeneration()&&Supports(requested);
	}
	const NativeTerrainProgramIdentity identity;
	const Profile profile;
	static int NormalSlot211(const Profile& p) {
		for(size_t slot=0;slot<p.fragment.size();++slot)
			if(p.fragment[slot].kind==ImageKind::Color2D&&p.fragment[slot].name=="normalsTex")return int(slot);
		return -1;
	}
	const int normalSlot211;
	const bool samplerSourceSupported211=NativeSMFSamplerSourceSupported211(smforiginal::SHADERS);
	// Exact192 forward profiles, reconstructed from the selected original map,
	// shader flag producer and cached MVK stages. Do not generalize a cached
	// math policy to another feature combination or an unobserved family.
	static MTLMathMode MathMode195(const Profile& p) {
		if(InterventionFastMathEnabled())return MTLMathModeFast;
		if(p.border||p.deferred||p.composite||p.combined)return MTLMathModeSafe;
		Definitions expected={{"SMF_TEXSQUARE_SIZE","1024.000000"},{"SMF_INTENSITY_MULT","0.823486"},
			{"SMF_ADV_SHADING",""},{"SMF_SPECULAR_LIGHTING",""},{"SMF_DETAIL_TEXTURE_SPLATTING",""},
			{"SMF_DETAIL_NORMAL_TEXTURE_SPLATTING",""},{"SMF_DETAIL_NORMAL_DIFFUSE_ALPHA",""},
			{"SMF_WATER_ABSORPTION",""},{"SMF_BLEND_NORMALS",""},{"HAVE_SHADOWS",""}};
		if(p.Has("HAVE_INFOTEX"))expected.emplace("HAVE_INFOTEX","");
		return p.defines==expected?MTLMathModeRelaxed:MTLMathModeSafe;
	}
	const MTLMathMode mathMode195;
	const uint64_t generation;
	const std::shared_ptr<const int> factory;
	__strong id<MTLLibrary> library=nil;
	mutable std::map<std::tuple<uint32_t,bool,MTLMathMode,SMFSamplerPolicy211>,id<MTLLibrary>> imageLibraries;
	mutable std::map<TerrainPipelineKeyType,id<MTLRenderPipelineState>> pipelines;
	OriginalProgram(NativeTerrainProgramIdentity id,Profile p,uint64_t g,std::shared_ptr<const int> f):
		identity(std::move(id)),profile(std::move(p)),normalSlot211(NormalSlot211(profile)),mathMode195(MathMode195(profile)),generation(g),factory(std::move(f)){}
	uint64_t ContextGeneration()const noexcept override{return identity.contextGeneration;}
	uint64_t LibraryGeneration()const noexcept override{return library&&!pipelines.empty()?generation:0;}
	bool Matches(const NativeTerrainProgramIdentity& requested)const noexcept override{return identity==requested;}
	bool Supports(const NativeTerrainProgramIdentity& requested)const noexcept override {
		return Matches(requested)&&library&&generation&&!pipelines.empty();
	}
	id<MTLLibrary> LibraryForImages(RuntimeLease& lease,uint32_t imageDepthMask,bool alphaTest193,double& milliseconds,
		SMFSamplerPolicy211 sampling211=SMFSamplerPolicy211::Generic)const {
		// A disabled legacy alpha test has no discard in the original GL state.
		// Keep the enabled source intact and key both library and PSO by the
		// actual captured draw state, including non-default image types.
		if(imageDepthMask==0&&!alphaTest193&&sampling211==SMFSamplerPolicy211::Generic)return library;
		const auto key=std::make_tuple(imageDepthMask,alphaTest193,mathMode195,sampling211);
		if(auto found=imageLibraries.find(key);found!=imageLibraries.end())return found->second;
		const auto start=Clock::now();NSError* error=nil;
		auto options=[MTLCompileOptions new];options.mathMode=mathMode195;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(options);
		auto selected=lease.NewLibraryWithSource(String(ShaderSource(profile,imageDepthMask,alphaTest193,sampling211)),options,&error);
		Require(selected!=nil,"Cannot compile original SMF terrain image-type library: "+Error(error));
		imageLibraries.emplace(key,selected);milliseconds+=Milliseconds(start);return selected;
	}
	id<MTLRenderPipelineState> Pipeline(RuntimeLease& lease,const CUSSourceTarget& target,
		const ScreenDrawState& surface,uint32_t imageDepthMask,bool validation,double& milliseconds,
		const std::array<bool,4>& combinedMask={true,true,true,true},
		SMFSamplerPolicy211 sampling211=SMFSamplerPolicy211::Generic)const {
		Require(RequireTarget(target)==profile.MRT()&&(!profile.combined||target.combinedColor==MTLPixelFormatRGBA8Unorm),"Original terrain program output family differs from the actual render target");
		Require([lease.Device() supportsTextureSampleCount:target.samples],"Original terrain target count is unsupported by its actual device");
		Require(library!=nil&&library.device==lease.Device(),"Original terrain PSO requested before its genuine same-device library");
		auto selectedLibrary=LibraryForImages(lease,imageDepthMask,surface.alphaTest.enabled,milliseconds);
		const auto& blend=surface.blend;
		const auto key=TerrainPipelineKey(mathMode195,target,lease.SampleShadingKey(target.samples),surface,imageDepthMask,validation,combinedMask,sampling211);
		if(auto found=pipelines.find(key);found!=pipelines.end()){lease.Retain(selectedLibrary);lease.Retain(found->second);return found->second;}
		// On cache hits, no specialized-library lookup or additional lease retain.
		// Each prepared Draw owns this OriginalProgram; its imageLibraries map
		// strongly owns the fragment library through completion/quarantine.
		auto fragmentLibrary=sampling211==SMFSamplerPolicy211::Generic?selectedLibrary:
			LibraryForImages(lease,imageDepthMask,surface.alphaTest.enabled,milliseconds,sampling211);
		const auto start=Clock::now();NSError* error=nil;
		auto descriptor=[MTL4RenderPipelineDescriptor new];
		auto vertex=[MTL4LibraryFunctionDescriptor new],fragment=[MTL4LibraryFunctionDescriptor new];
		vertex.library=selectedLibrary;fragment.library=fragmentLibrary;vertex.name=@"NativeSMFTerrainVertex";fragment.name=@"NativeSMFTerrainFragment";
		descriptor.vertexFunctionDescriptor=vertex;descriptor.fragmentFunctionDescriptor=fragment;
		descriptor.rasterSampleCount=target.samples;descriptor.alphaToCoverageState=surface.alphaToCoverage?MTL4AlphaToCoverageStateEnabled:MTL4AlphaToCoverageStateDisabled;descriptor.inputPrimitiveTopology=MTLPrimitiveTopologyClassTriangle;
		unsigned mask=0;for(unsigned c=0;c<4;++c)if(surface.colorWrite[c])mask|=MTLColorWriteMaskRed>>c;
		for(unsigned i=0;i<(profile.MRT()?5u:1u);++i) {
			auto attachment=descriptor.colorAttachments[i];attachment.pixelFormat=target.colors[i];
			attachment.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
			attachment.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);attachment.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
			attachment.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);attachment.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
			attachment.rgbBlendOperation=Equation(blend.colorEquation);attachment.alphaBlendOperation=Equation(blend.alphaEquation);
			attachment.writeMask=MTLColorWriteMask(mask);
		}
		if(target.combinedColor!=MTLPixelFormatInvalid){
			auto a=descriptor.colorAttachments[5];a.pixelFormat=target.combinedColor;
			a.blendingState=blend.enabled?MTL4BlendStateEnabled:MTL4BlendStateDisabled;
			a.sourceRGBBlendFactor=effectstate::Factor(blend.sourceColor);a.destinationRGBBlendFactor=effectstate::Factor(blend.destinationColor);
			a.sourceAlphaBlendFactor=effectstate::Factor(blend.sourceAlpha);a.destinationAlphaBlendFactor=effectstate::Factor(blend.destinationAlpha);
			a.rgbBlendOperation=Equation(blend.colorEquation);a.alphaBlendOperation=Equation(blend.alphaEquation);unsigned combinedBits=0;for(unsigned c=0;c<4;++c)if(combinedMask[c])combinedBits|=MTLColorWriteMaskRed>>c;
			a.writeMask=profile.combined?MTLColorWriteMask(combinedBits):MTLColorWriteMaskNone;
		}
		auto options=[MTL4PipelineOptions new];options.shaderValidation=ShaderValidationMode(validation);descriptor.options=options;
		auto pipeline=lease.NewRenderPipeline(descriptor,&error);
		Require(pipeline!=nil,"Cannot compile original SMF terrain pipeline: "+Error(error));
		pipelines.emplace(key,pipeline);lease.Retain(pipeline);lease.Retain(selectedLibrary);milliseconds+=Milliseconds(start);return pipeline;
	}
};
struct alignas(16) ImageUniform {simd_uint4 flags,swizzle;simd_float4 parameters;};
struct alignas(16) Uniform {
	simd_float4x4 modelView,projection,modelViewInverse,shadowMat;
	simd_float4 texGen0,texGen1,cameraPos,lightDir,groundAmbient,groundDiffuse,groundSpecular;
	simd_float4 waterMin,waterBase,waterAbsorb,mapHeights,splatScales,splatMults,fogColor,fogParameters;
	simd_float4 clipEye[3],inheritedClip;
	simd_uint4 flags;
	simd_int4 texSquare;
	simd_float4 alpha;
	ImageUniform heightImage,images[16];
};
static_assert(sizeof(ImageUniform)==48&&alignof(Uniform)==16);
static_assert(offsetof(Uniform,heightImage)==608&&offsetof(Uniform,images)==656&&sizeof(Uniform)==1424);
// Uniform and sampler checks run for every terrain draw. Build the named
// diagnostic only when the original check fails.
void RequireNamedTerrainInput(bool condition,const char* message,const char* name)
{
	if(!condition)Require(false,std::string(message)+name);
}
const NativeTerrainUniformValue& Value(const NativeTerrainUniformSnapshot& uniforms,const char* name,unsigned rows,unsigned columns=1)
{
	const auto it=uniforms.values.find(name);
	RequireNamedTerrainInput(it!=uniforms.values.end(),"Actual original terrain uniform is unobserved: ",name);
	const auto& value=it->second;
	RequireNamedTerrainInput(value.rows==rows&&value.columns==columns&&value.count==1,
		"Actual original terrain uniform shape differs: ",name);
	return value;
}
simd_float4 Float(const NativeTerrainUniformSnapshot& uniforms,const char* name,unsigned components)
{
	const auto& value=Value(uniforms,name,components);
	const auto* bytes=std::get_if<std::vector<float>>(&value.values);
	RequireNamedTerrainInput(bytes&&bytes->size()==components&&!value.transpose,"Actual original terrain float uniform type differs: ",name);
	simd_float4 result{};
	for(unsigned i=0;i<components;++i){Require(std::isfinite((*bytes)[i]),"Original terrain float uniform is nonfinite");result[i]=(*bytes)[i];}
	return result;
}
simd_int4 Int(const NativeTerrainUniformSnapshot& uniforms,const char* name,unsigned components)
{
	const auto& value=Value(uniforms,name,components);
	const auto* bytes=std::get_if<std::vector<int32_t>>(&value.values);
	RequireNamedTerrainInput(bytes&&bytes->size()==components&&!value.transpose,"Actual original terrain integer uniform type differs: ",name);
	simd_int4 result{};for(unsigned i=0;i<components;++i)result[i]=(*bytes)[i];return result;
}
simd_float4x4 Matrix(const float* source,bool transpose=false)
{
	simd_float4x4 result;
	for(unsigned c=0;c<4;++c)for(unsigned r=0;r<4;++r){const float value=source[transpose?r*4+c:c*4+r];
		Require(std::isfinite(value),"Original terrain matrix contains a nonfinite value");result.columns[c][r]=value;}
	return result;
}
simd_float4x4 Matrix(const NativeTerrainUniformSnapshot& uniforms,const char* name)
{
	const auto& value=Value(uniforms,name,4,4);const auto* bytes=std::get_if<std::vector<float>>(&value.values);
	Require(bytes&&bytes->size()==16,"Original terrain matrix uniform type differs");return Matrix(bytes->data(),value.transpose);
}
simd_float4 Vec(const std::array<float,4>& source)
{
	simd_float4 result;for(unsigned i=0;i<4;++i){Require(std::isfinite(source[i]),"Original terrain fixed-function state is nonfinite");result[i]=source[i];}return result;
}
Uniform DrawUniform(const NativeTerrainSquareDraw& draw,const Profile& profile,const CUSSourceTarget& target)
{
	Require(draw.program.uniforms&&draw.program.uniforms->contextGeneration==draw.program.identity.contextGeneration&&
		draw.program.uniforms->originalProgramGeneration==draw.program.identity.originalProgramGeneration,
		"Original terrain draw uniform lifetime differs from its compiled program");
	const auto& source=*draw.program.uniforms;Uniform result{};
	result.modelView=Matrix(draw.modelView.data());result.projection=Matrix(draw.projection.data());
	result.flags.x=(target.camera.clipDepth==ClipDepthConvention::MinusOneToOne?1u:0u)|(target.invertClipY?2u:0u);
	result.flags.z=draw.surface.alphaTest.enabled;result.flags.w=uint32_t(draw.surface.alphaTest.compare);
	Require(result.flags.w<=uint32_t(ScreenAlphaCompare::Always)&&std::isfinite(draw.surface.alphaTest.reference),"Original terrain alpha-test state is invalid");
	result.alpha.x=std::clamp(draw.surface.alphaTest.reference,0.0f,1.0f);
	if(profile.composite)return result;
	result.texSquare=Int(source,"texSquare",2);
	if(profile.border) {
		result.texGen0=Float(source,"mapSize",4);result.mapHeights.x=Float(source,"borderMinHeight",1).x;
		Require(result.texGen0.x>0&&result.texGen0.y>0&&result.texGen0.z>0&&result.texGen0.w>0,
			"Original border mapSize cannot address its actual height map");
		// SMFBorderVertProg does not write gl_ClipVertex. Do not invent its value
		// when an actual legacy user plane would consume that undefined output.
		Require(std::none_of(draw.clipEnabled.begin(),draw.clipEnabled.end(),[](bool enabled){return enabled;})&&!draw.world.inheritedClipPlane,
			"Original SMF border with active user clipping requires its actual defined clip-vertex source");
	}else{
		result.texGen0=Float(source,"specularTexGen",2);
		Require(result.texGen0.x>0&&result.texGen0.y>0,"Original terrain specularTexGen cannot address its actual map");
	}
	if(profile.Has("SMF_ADV_SHADING")) {
		const auto normal=Float(source,"normalTexGen",2);result.texGen0.z=normal.x;result.texGen0.w=normal.y;
		if(!profile.Has("SMF_SPECULAR_LIGHTING")) {
			result.groundSpecular=Float(source,"groundSpecularColor",3);
			if(!profile.deferred)result.groundSpecular.w=Float(source,"groundSpecularExponent",1).x;
		}
		if((!profile.deferred&&profile.Has("SMF_SKY_REFLECTIONS"))||profile.Has("SMF_PARALLAX_MAPPING"))result.cameraPos=Float(source,"cameraPos",3);
		if(!profile.deferred) {
			result.lightDir=Float(source,"lightDir",4);result.groundAmbient=Float(source,"groundAmbientColor",3);result.groundDiffuse=Float(source,"groundDiffuseColor",3);
			const auto determinant=simd_determinant(result.modelView);
			Require(std::isfinite(determinant)&&determinant!=0,"Original terrain model-view inverse is undefined");
			result.modelViewInverse=simd_inverse(result.modelView);
			for(auto column:result.modelViewInverse.columns)for(unsigned row=0;row<4;++row)Require(std::isfinite(column[row]),"Original terrain model-view inverse is nonfinite");
		}
	}
	if(!profile.deferred&&profile.Has("HAVE_INFOTEX")){result.texGen1=Float(source,"infoTexGen",2);result.texGen1.z=Float(source,"infoTexIntensityMul",1).x;}
	if(!profile.deferred&&profile.Has("HAVE_SHADOWS")){result.shadowMat=Matrix(source,"shadowMat");result.mapHeights.z=Float(source,"groundShadowDensity",1).x;}
	if(!profile.deferred&&profile.Has("SMF_WATER_ABSORPTION")){const auto heights=Float(source,"mapHeights",2);result.mapHeights.x=heights.x;result.mapHeights.y=heights.y;
		result.waterMin=Float(source,"waterMinColor",3);result.waterBase=Float(source,"waterBaseColor",3);result.waterAbsorb=Float(source,"waterAbsorbColor",3);}
	if(profile.Has("SMF_DETAIL_NORMAL_TEXTURE_SPLATTING")||profile.Has("SMF_DETAIL_TEXTURE_SPLATTING")){
		result.splatScales=Float(source,"splatTexScales",4);result.splatMults=Float(source,"splatTexMults",4);}
	if(!profile.deferred){result.fogColor=Vec(draw.fogColor);result.fogParameters=Vec(draw.fogParameters);}
	for(unsigned i=0;i<3;++i)if(draw.clipEnabled[i]){result.flags.y|=1u<<i;result.clipEye[i]=Vec(draw.clipEyeEquations[i]);}
	if(draw.world.inheritedClipPlane){result.flags.y|=8u;result.inheritedClip=Vec(*draw.world.inheritedClipPlane);}
	return result;
}
MTLSamplerAddressMode Wrap(unsigned value)
{
	switch(value){case 0x2901:return MTLSamplerAddressModeRepeat;case 0x8370:return MTLSamplerAddressModeMirrorRepeat;
		case 0x812F:return MTLSamplerAddressModeClampToEdge;case 0x812D:return MTLSamplerAddressModeClampToBorderColor;
		case 0x2900:throw std::runtime_error("Original terrain GL_CLAMP requires its exact half-border sampler; edge/border substitution is forbidden");}
	throw std::runtime_error("Original terrain texture wrapping mode is unsupported");
}
MTLCompareFunction TextureCompare(unsigned value)
{
	Require(value>=0x0200&&value<=0x0207,"Original terrain comparison function is invalid");
	return effectstate::Compare(ScreenAlphaCompare(value-0x0200));
}
bool Mipmapped(const NativeTextureSampling& state){return state.minFilter>=0x2700&&state.minFilter<=0x2703;}
id<MTLSamplerState> NewSampler(RuntimeLease& lease,const NativeTextureSampling& state,bool shadow)
{
	ValidateNativeTerrainSampling(state);
	Require(state.compareMode==(shadow?0x884eu:0u),"Original terrain sampler comparison mode does not match the active original sampler type");
	Require(state.minLod<=state.maxLod&&state.maxLod>=0,"Original terrain negative or inverted LOD range needs an exact unsupported sampler implementation");
	Require(state.anisotropy<=16&&std::floor(state.anisotropy)==state.anisotropy,"Original terrain anisotropy is not representable exactly by the native sampler");
	Require(state.magFilter==0x2600||state.magFilter==0x2601,"Original terrain magnification filter is invalid");
	auto descriptor=[MTLSamplerDescriptor new];
	switch(state.minFilter){
		case 0x2600:descriptor.minFilter=MTLSamplerMinMagFilterNearest;descriptor.mipFilter=MTLSamplerMipFilterNotMipmapped;break;
		case 0x2601:descriptor.minFilter=MTLSamplerMinMagFilterLinear;descriptor.mipFilter=MTLSamplerMipFilterNotMipmapped;break;
		case 0x2700:descriptor.minFilter=MTLSamplerMinMagFilterNearest;descriptor.mipFilter=MTLSamplerMipFilterNearest;break;
		case 0x2701:descriptor.minFilter=MTLSamplerMinMagFilterLinear;descriptor.mipFilter=MTLSamplerMipFilterNearest;break;
		case 0x2702:descriptor.minFilter=MTLSamplerMinMagFilterNearest;descriptor.mipFilter=MTLSamplerMipFilterLinear;break;
		case 0x2703:descriptor.minFilter=MTLSamplerMinMagFilterLinear;descriptor.mipFilter=MTLSamplerMipFilterLinear;break;
		default:throw std::runtime_error("Original terrain minification filter is invalid");
	}
	descriptor.magFilter=state.magFilter==0x2600?MTLSamplerMinMagFilterNearest:MTLSamplerMinMagFilterLinear;
	descriptor.sAddressMode=Wrap(state.wrapS);descriptor.tAddressMode=Wrap(state.wrapT);descriptor.rAddressMode=Wrap(state.wrapR);
	if(state.wrapS==0x812D||state.wrapT==0x812D||state.wrapR==0x812D){
		if(state.borderColor==std::array<float,4>{0,0,0,0})descriptor.borderColor=MTLSamplerBorderColorTransparentBlack;
		else if(state.borderColor==std::array<float,4>{0,0,0,1})descriptor.borderColor=MTLSamplerBorderColorOpaqueBlack;
		else if(state.borderColor==std::array<float,4>{1,1,1,1})descriptor.borderColor=MTLSamplerBorderColorOpaqueWhite;
		else throw std::runtime_error("Original terrain arbitrary border color requires its exact sampler implementation");
	}
	descriptor.normalizedCoordinates=YES;descriptor.lodMinClamp=std::max(0.0f,state.minLod);descriptor.lodMaxClamp=state.maxLod;
	descriptor.maxAnisotropy=NSUInteger(state.anisotropy);descriptor.supportArgumentBuffers=YES;
	if(shadow){Require(state.lodBias==0,"Original shadow sampler LOD bias requires its exact comparison sampling implementation");descriptor.compareFunction=TextureCompare(state.compareFunction);}
	auto sampler=[lease.Device()newSamplerStateWithDescriptor:descriptor];Require(sampler!=nil,"Cannot create actual original terrain sampler");return sampler;
}
void ImageParameters(ImageUniform& result,const NativeTextureSampling& sampling,const CUSSourceImage& image,bool explicitLod)
{
	Require(sampling.depthMode==0x1909||sampling.depthMode==0x1906||sampling.depthMode==0x8049||sampling.depthMode==0x1903,
		"Original terrain depth-texture mode is unsupported");
	result.flags={uint32_t(image.depthImage),uint32_t(image.flipV),sampling.depthMode,uint32_t(image.clampDepthCompare)};
	for(unsigned i=0;i<4;++i){const auto channel=sampling.swizzle[i];Require(channel==0||channel==1||(channel>=0x1903&&channel<=0x1906),"Original terrain texture swizzle is invalid");result.swizzle[i]=channel;}
	// textureLod ignores object LOD bias. Metal explicit levels also ignore its
	// sampler LOD clamps, so the vertex expression receives the original clamp.
	result.parameters={explicitLod?0.0f:sampling.lodBias,Mipmapped(sampling)?sampling.minLod:0.0f,Mipmapped(sampling)?sampling.maxLod:0.0f,0};
}
id<MTLTexture> MipView(RuntimeLease& lease,const NativeCUSTextureObject& object,id<MTLTexture> original,InterventionMipViews* cache)
{
	const auto& state=object.sampling;const auto& allocation=object.allocation;
	Require(original.width==allocation.width&&original.height==allocation.height&&original.mipmapLevelCount>=allocation.levels,
		"Original terrain image's physical level0 or retained mip allocation differs from its actual GL object");
	Require(allocation.levels&&allocation.levels<=32&&uint32_t(state.baseLevel)<allocation.levels&&state.maxLevel>=state.baseLevel,
		"Original terrain texture base/max levels describe incomplete source storage");
	const auto base=uint32_t(state.baseLevel);
	uint32_t last=base;
	if(Mipmapped(state)) {
		uint32_t size=std::max(allocation.width,allocation.height)>>base;
		while(size>1){size>>=1;++last;}
		last=std::min(last,uint32_t(state.maxLevel));
		Require(last<allocation.levels,"Original terrain mipmapped source is incomplete; no replacement pixels may be supplied");
	}
	const uint32_t count=last-base+1;
	if(base==0&&count==original.mipmapLevelCount)return original;
	const NSRange slices=NSMakeRange(0,original.textureType==MTLTextureTypeCube?6:1);
	// Preserve provider-owned original-format expansion (e.g. BC1_RGB alpha1)
	// across the mip view. This is distinct from the source GL texture swizzle,
	// which is still applied exactly once by the SMF sampling adapter.
	auto create=[&](){return [original newTextureViewWithPixelFormat:original.pixelFormat textureType:original.textureType levels:NSMakeRange(base,count) slices:slices swizzle:original.swizzle];};
	auto view=cache?cache->Acquire(original,base,count,create):create();
	Require(view!=nil,"Cannot retain exact original terrain base/max-level image view");lease.AddAllocation(view);return view;
}
NativePolygonMode PolygonMode(const NativeWorldDrawState& state)
{
	Require(state.cull<=NativeWorldCull::FrontAndBack&&state.frontPolygonMode<=NativePolygonMode::Fill&&state.backPolygonMode<=NativePolygonMode::Fill,
		"Original terrain culling/polygon state is invalid");
	if(state.cullEnabled&&state.cull==NativeWorldCull::Front)return state.backPolygonMode;
	if(state.cullEnabled&&state.cull==NativeWorldCull::Back)return state.frontPolygonMode;
	Require(state.frontPolygonMode==state.backPolygonMode,"Original terrain independent front/back polygon modes require separate native face draws");
	return state.frontPolygonMode;
}
} // namespace

struct PreparedTerrainSource::Impl {
 RetiredArgumentOwners argumentOwners;
 std::vector<std::shared_ptr<ScreenUploadStorage>> uniformOwners;
	struct Draw {
		uint64_t sequence=0;
		NativeWorldDrawState world;
		NativePolygonMode polygon=NativePolygonMode::Fill;
		std::optional<effectstate::Raster> raster;
		std::shared_ptr<const OriginalProgram> program;
		__strong id<MTLRenderPipelineState> pipeline=nil;
		__strong id<MTLDepthStencilState> depth=nil;
		__strong id<MTLBuffer> uniforms=nil;
		__strong id<MTL4ArgumentTable> fragment=nil;
		__strong id<MTL4ArgumentTable> vertex=nil;
		__strong id<MTLBuffer> positions=nil,colors=nil,indices=nil;
		uint32_t count=0;
		bool indexed=false;
		std::vector<CUSSourceImage> images;
	};
	std::shared_ptr<const NativeWaterGroundSourceFrame> source;
	std::vector<Draw> draws;
	bool dependencyAttempted=false,dependencies=false,finished=false,completed=false;
	size_t encoded=0;
	uint32_t encodedDraws=0;
};
PreparedTerrainSource::PreparedTerrainSource():impl(std::make_unique<Impl>()){}
PreparedTerrainSource::~PreparedTerrainSource()=default;
size_t PreparedTerrainSource::DrawCount()const{return impl->draws.size();}
uint32_t PreparedTerrainSource::EncodedDrawCount()const{return impl->encodedDraws;}
uint64_t PreparedTerrainSource::SequenceAt(size_t index)const
{
	Require(index<DrawCount(),"Original terrain ordered draw index exceeds source");return impl->draws[index].sequence;
}
void PreparedTerrainSource::EncodeDependencies(id<MTL4CommandBuffer> command)
{
	Require(command&&!impl->dependencyAttempted&&!impl->completed,"Original terrain dependencies are missing or repeated");
	impl->dependencyAttempted=true; // A failed encode may not be retried on a new command.
	std::set<const void*> seen;
	for(const auto& draw:impl->draws)for(const auto& image:draw.images)
		if(seen.insert(image.owner.get()).second&&image.encodeDependencies)image.encodeDependencies(command);
	impl->dependencies=true;
}
void PreparedTerrainSource::EncodeDraw(size_t index,id<MTL4RenderCommandEncoder> encoder)
{
	Require(encoder&&impl->dependencies&&!impl->finished&&!impl->completed&&index==impl->encoded&&index<DrawCount(),
		"Original terrain ordered draw is repeated/skipped or lacks its same-submission dependencies");
	const auto& draw=impl->draws[index];
	if(draw.raster&&(!draw.world.cullEnabled||draw.world.cull!=NativeWorldCull::FrontAndBack)) {
		for(const auto& image:draw.images)if(image.requireReadable)image.requireReadable();
		effectstate::RasterState(encoder,draw.world,*draw.raster);
		[encoder setDepthClipMode:draw.world.depthClamp?MTLDepthClipModeClamp:MTLDepthClipModeClip];
		[encoder setCullMode:!draw.world.cullEnabled||draw.world.cull==NativeWorldCull::None?MTLCullModeNone:draw.world.cull==NativeWorldCull::Front?MTLCullModeFront:MTLCullModeBack];
		[encoder setTriangleFillMode:draw.polygon==NativePolygonMode::Line?MTLTriangleFillModeLines:MTLTriangleFillModeFill];
		[encoder setRenderPipelineState:draw.pipeline];[encoder setDepthStencilState:draw.depth];
		[encoder setArgumentTable:draw.fragment atStages:MTLRenderStageFragment];
		Require(impl->encodedDraws!=std::numeric_limits<uint32_t>::max(),"Original terrain actual encoded draw count overflow");
		[encoder setArgumentTable:draw.vertex atStages:MTLRenderStageVertex];
		// These are the original GL_TRIANGLES submits, each with one instance.
		if(draw.indexed)
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:draw.count indexType:MTLIndexTypeUInt32 indexBuffer:draw.indices.gpuAddress indexBufferLength:draw.indices.length];
		else
			[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:draw.count instanceCount:1];
		++impl->encodedDraws;
	}
	++impl->encoded;
}
void PreparedTerrainSource::FinishOrderedDraws()
{
	Require(impl->dependencies&&!impl->finished&&!impl->completed&&impl->encoded==DrawCount(),"Original terrain source list did not finish exactly once");impl->finished=true;
}
void PreparedTerrainSource::DidComplete()
{
	Require(impl->finished&&!impl->completed,"Original terrain retirement preceded its successful real submission");
	std::set<const void*> seen;
	for(const auto& draw:impl->draws)for(const auto& image:draw.images)
		if(seen.insert(image.owner.get()).second&&image.didComplete)image.didComplete();
	impl->completed=true;
}
struct TerrainSourceGPU::Impl {
 RetiredArguments argumentPool;
 ScreenUploadPool uniformPool;
 InterventionMipViews mipViews;
 TerrainGeometryCache geometryCache;
	std::shared_ptr<const int> factory=std::make_shared<const int>(0);
	uint64_t nextGeneration=0;
	std::vector<std::pair<NativeTextureSampling,id<MTLSamplerState>>> samplers;
	id<MTLSamplerState> Sampler(RuntimeLease& lease,const NativeTextureSampling& state,bool shadow) {
		// compareMode distinguishes shadow/nonshadow entries; reject a type
		// mismatch before lookup so a cached sampler cannot bypass this check.
		Require(state.compareMode==(shadow?0x884eu:0u),"Original terrain sampler type and source compare mode disagree");
		for(const auto& cached:samplers)if(cached.first==state){lease.Retain(cached.second);return cached.second;}
		auto sampler=NewSampler(lease,state,shadow);samplers.emplace_back(state,sampler);lease.Retain(sampler);return sampler;
	}
};
TerrainSourceGPU::TerrainSourceGPU():impl(std::make_unique<Impl>()){}
TerrainSourceGPU::~TerrainSourceGPU()=default;
NativeTerrainProgramLink TerrainSourceGPU::CompileOriginalProgram(RuntimeLease& lease,const NativeTerrainProgramRequest& request,
	const CUSSourceTarget& target,bool validation,double& milliseconds)
{
	Profile profile;
	try{profile=OriginalProfile(request);}catch(const UnsupportedSource& unsupported){return {nullptr,unsupported.what(),{}};}
	Require(RequireTarget(target)==profile.MRT()&&(!profile.combined||target.combinedColor==MTLPixelFormatRGBA8Unorm),"Original terrain link target differs from its actual fragment output family");
	Require(impl->nextGeneration!=std::numeric_limits<uint64_t>::max(),"Original terrain library generation overflow");
	auto original=std::make_shared<OriginalProgram>(request.identity,std::move(profile),++impl->nextGeneration,impl->factory);
	const auto start=Clock::now();NSError* error=nil;
	auto options=[MTLCompileOptions new];options.mathMode=original->mathMode195;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;ConfigureInterventionColorMath(options);
	original->library=lease.NewLibraryWithSource(String(ShaderSource(original->profile,0,false)),options,&error);
	Require(original->library!=nil,"Cannot compile actual original SMF terrain library: "+Error(error));
	milliseconds+=Milliseconds(start);
	ScreenDrawState initial;initial.blend.enabled=false;
	original->Pipeline(lease,target,initial,0,validation,milliseconds);
	ValidateNativeTerrainProgram(request.identity,original,request.identity.contextGeneration);
	original->identityValidated220=true;
	return {original,{},original->profile.defaults};
}
std::shared_ptr<PreparedTerrainSource> TerrainSourceGPU::Prepare(RuntimeLease& lease,
	std::shared_ptr<const NativeWaterGroundSourceFrame> source,
	const CUSSourceTarget& target,const TerrainSourceImageProvider& provider,bool validation,double& milliseconds)
{
	Require(source&&source->contextGeneration&&provider,"Original terrain source frame/provider is missing");
	Require(source->stage==NativeWorldDrawStage::Ground||source->stage==NativeWorldDrawStage::TerrainReflection||
		source->stage==NativeWorldDrawStage::WaterReflection||source->stage==NativeWorldDrawStage::WaterRefraction,
		"Original terrain source belongs to another actual ground draw stage");
	const bool deferred=RequireTarget(target);
	Require(source->deferredEntered==source->deferred.has_value(),"Original terrain deferred entry lacks its completed source span");
	if(source->deferred) {
		const auto& span=*source->deferred;
		Require(source->drawEntered&&source->stage==NativeWorldDrawStage::Ground&&span.begin<span.end&&
			source->source.begin<=span.begin&&span.end<=source->source.end,
			"Original Normal deferred span does not belong to its actual ground invocation");
	}
	Require(!deferred||source->deferredEntered,"Original deferred terrain target lacks an entered original deferred pass");
	Require(target.width&&target.height&&target.logicalWidth&&target.logicalHeight&&
		(target.camera.clipDepth==ClipDepthConvention::MinusOneToOne||target.camera.clipDepth==ClipDepthConvention::ZeroToOne),
		"Original terrain target viewport or actual clip convention is unobserved");
	auto prepared=std::shared_ptr<PreparedTerrainSource>(new PreparedTerrainSource);auto& out=*prepared->impl;out.source=std::move(source);
	const auto uniformCount=std::count_if(out.source->squares.begin(),out.source->squares.end(),[&](const auto& draw){return draw.deferred==deferred;});
	const bool reuse=InterventionPoolsEnabled();
	if(reuse){impl->argumentPool.BeginBatch(lease);impl->uniformPool.BeginBatch(lease.Device());impl->mipViews.Begin(lease);}
	TerrainUniformBatch uniformBatch(lease,uniformCount,sizeof(Uniform),9,reuse?&impl->uniformPool:nullptr,reuse?&out.uniformOwners:nullptr);
	uint64_t previous=0;
	for(size_t index=0;index<out.source->squares.size();++index) {
		const auto& draw=out.source->squares[index];const auto& mesh=draw.Geometry();
		Require(out.source->drawEntered&&!(draw.border&&draw.deferred)&&
			(draw.deferred?out.source->deferredEntered:draw.border?out.source->borderEntered:draw.composite?out.source->compositeEntered:out.source->forwardEntered)&&
			draw.sequence>previous&&out.source->source.Contains(draw.sequence),
			"Original terrain submission does not belong to its entered deferred/forward/border source span");previous=draw.sequence;
		Require(draw.deferred==bool(out.source->deferred&&out.source->deferred->Contains(draw.sequence)),
			"Original terrain submission disagrees with its actual deferred source span");
		// One immutable frame retains both original passes. Each actual target
		// consumes only its own source submissions without renumbering ordinals.
		if(draw.deferred!=deferred)continue;
		const size_t count=mesh.indexed?mesh.indices.size():mesh.positions.size();
		Require(count&&count<=size_t(INT32_MAX)&&count%3==0&&
			(mesh.indexed?draw.originalIndexCount==mesh.indices.size():draw.originalIndexCount==0)&&
			!mesh.positions.empty()&&mesh.positions.size()<=size_t(INT32_MAX),
			"Original terrain immutable geometry differs from its actual nonempty GL_TRIANGLES submission");
		Require(mesh.indexed||mesh.indices.empty(),"Original non-indexed terrain submission carries replacement indices");
		Require(draw.border?mesh.colors.size()==mesh.positions.size():mesh.colors.empty(),
			"Original terrain border color attribute differs from the submitted position stream");
		// Exact byte comparison against the validated immutable cache entry
		// reuses geometry validation; misses validate inside Acquire before upload.
		Require(draw.mesh==NativeTerrainMeshSource::Basic||draw.mesh==NativeTerrainMeshSource::ROAM||(draw.mesh==NativeTerrainMeshSource::Composite&&draw.composite),"Original terrain mesh provenance is unsupported");
		const auto validatedProgram220=TerrainProgramValidation220Enabled()?dynamic_cast<const OriginalProgram*>(draw.program.consumer.get()):nullptr;
		if(!validatedProgram220||!validatedProgram220->ReuseIdentityValidation220(draw.program.identity,out.source->contextGeneration))
			ValidateNativeTerrainProgram(draw.program.identity,draw.program.consumer,out.source->contextGeneration);
		ValidateNativeTerrainImages(draw.images,out.source->contextGeneration,out.source->sourceDrawFrame);
		auto original=std::dynamic_pointer_cast<const OriginalProgram>(draw.program.consumer);
		Require(original&&original->factory==impl->factory,"Original terrain draw does not retain this genuine terrain library/PSO owner");
		Require(draw.border==original->profile.border&&draw.composite==original->profile.composite&&draw.deferred==original->profile.MRT(),
			"Original terrain submission selected a different border/forward/deferred program family");
		PreparedTerrainSource::Impl::Draw ready;ready.sequence=draw.sequence;ready.program=original;ready.world=draw.world;
		ready.polygon=PolygonMode(draw.world);
		Require(ready.polygon!=NativePolygonMode::Point,"Original terrain polygon points need their actual native raster implementation");
		Require(ready.polygon!=NativePolygonMode::Line||(draw.world.lineWidth==1&&!draw.world.lineStipple),
			"Original terrain wide/stippled polygon lines require their exact native raster implementation");
		Require(draw.world.colorWrite==draw.surface.colorWrite,"Original terrain captured color-write owners disagree");
		Require(std::isfinite(draw.world.offsetFactor)&&std::isfinite(draw.world.offsetUnits),"Original terrain polygon offset is nonfinite");
		auto camera=target.camera;
		if(draw.surface.viewport){const auto& viewport=*draw.surface.viewport;
			Require(viewport.width<=uint32_t(INT32_MAX)&&viewport.height<=uint32_t(INT32_MAX),"Original terrain viewport exceeds source GLsizei");
			camera.viewport={viewport.x,viewport.y,int32_t(viewport.width),int32_t(viewport.height)};}
		ready.raster=effectstate::Viewport(camera,target.width,target.height,target.logicalWidth,target.logicalHeight,draw.surface.scissor);
		ready.depth=effectstate::Depth(lease,draw.world,target.stencil);
		Uniform uniforms=DrawUniform(draw,original->profile,target);
		const auto& uniformSource=*draw.program.uniforms;
		std::array<std::optional<ImageKind>,NativeCUSTextureHistory::Units> activeUnitTypes{};
		auto resolve=[&](const char* name,ImageKind kind,ImageUniform& parameters,bool vertex){
			const int32_t unit=Int(uniformSource,name,1).x;
			RequireNamedTerrainInput(unit>=0&&uint32_t(unit)<NativeCUSTextureHistory::Units,"Original terrain sampler unit is outside observed source history: ",name);
			auto& unitType=activeUnitTypes[unsigned(unit)];
			Require(!unitType||*unitType==kind,"Original terrain active samplers of different GL types alias one source unit");
			unitType=kind;
			const auto sourceTarget=kind==ImageKind::Cube?NativeCUSTextureTarget::Cube:NativeCUSTextureTarget::Image2D;
			const auto& binding=RequireNativeTerrainImage(draw.images,unsigned(unit),sourceTarget);
			const auto& sampling=binding.object->sampling;
			auto sampler=impl->Sampler(lease,sampling,kind==ImageKind::Shadow);
			auto image=provider(lease,binding,unsigned(unit),sampler);
			RequireNamedTerrainInput(image.texture&&image.sampler==sampler&&image.owner,
				"Original terrain source image lost its exact sampler or retained owner: ",name);
			// Common image providers may establish synchronous upload completion
			// while resolving an owner. Planned producers carry requireReadable;
			// that optional callback is enforced immediately before actual draws.
			RequireNamedTerrainInput(image.texture.textureType==(kind==ImageKind::Cube?MTLTextureTypeCube:MTLTextureType2D),
				"Original terrain source image has the wrong physical texture type: ",name);
			if(kind==ImageKind::Cube)Require(!image.depthImage&&!image.flipV,"Original terrain reflection cube requires its actual color-cube coordinate convention");
			if(kind==ImageKind::Shadow)Require(image.depthImage&&sampling.swizzle==std::array<unsigned,4>{0x1903,0x1904,0x1905,0x1906},
				"Original terrain shadow comparison has unsupported depth storage/swizzle");
			ImageParameters(parameters,sampling,image,vertex);
			lease.AddAllocation(image.texture);image.texture=MipView(lease,*binding.object,image.texture,reuse?&impl->mipViews:nullptr);
			lease.AddAllocation(image.texture);ready.images.push_back(std::move(image));
		};
		const size_t fragmentOffset=original->profile.composite?0:1;
		if(fragmentOffset)resolve("heightMapTex",ImageKind::Color2D,uniforms.heightImage,true);
		SMFSamplerSelection211 sampling211(original->samplerSourceSupported211,original->normalSlot211);
		for(size_t slot=0;slot<original->profile.fragment.size();++slot){
			const auto& abi=original->profile.fragment[slot];
			resolve(abi.name.c_str(),abi.kind,uniforms.images[slot],false);
			// Inspect the exact transport already resolved for this upload. No
			// second sampler walk, owner census, or source-string work per draw.
			if(abi.kind==ImageKind::Color2D)sampling211.ObserveColor(uniforms.images[slot],unsigned(slot));
		}
		// Select the shader ABI from the same resolved images used below for
		// argument bindings. Unselected color/depth resources are not declared.
		uint32_t imageDepthMask=fragmentOffset&&ready.images.front().depthImage?1u:0u;
		for(size_t slot=0;slot<original->profile.fragment.size();++slot)
			if(original->profile.fragment[slot].kind==ImageKind::Color2D&&ready.images[slot+fragmentOffset].depthImage)
				imageDepthMask|=uint32_t(1)<<(slot+1);
		ready.pipeline=original->Pipeline(lease,target,draw.surface,imageDepthMask,validation,milliseconds,draw.world.combinedColorWrite,sampling211.Select());
		const auto uniformSlice=uniformBatch.Upload(&uniforms,sizeof(uniforms));
		ready.uniforms=uniformSlice.buffer;
		auto fd=[MTL4ArgumentTableDescriptor new];fd.maxBufferBindCount=2;fd.maxTextureBindCount=original->profile.fragment.size()*2;
		fd.maxSamplerStateBindCount=original->profile.fragment.size();fd.initializeBindings=YES;
		ready.fragment=reuse?impl->argumentPool.Acquire(lease,fd,out.argumentOwners,nullptr,uint64_t(1)<<1):[lease.Device()newArgumentTableWithDescriptor:fd error:nil];Require(ready.fragment!=nil,"Cannot create compact original terrain fragment argument table");
		lease.Retain(ready.fragment);[ready.fragment setAddress:ready.uniforms.gpuAddress+uniformSlice.offset atIndex:1];
		for(size_t slot=0;slot<original->profile.fragment.size();++slot){const auto& image=ready.images[slot+fragmentOffset];const auto kind=original->profile.fragment[slot].kind;
			const auto textureSlot=slot*2+(kind==ImageKind::Color2D&&image.depthImage?1:0);
			[ready.fragment setTexture:image.texture.gpuResourceID atIndex:textureSlot];[ready.fragment setSamplerState:image.sampler.gpuResourceID atIndex:slot];}
		static_assert(sizeof(std::array<float,3>)==12&&sizeof(std::array<uint8_t,4>)==4);
		ready.indexed=mesh.indexed;ready.count=uint32_t(count);
		const auto geometry=impl->geometryCache.Acquire(lease,{draw.squareX,draw.squareZ,unsigned(draw.mesh),draw.border},mesh,draw.meshSnapshot);
		ready.positions=geometry.positions;
		Require(ready.positions!=nil,"Cannot retain original terrain position stream");lease.AddAllocation(ready.positions);
		if(draw.border){
			ready.colors=geometry.colors;
			Require(ready.colors!=nil,"Cannot retain original border normalized UByte color stream");lease.AddAllocation(ready.colors);
		}
		if(mesh.indexed){
			ready.indices=geometry.indices;
			Require(ready.indices!=nil,"Cannot retain original terrain index order");lease.AddAllocation(ready.indices);
		}
		auto vd=[MTL4ArgumentTableDescriptor new];vd.maxBufferBindCount=draw.border?3:2;vd.maxTextureBindCount=fragmentOffset?2:0;vd.maxSamplerStateBindCount=fragmentOffset?1:0;vd.initializeBindings=YES;
		ready.vertex=reuse?impl->argumentPool.Acquire(lease,vd,out.argumentOwners,nullptr,draw.border?7u:3u):[lease.Device()newArgumentTableWithDescriptor:vd error:nil];Require(ready.vertex!=nil,"Cannot create original terrain vertex argument table");lease.Retain(ready.vertex);
		[ready.vertex setAddress:ready.positions.gpuAddress atIndex:0];[ready.vertex setAddress:ready.uniforms.gpuAddress+uniformSlice.offset atIndex:1];
		if(draw.border)[ready.vertex setAddress:ready.colors.gpuAddress atIndex:2];
		if(fragmentOffset){const auto& height=ready.images.front();[ready.vertex setTexture:height.texture.gpuResourceID atIndex:height.depthImage?1:0];[ready.vertex setSamplerState:height.sampler.gpuResourceID atIndex:0];}
		out.draws.push_back(std::move(ready));
	}
	return prepared;
}
} // namespace rendering::metal4
