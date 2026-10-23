/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativePostprocessData.h"
#include "ScreenDrawList.h"
#include "NativeCUSUniformData.h"
#include "NativeCASProgram.h"

#include <cmath>
#include <cstring>
#include <algorithm>
#include <set>
#include <stdexcept>
#include <utility>
#include <limits>

namespace rendering {
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
using VersionKey = std::pair<std::uint64_t, std::uint64_t>;
VersionKey Key(const NativePostprocessVersion& value)
{
	Check(value.image && value.generation, "Postprocess requires a real immutable image version");
	ValidateNativePostprocessImage(*value.image);
	return {value.image->object, value.generation};
}
void RectInside(const NativePostprocessRect& rect, std::uint32_t width, std::uint32_t height)
{
	Check(rect.x >= 0 && rect.y >= 0 && rect.width && rect.height &&
		std::uint64_t(rect.x) + rect.width <= width && std::uint64_t(rect.y) + rect.height <= height,
		"Postprocess copy rectangle exceeds actual source/destination extent");
}
template<class T> void Finite(const T& values)
{
	for (float value: values) Check(std::isfinite(value), "Postprocess received nonfinite original values");
}
}
std::shared_ptr<NativePostprocessBinding> CreateNativePostprocessBinding(std::uint32_t width, std::uint32_t height,
	NativePostprocessFormat format, NativePostprocessSampler sampler, bool renderToTexture, std::uint32_t maximumDimension)
{
	static std::uint64_t nextObject = 1; // Main-thread local; never reset/reuse.
	Check(maximumDimension && width <= maximumDimension && height <= maximumDimension &&
		nextObject != std::numeric_limits<std::uint64_t>::max(), "Native copied image exceeds actual extent/identity bounds");
	auto image = std::make_shared<NativePostprocessImage>();
	image->object = nextObject; image->width = width; image->height = height;
	image->format = format; image->sampler = sampler; image->renderToTexture = renderToTexture;
	ValidateNativePostprocessImage(*image);
	auto binding = std::make_shared<NativePostprocessBinding>(); binding->image = std::move(image);
	++nextObject; return binding;
}
void ValidateNativePostprocessImage(const NativePostprocessImage& image)
{
	Check(image.object && image.width && image.height && image.width <= 32768 && image.height <= 32768,
		"Postprocess image identity/dimensions are invalid");
	Check(image.format <= NativePostprocessFormat::R8Unorm && image.sampler.minFilter <= NativePostprocessFilter::Linear &&
		image.sampler.magFilter <= NativePostprocessFilter::Linear && image.sampler.wrapS <= NativePostprocessWrap::LegacyClamp &&
		image.sampler.wrapT <= NativePostprocessWrap::LegacyClamp, "Postprocess image format/sampler is unsupported");
	Check(!IsNativePostprocessDepth(image.format) || !image.renderToTexture,
		"Depth image is a real screen-copy destination, not a supported color attachment");
	if(image.originalScreenTarget)Check(image.format==NativePostprocessFormat::RGBA8 && !image.renderToTexture &&
		image.originalScreenTarget->resourceId && image.originalScreenTarget->width==image.width && image.originalScreenTarget->height==image.height,
		"Copied panel storage differs from its original logical FBO image");
}
bool HasNativePostprocessCPUContents(const NativePostprocessVersion& version)
{
	if (!version.sourceImage) return false;
	Check(version.image && version.generation, "CPU TexImage lacks its immutable shared image identity");
	ValidateNativePostprocessImage(*version.image);
	const auto& source = *version.sourceImage;
	Check(version.image->format == NativePostprocessFormat::BC1RGBA && source.format == ScreenTextureFormat::BC1RGBA &&
		source.width == version.image->width && source.height == version.image->height && source.generation == version.generation &&
		source.levelDefinitions && !source.incomplete && !source.renderTarget && !source.postprocessVersion && !source.sourceTexture &&
		!source.generateMipmaps && source.bytes.empty() && !source.bytesPerRow && !source.mips.empty() &&
		!source.mips[0].allocatedUndefined && !source.mips[0].bytes.empty(),
		"Immediate compressed TexImage differs from its owned CPU payload");
	return true;
}
void ValidateNativePostprocessOperation(const NativePostprocessOperation& op, std::uint32_t width, std::uint32_t height)
{
	Check(width && height && op.kind <= NativePostprocessOperationKind::Sepia, "Invalid postprocess frame/operation");
	Check(op.previous.size() == op.outputs.size(), "Postprocess attachment previous-version count differs");
	Check(op.outputs.size() <= 4, "Postprocess supports the original bounded four-output MRT");
	if(op.priorScreen) {
		Check(op.kind==NativePostprocessOperationKind::CopyColor && op.outputs.size()==1 && op.outputs[0] && !op.previous[0],
			"A panel copy must preserve one canonical prior version without a competing previous image");
		const auto& prior=*op.priorScreen;const auto& next=*op.outputs[0];
		Check(next.image && next.image->originalScreenTarget && ScreenTextureTargetIdentity(prior)==next.image->originalScreenTarget &&
			prior.generation && prior.generation<next.generation && prior.format==ScreenTextureFormat::RGBA8Unorm &&
			prior.width==next.image->width && prior.height==next.image->height && prior.flipV && !prior.incomplete &&
			!prior.sourceTexture && !prior.generateMipmaps && prior.bytes.empty() && !prior.bytesPerRow && prior.mips.empty() &&
			bool(prior.renderTarget)!=bool(prior.postprocessVersion),"Panel copy lost its actual prior immutable FBO/copy contents");
	}
	std::set<std::uint64_t> outputObjects;
	for (std::size_t i = 0; i < op.outputs.size(); ++i) {
		const auto& out = op.outputs[i]; Check(out != nullptr, "Missing real postprocess output"); Key(*out);
		Check(!out->sourceImage, "A CPU TexImage version cannot also be a framebuffer producer output");
		Check(outputObjects.insert(out->image->object).second, "Duplicate postprocess MRT attachment");
		if (i) Check(out->image->width == op.outputs[0]->image->width && out->image->height == op.outputs[0]->image->height,
			"Postprocess MRT attachment dimensions differ");
		if (op.previous[i]) {
			Key(*op.previous[i]);
			Check(op.previous[i]->image == out->image && op.previous[i]->generation < out->generation,
				"Postprocess partial update requires an earlier generation of the same actual image");
		}
	}
 if(op.kind==NativePostprocessOperationKind::Sepia) {
	Check(op.stage==NativePostprocessStage::Screen&&!op.sequence&&op.screenOutput&&op.outputs.empty()&&op.viewport,"Sepia requires its original root screen stage");
	const auto& u=op.sepia;Check(u.engine&&u.raster,"Sepia original uniforms/raster missing");ValidateNativeCUSUniformPublication(*u.engine);
	Finite(std::array{u.gamma});for(const auto& row:u.rows)Finite(row);
	Check(op.inputs[0]&&op.inputEnabled[0],"Sepia screen-copy input missing");Key(*op.inputs[0]);
	const auto& image=*op.inputs[0]->image;Check(image.format==NativePostprocessFormat::RGBA8,"Sepia original screen-copy format differs");
	NativePostprocessRect copied{u.viewPos[0],u.viewPos[1],image.width,image.height};RectInside(copied,width,height);
	const auto& v=*op.viewport;Check(v.x>=copied.x&&v.y>=copied.y&&std::int64_t(v.x)+v.width<=std::int64_t(copied.x)+copied.width&&std::int64_t(v.y)+v.height<=std::int64_t(copied.y)+copied.height,"Sepia viewport fetch lies outside actual copied image");
	const auto& surface=u.raster->surface;const auto& world=u.raster->world;
	Check(!surface.blend.enabled&&!surface.attachmentClear&&!world.inheritedClipPlane&&world.frontPolygonMode==NativePolygonMode::Fill&&world.backPolygonMode==NativePolygonMode::Fill,"Sepia original triangle raster differs");
	Check(surface.alphaTest.compare<=ScreenAlphaCompare::Always&&world.cull<=NativeWorldCull::FrontAndBack&&world.depthCompare<=ScreenAlphaCompare::Always,"Sepia raster enum invalid");
	Finite(std::array{surface.alphaTest.reference,world.offsetFactor,world.offsetUnits});
	for(unsigned i=1;i<6;++i)Check(!op.inputs[i]&&!op.inputEnabled[i],"Sepia has an unexpected input");
	return;
 }
 if(op.kind==NativePostprocessOperationKind::CAS) {
  Check(op.stage==NativePostprocessStage::Screen && !op.sequence && op.screenOutput && op.outputs.empty() && op.viewport,"CAS must retain its root screen output and viewport");
  const auto& u=op.cas;Check(u.variant<8 && u.engine && u.raster,"CAS compiled source/engine publication missing");ValidateNativeCUSUniformPublication(*u.engine);
  Finite(std::array{u.strength,u.groundY,u.debugFootprint});
  if(u.variant&4u){
   for(unsigned matrix:{4u,7u,9u}){std::array<float,16> value;std::memcpy(value.data(),u.engine->matrices->bytes.data()+matrix*64,64);Finite(value);}
   std::array<float,4> geometry;std::memcpy(geometry.data(),u.engine->params->bytes.data()+32,16);Finite(geometry);
   float projectionY=0;std::memcpy(&projectionY,u.engine->matrices->bytes.data()+4*64+5*4,4);Check(geometry[1]>0 && projectionY!=0,"CAS footprint has an invalid original projection/view height");
  }
  Check(u.viewRect[0]>=0 && u.viewRect[1]>=0 && u.viewRect[2]>=0 && u.viewRect[3]>=0 && u.viewRect[2]<32768 && u.viewRect[3]<32768,"CAS original viewRect invalid");
  const auto& v=*op.viewport;
  const NativePostprocessRect copiedRect{u.viewRect[0],u.viewRect[1],std::uint32_t(u.viewRect[2]+1),std::uint32_t(u.viewRect[3]+1)};RectInside(copiedRect,width,height);
  Check(v.x>=copiedRect.x && v.y>=copiedRect.y && std::int64_t(v.x)+v.width<=std::int64_t(copiedRect.x)+copiedRect.width && std::int64_t(v.y)+v.height<=std::int64_t(copiedRect.y)+copiedRect.height,"CAS viewport would sample outside the original copied view");
  const auto& surface=u.raster->surface;const auto& world=u.raster->world;
  Check(!surface.attachmentClear && !world.inheritedClipPlane && world.frontPolygonMode==NativePolygonMode::Fill && world.backPolygonMode==NativePolygonMode::Fill,"CAS original full triangle has unsupported raster geometry");
  Check(surface.alphaTest.compare<=ScreenAlphaCompare::Always && surface.blend.colorEquation<=ScreenBlendEquation::Maximum && surface.blend.alphaEquation<=ScreenBlendEquation::Maximum && world.cull<=NativeWorldCull::FrontAndBack && world.depthCompare<=ScreenAlphaCompare::Always,"CAS captured raster state is invalid");
  Finite(std::array{surface.alphaTest.reference,world.offsetFactor,world.offsetUnits});
  Check(op.inputs[0] && op.inputEnabled[0],"CAS requires the real screen-copy manager image");Key(*op.inputs[0]);
  const auto& image=*op.inputs[0]->image;Check(image.format==NativePostprocessFormat::RGBA8 && image.width==copiedRect.width && image.height==copiedRect.height,"CAS screen copy extent/format differs");
  for(unsigned i=1;i<6;++i)Check(!op.inputs[i] && !op.inputEnabled[i],"CAS has an unexpected sampler");
  return;
 }
	if (op.kind != NativePostprocessOperationKind::DOF) {
		Check(!op.viewport && !op.screenOutput && op.outputs.size() == 1, "Screen copy must write one real image");
		Check(op.stage<=NativePostprocessStage::Screen && ((op.stage==NativePostprocessStage::Screen)?op.sequence==0:op.sequence!=0),"Copy stage/actual command sequence is invalid");
		Check((op.kind == NativePostprocessOperationKind::CopyDepth ? IsNativePostprocessDepth(op.outputs[0]->image->format) : op.outputs[0]->image->format == NativePostprocessFormat::RGBA8),
			"Screen copy image format differs from original color/depth source");
		RectInside(op.source, width, height); RectInside(op.destination, op.outputs[0]->image->width, op.outputs[0]->image->height);
		Check(op.source.width == op.destination.width && op.source.height == op.destination.height,
			"Original CopyTexSubImage2D does not rescale copy rectangles");
		for (const auto& in: op.inputs) Check(!in, "Screen copy must read the current framebuffer, not a substitute image");
		return;
	}
	Check(op.stage==NativePostprocessStage::Screen && op.sequence==0,"Original DOF is a screen-stage program");
	const auto& u = op.uniforms;
	Check(u.pass >= 0 && u.pass <= 7 && (u.quality == 0 || u.quality == 1) && (u.pass == 5 || u.quality == 1) &&
		(u.autofocus == 0 || u.autofocus == 1) && (u.mousefocus == 0 || u.mousefocus == 1),
		"Unknown original31657 DOF compiled pass/quality/focus mode");
	Finite(u.projection); Finite(u.resolution); Finite(u.mouseDepthCoordinate);
	Finite(std::array{u.autofocusFudgeFactor,u.autofocusPower,u.autofocusFocalLength,u.manualFocusDepth,u.fStop});
	Finite(op.quad.clip); Finite(op.quad.uv);
	if (u.pass == 0) Check(u.fStop > 0 && u.autofocusFudgeFactor > 0 && u.autofocusFocalLength > 0,
		"Original DOF filter divisor parameters must be positive");
	if (u.pass <= 4 || u.pass == 7) Check(u.resolution[0] > 0 && u.resolution[1] > 0,
		"Original DOF active resolution uniform is missing");
	const std::size_t count = u.pass == 0 ? 2 : u.pass == 1 ? 3 : u.pass == 3 ? 4 : 1;
	Check(op.screenOutput == (u.pass == 5) && (op.screenOutput ? op.outputs.empty() : op.outputs.size() == count),
		"DOF31657 pass output count differs from its original FBO");
	const auto mask = NativeDOFInputMask(u.pass,u.quality);
	for (unsigned i = 0; i < op.inputs.size(); ++i) {
		if (!(mask & (1u << i))) {
			Check(!op.inputs[i] && !op.inputEnabled[i],"DOF captured a sampler not used by its original program");
			continue;
		}
		Check(op.inputEnabled[i] && op.inputs[i],"Original DOF input is not bound to produced image contents"); Key(*op.inputs[i]);
		Check(!outputObjects.contains(op.inputs[i]->image->object),"DOF active attachment feedback is unsupported and invalid");
	}
	Check(op.viewport.has_value(),"DOF draw lost its explicit original viewport");
	const auto& viewport = *op.viewport;
	if (op.screenOutput) RectInside(viewport,width,height);
	else {
		const auto& output = *op.outputs[0]->image;
		Check(viewport.x == 0 && viewport.y == 0 && viewport.width == output.width && viewport.height == output.height,
			"Original DOF offscreen viewport differs from its actual attachment");
	}
	const auto input = [&](unsigned unit) -> const NativePostprocessImage& { return *op.inputs[unit]->image; };
	const auto size = [](const NativePostprocessImage& a,const NativePostprocessImage& b) { return a.width==b.width && a.height==b.height; };
	const auto sampler = [](const NativePostprocessImage& image,bool tile) {
		const auto f=tile?NativePostprocessFilter::Nearest:NativePostprocessFilter::Linear;
		const auto w=tile?NativePostprocessWrap::Repeat:NativePostprocessWrap::ClampEdge;
		Check(image.sampler.minFilter==f && image.sampler.magFilter==f && image.sampler.wrapS==w && image.sampler.wrapT==w,
			"Original DOF image sampling changed");
	};
	for (unsigned i=0;i<op.inputs.size();++i) if(mask&(1u<<i)) {
		const bool depth=u.pass==0 && i==1;
		const bool tile=i==5 || u.pass==7;
		if (!depth) sampler(input(i),tile);
	}
	for (std::size_t i=0;i<op.outputs.size();++i) {
		const auto format=u.pass==0?(i==0?NativePostprocessFormat::RGBA16Float:NativePostprocessFormat::RG8):
			(u.pass==1 || u.pass==3)?NativePostprocessFormat::RGBA16Float:NativePostprocessFormat::RGBA8;
		Check(op.outputs[i]->image->format==format,"Original DOF attachment format/quantization changed");
		sampler(*op.outputs[i]->image,u.pass>=6);
	}
	if (u.pass == 0) {
		const auto& depth=input(1);
		Check(input(0).format==NativePostprocessFormat::RGBA8 && depth.format==NativePostprocessFormat::Depth32 && size(input(0),depth) &&
			depth.sampler.minFilter==NativePostprocessFilter::Nearest && depth.sampler.magFilter==NativePostprocessFilter::Nearest &&
			depth.sampler.wrapS==NativePostprocessWrap::Repeat && depth.sampler.wrapT==NativePostprocessWrap::Repeat,
			"Original DOF depth requires genuine same-size NEAREST/REPEAT depth contents");
		Check(op.outputs[0]->image->width==input(0).width/2 && op.outputs[0]->image->height==input(0).height/2 &&
			u.resolution==std::array<float,2>{input(0).width/2.f,input(0).height/2.f},
			"DOF filter must retain truncated allocation and fractional original resolution separately");
	} else if (u.pass >= 1 && u.pass <= 4) {
		const auto& base=input(0); const auto& tile=input(5);
		Check(base.format==NativePostprocessFormat::RGBA16Float && size(base,*op.outputs[0]->image) &&
			tile.format==NativePostprocessFormat::RGBA8 && tile.width==(base.width+15)/16 && tile.height==(base.height+15)/16 &&
			std::floor(u.resolution[0])==base.width && std::floor(u.resolution[1])==base.height,
			"DOF blur requires its actual half-size HDR base and tile mask");
		for (unsigned i=1;i<5;++i) if(mask&(1u<<i)) Check(input(i).format==NativePostprocessFormat::RGBA16Float && size(input(i),base),
			"DOF complex intermediate dimensions/format changed");
	} else if (u.pass == 5) {
		Check(input(0).format==NativePostprocessFormat::RGBA8 && input(1).format==NativePostprocessFormat::RGBA8 &&
			viewport.width==input(0).width && viewport.height==input(0).height && input(1).width==input(0).width/2 && input(1).height==input(0).height/2,
			"DOF composition requires its actual source and far result");
		if (u.quality) Check(input(2).format==NativePostprocessFormat::RGBA8 && size(input(1),input(2)),"DOF near composition image changed");
	} else if (u.pass == 6) {
		Check(input(0).format==NativePostprocessFormat::RG8 && op.outputs[0]->image->width==(input(0).width+15)/16 &&
			op.outputs[0]->image->height==(input(0).height+15)/16,"DOF tile flags require the real per-pixel RG8 flags");
	} else {
		Check(input(0).format==NativePostprocessFormat::RGBA8 && size(input(0),*op.outputs[0]->image) &&
			input(0).width==(std::uint32_t(std::floor(u.resolution[0]))+15)/16 &&
			input(0).height==(std::uint32_t(std::floor(u.resolution[1]))+15)/16,"DOF tile expansion dimensions changed");
	}
	if(u.pass!=2 && u.pass!=4) Check(op.quad.clip==std::array<float,4>{-1,-1,1,1} && op.quad.uv==std::array<float,4>{0,0,1,1},
		"DOF pixel-space pass lost its actual screen-basis TexRect");
	else {
		// Lua computes the half-pixel coordinates in double before GL float
		// conversion; tolerate only that final-rounding difference here.
		const std::array<float,4> expected{-1-.25f/u.resolution[0],1+.25f/u.resolution[1],1+.25f/u.resolution[0],-1-.25f/u.resolution[1]};
		for(unsigned i=0;i<4;++i) Check(std::abs(op.quad.clip[i]-expected[i])<=2*std::numeric_limits<float>::epsilon(),
			"DOF final blur lost its direct clip-space half-pixel oversize quad");
		Check(op.quad.uv==std::array<float,4>{0,1,1,0},"DOF final blur TexRect orientation changed");
	}

}

