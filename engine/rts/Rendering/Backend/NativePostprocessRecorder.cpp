/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "NativePostprocessRecorder.h"
#include "NativeWorldDrawSequence.h"
#include "NativeCUSUniformData.h"
#include <limits>
#include <algorithm>
#include <exception>
#include <stdexcept>

namespace rendering {
namespace { void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); } }
void NativePostprocessRecorder::Rollback() noexcept
{
	for (auto i = changes.rbegin(); i != changes.rend(); ++i) {
  if(i->actualSubmission)continue; // A real GPU submission is not reversible.
  if(i->publication)i->publication->Cancel();
		if (i->binding->current == i->published) {
			i->binding->current = i->previous;
		}
	}
	std::erase_if(changes,[](const auto& change){return !change.actualSubmission;});
}
void NativePostprocessRecorder::BeginFrame(std::uint32_t width, std::uint32_t height, std::uint64_t sourceDrawFrame)
{
	Check(!active && width && height && nextSerial != std::numeric_limits<std::uint64_t>::max(), "Invalid/nested postprocess frame");
	Check(std::none_of(changes.begin(),changes.end(),[](const auto& change){return change.actualSubmission!=0;}),
		"Postprocess submitted publications require actual retirement or failed-context teardown");
	// A hidden/failed frame has no matching actual color/depth source. Retain
	// its immutable packet for any current reader, but undo unpublished cells.
	Rollback(); pending.reset();
	auto frame = std::make_shared<NativePostprocessFrame>(); frame->serial = nextSerial++; frame->logicalWidth = width; frame->logicalHeight = height; frame->sourceDrawFrame=sourceDrawFrame;
	recording = std::move(frame); active = true;
}
std::shared_ptr<const NativePostprocessFrame> NativePostprocessRecorder::EndFrame()
{
	Check(active && recording, "Postprocess EndFrame outside active frame");
	pending = recording; recording.reset(); active = false; return pending;
}
std::shared_ptr<const NativePostprocessFrame> NativePostprocessRecorder::SnapshotPrefix(std::uint64_t cutoff) const
{
	Check(active && recording && cutoff,"Postprocess prefix requires its active source frame and actual cutoff");
	auto prefix=std::make_shared<NativePostprocessFrame>(*recording);
	std::erase_if(prefix->operations,[&](const auto& op){return op.commandSequence>=cutoff;});
	std::erase_if(prefix->externalOutputs,[&](const auto& producer){return producer.sequence>=cutoff;});
	return prefix;
}
void NativePostprocessRecorder::AbortFrame() noexcept { Rollback(); recording.reset(); pending.reset(); active = false; }
void NativePostprocessRecorder::RetireCompletedFrame(std::uint64_t serial)
{
	Check(!active && pending && pending->serial == serial, "Postprocess completion does not match pending actual frame");
	changes.clear(); pending.reset();
}
void NativePostprocessRecorder::RecordSubmission(const NativePostprocessFrame& selected,std::uint64_t actualSubmission) noexcept
{
	const auto& owned=active?recording:pending;
	if(!actualSubmission || !owned || selected.serial!=owned->serial ||
		selected.sourceDrawFrame!=owned->sourceDrawFrame)std::terminate();
	const auto mark=[&](const auto& version){
		for(auto& change:changes)if(change.published==version){
			if(change.actualSubmission && change.actualSubmission!=actualSubmission)std::terminate();
			change.actualSubmission=actualSubmission;
		}
	};
	for(const auto& operation:selected.operations)for(const auto& output:operation.outputs)mark(output);
	for(const auto& producer:selected.externalOutputs)for(const auto& output:producer.outputs)mark(output);
}
void NativePostprocessRecorder::RetireSubmission(std::uint64_t actualSubmission)
{
	Check(actualSubmission!=0,"Postprocess prefix retirement has no actual completed submission");
	// Keep commands/serial until the final source EndFrame. The transaction's
	// consumed ordinal selects the suffix; removing history here would change
	// original offsets and hide cross-batch input validation.
	std::erase_if(changes,[&](const auto& change){return change.actualSubmission==actualSubmission;});
}
void NativePostprocessRecorder::Reset() noexcept { AbortFrame(); }
bool NativePostprocessRecorder::OwnsPendingVersion(const std::shared_ptr<const NativePostprocessVersion>& version) const noexcept
{
	for(const auto& change:changes)if(change.published==version)return true;return false;
}
void NativePostprocessRecorder::Commit(NativePostprocessOperation operation, std::span<const std::shared_ptr<NativePostprocessBinding>> outputs,
	std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitScreen)
{
	Check(active && recording && recording->operations.size()+recording->externalOutputs.size() < 4096, "Postprocess draw outside active/bounded frame");
	if(!recording->operations.empty()){const auto& previous=recording->operations.back();Check(operation.stage>=previous.stage,"Postprocess source stage precedes an earlier operation");if(operation.stage==previous.stage){if(operation.stage==NativePostprocessStage::Screen)Check(operation.screenCommandOffset>=previous.screenCommandOffset,"Postprocess operation precedes an earlier original screen command");else Check(operation.sequence>previous.sequence,"Postprocess world sequence is not increasing");}}
	std::vector<Change> proposed; proposed.reserve(outputs.size());
	for (const auto& binding: outputs) {
		Check(binding && !binding->deleted && binding->image && binding->nextGeneration && binding->nextGeneration != std::numeric_limits<std::uint64_t>::max(),
			"Postprocess writes deleted/missing/exhausted image");
		auto next = std::make_shared<NativePostprocessVersion>(); next->image = binding->image; next->generation = binding->nextGeneration;
		operation.outputs.push_back(next); operation.previous.push_back(commitScreen?nullptr:binding->current);
		proposed.push_back({binding, binding->current, std::move(next),{}});
	}
	ValidateNativePostprocessOperation(operation, recording->logicalWidth, recording->logicalHeight);
	// All allocations/validation precede mutation of a live resource cell.
	changes.reserve(changes.size() + proposed.size()); recording->operations.reserve(recording->operations.size() + 1);
	try {
  if(!commitScreen && publicationObserver)for(auto& change:proposed)
   change.publication=publicationObserver(change.binding,change.published,recording->sourceDrawFrame);
  operation.commandSequence=NextNativeWorldDrawSequence();
  if(commitScreen) {
   Check(proposed.size()==1 && proposed[0].published->image->originalScreenTarget,"Panel copy publication requires its original UI cell");
   // UI owns all copy/RTT cell and registry receipts in one LIFO. This
   // recorder owns only GPU versions, so abort order cannot resurrect a copy.
   commitScreen(proposed[0].published);
  }
 } catch(...) {
  for(auto i=proposed.rbegin();i!=proposed.rend();++i)if(i->publication)i->publication->Cancel();
  throw;
 }
	recording->operations.push_back(std::move(operation));
	for (auto& change: proposed) {
		change.binding->current = change.published; ++change.binding->nextGeneration;
		changes.push_back(std::move(change));
	}
}
std::shared_ptr<const NativePostprocessVersion> NativePostprocessRecorder::Copy(std::shared_ptr<NativePostprocessBinding> destination,
	NativePostprocessOperationKind kind, NativePostprocessRect source, NativePostprocessRect offset, std::uint32_t commandOffset, NativePostprocessStage stage, std::uint64_t sequence,
	std::shared_ptr<const ScreenTexture> priorScreen,std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitScreen)
{
	Check(kind == NativePostprocessOperationKind::CopyColor || kind == NativePostprocessOperationKind::CopyDepth, "Copy requires real current screen source");
	NativePostprocessOperation op; op.kind = kind; op.source = source; op.destination = offset; op.screenCommandOffset = commandOffset; op.stage=stage;op.sequence=sequence;
	op.priorScreen=std::move(priorScreen);
	const std::array outputs{destination}; Commit(std::move(op), outputs,std::move(commitScreen)); return destination->current;
}