void ValidateNativeExternalImageProducer(const NativeExternalImageProducer& producer)
{
	const bool cloud=producer.family==NativeExternalImageFamily::VolumetricCloud;
 const bool sensor=producer.family==NativeExternalImageFamily::SensorPreview;
 const bool blockout=producer.family==NativeExternalImageFamily::MapBlockoutDepth;
 Check(producer.sequence&&((cloud&&producer.stage==NativePostprocessStage::PreUnit)||
  (sensor&&producer.stage==NativePostprocessStage::World)||
  (blockout&&producer.stage==NativePostprocessStage::PreUnit)),"External producer must identify its actual supported family/stage/order");
	Check(producer.outputs.size()==1 && producer.previous.size()==1 && producer.inputs.size()<=16,"External producer attachment/input extent invalid");
	const auto& output=producer.outputs[0];Check(output!=nullptr,"Missing external producer image");Key(*output);
	Check(!output->sourceImage,"A CPU TexImage version cannot also be an external producer output");
	Check(blockout?(!output->image->renderToTexture&&output->image->format==NativePostprocessFormat::Depth24Unorm):
	 (output->image->renderToTexture&&
  (cloud?output->image->format==NativePostprocessFormat::RGBA8:
   output->image->format==NativePostprocessFormat::R16Float||output->image->format==NativePostprocessFormat::R32Float||output->image->format==NativePostprocessFormat::R8Unorm)),
  "Original external output has another actual family target format");
	if(producer.previous[0]){const auto& previous=*producer.previous[0];Key(previous);Check(previous.image==output->image && previous.generation<output->generation,"External partial update requires the earlier actual image generation");}
	for(const auto& input:producer.inputs){Check(input!=nullptr,"Missing external input image");Key(*input);Check(input->image->object!=output->image->object,"External active-target feedback is unsupported");}
}
std::vector<NativePostprocessCommandRef> OrderNativePostprocessCommands(const NativePostprocessFrame& frame)
{
	Check(frame.operations.size()+frame.externalOutputs.size()<=4096,"Shared image producer graph exceeds bounded contract");
	std::vector<NativePostprocessCommandRef> order;order.reserve(frame.operations.size()+frame.externalOutputs.size());
	NativePostprocessStage previousStage=NativePostprocessStage::PreDecals;std::uint64_t previousSequence=0;std::uint32_t previousOffset=0;
	for(std::size_t i=0;i<frame.operations.size();++i){const auto& op=frame.operations[i];ValidateNativePostprocessOperation(op,frame.logicalWidth,frame.logicalHeight);
  if(op.kind==NativePostprocessOperationKind::CAS)Check(op.cas.engine->matrices->sourceDrawFrame<=frame.sourceDrawFrame && op.cas.engine->params->sourceDrawFrame<=frame.sourceDrawFrame,"CAS engine publication is from a future frame");
  if(op.kind==NativePostprocessOperationKind::Sepia)Check(op.sepia.engine->matrices->sourceDrawFrame<=frame.sourceDrawFrame&&op.sepia.engine->params->sourceDrawFrame<=frame.sourceDrawFrame,"Sepia publication is from a future frame");
		Check(op.stage>=previousStage,"Copy/effect stage moved backwards");
		if(op.stage==previousStage){if(op.stage==NativePostprocessStage::Screen)Check(op.screenCommandOffset>=previousOffset,"Original screen insertion moved backwards");else Check(op.sequence>previousSequence,"Original world copy order is not strictly increasing");}
		previousStage=op.stage;previousSequence=op.sequence;previousOffset=op.screenCommandOffset;order.push_back({false,i});}
	for(std::size_t i=0;i<frame.externalOutputs.size();++i){ValidateNativeExternalImageProducer(frame.externalOutputs[i]);order.push_back({true,i});}
	auto stage=[&](const auto& ref){return ref.external?frame.externalOutputs[ref.index].stage:frame.operations[ref.index].stage;};
	auto sequence=[&](const auto& ref){return ref.external?frame.externalOutputs[ref.index].sequence:frame.operations[ref.index].sequence;};
	std::stable_sort(order.begin(),order.end(),[&](const auto& a,const auto& b){if(stage(a)!=stage(b))return stage(a)<stage(b);return stage(a)==NativePostprocessStage::Screen?false:sequence(a)<sequence(b);});
	for(std::size_t i=1;i<order.size();++i)if(stage(order[i])!=NativePostprocessStage::Screen && stage(order[i-1])==stage(order[i]))Check(sequence(order[i-1])!=sequence(order[i]),"Two source producers share a world command sequence");
	return order;
}
void ValidateNativePostprocessFrame(const NativePostprocessFrame& frame, bool (*isRetired)(const NativePostprocessVersion&, void*), void* state)
{
	Check(frame.serial && frame.logicalWidth && frame.logicalHeight,"Postprocess frame extent/serial invalid");
	std::set<VersionKey> produced;
	auto available=[&](const std::shared_ptr<const NativePostprocessVersion>& version){if(version)Check(produced.contains(Key(*version)) || HasNativePostprocessCPUContents(*version) || (isRetired && isRetired(*version,state)),"Postprocess references missing/unretired image contents");};
	for(const auto& ref:OrderNativePostprocessCommands(frame)){
		if(ref.external){const auto& producer=frame.externalOutputs[ref.index];for(const auto& in:producer.inputs)available(in);for(const auto& previous:producer.previous)available(previous);for(const auto& out:producer.outputs)Check(produced.insert(Key(*out)).second,"Image generation has multiple actual producers");}
		else{const auto& op=frame.operations[ref.index];for(const auto& in:op.inputs)available(in);for(const auto& previous:op.previous)available(previous);
			// An RTT prior is structurally validated above and resolved against the
			// actual ScreenGPU ordered producer plus its encode-time readable flag.
			// It is never passed to isRetired as a fabricated postprocess version.
			if(op.priorScreen && op.priorScreen->postprocessVersion)available(op.priorScreen->postprocessVersion);
			for(const auto& out:op.outputs)Check(produced.insert(Key(*out)).second,"Image generation has multiple actual producers");}
	}
}
void ValidateNativePostprocessConsumer(const NativePostprocessFrame& frame,const NativePostprocessVersion& version,
	const std::optional<NativePostprocessConsumer>& consumer,bool (*isRetired)(const NativePostprocessVersion&,void*),void* state)
{
	const auto key=Key(version);
	if(HasNativePostprocessCPUContents(version))return; // Original immediate upload, not GPU retirement.
	if(isRetired && isRetired(version,state))return;
	Check(consumer.has_value(),"Current source-frame image cannot be sampled by an unordered cached-panel producer");
	Check(consumer->stage<=NativePostprocessStage::Screen && (consumer->stage==NativePostprocessStage::Screen?consumer->sequence==0:consumer->sequence!=0),"Image consumer stage/sequence invalid");
	for(const auto& ref:OrderNativePostprocessCommands(frame)){
		const auto& outputs=ref.external?frame.externalOutputs[ref.index].outputs:frame.operations[ref.index].outputs;
		if(std::none_of(outputs.begin(),outputs.end(),[&](const auto& out){return Key(*out)==key;}))continue;
		const auto stage=ref.external?frame.externalOutputs[ref.index].stage:frame.operations[ref.index].stage;
		const auto sequence=ref.external?frame.externalOutputs[ref.index].sequence:frame.operations[ref.index].sequence;
		Check(stage<=consumer->stage,"Native image consumer precedes its actual source stage");
		if(stage==consumer->stage){
			if(stage==NativePostprocessStage::Screen){
				Check(!ref.external && frame.operations[ref.index].screenCommandOffset<=consumer->screenCommandOffset,"Screen sample precedes its exact original copy insertion");
				if(frame.operations[ref.index].screenCommandOffset==consumer->screenCommandOffset && consumer->commandSequence)
					Check(frame.operations[ref.index].commandSequence<consumer->commandSequence,"Target sample precedes its same-offset original copy insertion");
			}
			else Check(sequence<consumer->sequence,"World sample precedes its exact source command");
		}
		return;
	}
	throw std::runtime_error("Image consumer has no actual declared/retired producer");
}
} // namespace rendering