std::shared_ptr<const NativePostprocessVersion> NativePostprocessRecorder::PublishExternalOutput(
	std::shared_ptr<NativePostprocessBinding> destination,NativePostprocessStage stage,std::uint64_t sequence,NativeExternalImageFamily family,
	std::span<const std::shared_ptr<const NativePostprocessVersion>> inputs,
	std::function<void(std::shared_ptr<const NativePostprocessVersion>)> commitActualProducer)
{
	Check(active && recording && recording->operations.size()+recording->externalOutputs.size()<4096,"External image declaration outside active/bounded frame");
	Check(destination && !destination->deleted && destination->image && destination->nextGeneration && destination->nextGeneration!=std::numeric_limits<std::uint64_t>::max(),"External target image missing/deleted/exhausted");
	Check(bool(commitActualProducer),"External output requires an actual producer append transaction");
	auto next=std::make_shared<NativePostprocessVersion>();next->image=destination->image;next->generation=destination->nextGeneration;
	NativeExternalImageProducer producer;producer.family=family;producer.stage=stage;producer.sequence=sequence;producer.outputs.push_back(next);producer.previous.push_back(destination->current);producer.inputs.assign(inputs.begin(),inputs.end());
	ValidateNativeExternalImageProducer(producer);
	changes.reserve(changes.size()+1);recording->externalOutputs.reserve(recording->externalOutputs.size()+1);
	Change change{destination,destination->current,next,{}};
	try {
  if(publicationObserver)change.publication=publicationObserver(destination,next,recording->sourceDrawFrame);
  commitActualProducer(next);
 } catch(...) {if(change.publication)change.publication->Cancel();throw;}
 // Source publication and the actual family append are now one transaction.
	recording->externalOutputs.push_back(std::move(producer));
	destination->current=next;++destination->nextGeneration;changes.push_back(std::move(change));return next;
}
void NativePostprocessRecorder::Draw(std::span<const std::shared_ptr<NativePostprocessBinding>> destinations,
	std::array<std::shared_ptr<const NativePostprocessVersion>, 6> inputs, std::array<bool, 6> enabled,
	NativeDOFUniforms uniforms, NativePostprocessQuad quad, NativePostprocessRect viewport, std::uint32_t commandOffset, bool screenOutput)
{
	NativePostprocessOperation op; op.kind = NativePostprocessOperationKind::DOF; op.screenCommandOffset = commandOffset;
	op.inputs = std::move(inputs); op.inputEnabled = enabled; op.uniforms = uniforms; op.quad = quad; op.viewport = viewport; op.screenOutput = screenOutput;
	Commit(std::move(op), destinations);
}
void NativePostprocessRecorder::DrawSepia(std::shared_ptr<const NativePostprocessVersion> input,NativeSepiaUniforms uniforms,
 NativePostprocessRect viewport,std::uint32_t offset)
{
	Check(recording&&uniforms.engine&&uniforms.engine->matrices&&uniforms.engine->params&&
		uniforms.engine->matrices->sourceDrawFrame<=recording->sourceDrawFrame&&uniforms.engine->params->sourceDrawFrame<=recording->sourceDrawFrame,
		"Sepia engine publication does not belong to the current frame");
	NativePostprocessOperation op;op.kind=NativePostprocessOperationKind::Sepia;op.screenOutput=true;op.screenCommandOffset=offset;
	op.inputs[0]=std::move(input);op.inputEnabled[0]=true;op.sepia=std::move(uniforms);op.viewport=viewport;Commit(std::move(op),{});
}
void NativePostprocessRecorder::DrawCAS(std::shared_ptr<const NativePostprocessVersion> input,NativeCASUniforms uniforms,
 NativePostprocessRect viewport,std::uint32_t offset)
{
 Check(recording && uniforms.engine && uniforms.engine->matrices && uniforms.engine->params &&
  uniforms.engine->matrices->sourceDrawFrame<=recording->sourceDrawFrame && uniforms.engine->params->sourceDrawFrame<=recording->sourceDrawFrame,
  "CAS engine uniforms do not belong to this actual draw frame");
 NativePostprocessOperation op;op.kind=NativePostprocessOperationKind::CAS;op.screenOutput=true;op.screenCommandOffset=offset;
 op.inputs[0]=std::move(input);op.inputEnabled[0]=true;op.cas=std::move(uniforms);op.viewport=viewport;Commit(std::move(op),{});
}
} // namespace rendering
