/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
// Included only by Metal4CUSSource.mm's anonymous namespace. Opt-in CPU logs;
// no GPU readback, capture, setting change or gameplay automation.
using TraceClock = std::chrono::steady_clock;
struct ModelTraceState {
	bool enabled = [] { const char* v = std::getenv("BAR_NATIVE_MODEL_TRACE"); return v && std::strcmp(v, "1") == 0; }();
	bool window = false;
	ModelTraceBudget budget;
	uint64_t allocations = 0, allocationBytes = 0, cacheHits = 0, publishedUploads = 0, publishedBytes = 0;
};
ModelTraceState& ModelTrace() { static ModelTraceState state; return state; }
double TraceUS(TraceClock::time_point start) { return std::chrono::duration<double, std::micro>(TraceClock::now() - start).count(); }
uint64_t TraceHash(const void* bytes, size_t count, uint64_t h = 14695981039346656037ull) {
	const auto* p = static_cast<const uint8_t*>(bytes);
	for (size_t n = 0; n < count; ++n) { h ^= p[n]; h *= 1099511628211ull; }
	return h;
}
void TraceRecord(const char* format, ...) {
	auto& t = ModelTrace();
	if (!t.window || t.budget.stopped) return;
	char buffer[1536]; va_list args; va_start(args, format);
	const int size = std::vsnprintf(buffer, sizeof(buffer), format, args); va_end(args);
	if (size < 0) return;
	const size_t length = std::min(size_t(size), sizeof(buffer) - 1);
	// Include a conservative allowance for the engine's line prefix.
	if (t.budget.TakeRecord(length + 160)) LOG("[ModelTrace24] %s", buffer);
}
bool TraceFocus(const NativeCUSModelSubmission& s) {
	if (s.object.kind != NativeCUSObjectKind::Unit) return false;
	const auto& n = s.geometry->sourceName;
	for (const char* part : {"corsolar", "corcom", "corwin", "cormex", "armsolar", "armcom", "armwin", "armmex"})
		if (n.find(part) != std::string::npos) return true;
	return false;
}
bool TraceFrame(const NativeCUSSourceFrame& source) {
	auto& t = ModelTrace();
	if (!t.enabled || t.budget.stopped) return false;
	const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(TraceClock::now().time_since_epoch()).count();
	bool eligible = false;
	if (!t.budget.samples || uint64_t(now) >= t.budget.next) {
		for (const auto& d : source.draws) for (const auto& s : d.submissions)
			if (s && s->geometry && TraceFocus(*s)) { eligible = true; break; }
	}
	return t.budget.Select(source.sourceDrawFrame, now, eligible);
}
struct TraceImage { uint64_t texture = 0, sampler = 0; unsigned format = 0, width = 0, height = 0, levels = 0; };
void TraceDraw(const NativeCUSSourceFrame& frame, const NativeCUSSourceDraw& d,
	const CUSSourceTarget& target, const NativeWorldDrawState& state,
	const NativeCUSSampleTransport& transport, const std::array<TraceImage, 12>& images,
	const NativeCUSRenderReadSet& reads, id<MTLBuffer> instances, id<MTLBuffer> transforms,
	id<MTLBuffer> models, const std::vector<uint8_t>& vertex, const std::vector<uint8_t>& fragment) {
	auto& t = ModelTrace();
	if (!t.window || std::none_of(d.submissions.begin(), d.submissions.end(), [](const auto& s) { return TraceFocus(*s); }) || !t.budget.TakeDraw()) return;
	const auto f = static_cast<unsigned long long>(frame.sourceDrawFrame), seq = static_cast<unsigned long long>(d.sequence);
	TraceRecord("draw f=%llu seq=%llu family=%s pass=%u shader=%.16s/%.16s objects=%zu target=%ux%u samples=%u colors=%lu/%lu/%lu/%lu/%lu depthFormat=%lu stencilFormat=%lu depth=%d/%d/%u cull=%d/%u coverage=%d clamp=%d bias=%d/%.6g/%.6g stencil=%d clip=%u masks=%u/%u/%u/%u colorMask=%d%d%d%d blend=%d/%u/%u",
		f, seq, d.program->family.c_str(), d.program->originalPass, d.program->vertexSHA512.c_str(), d.program->fragmentSHA512.c_str(), d.submissions.size(), target.width, target.height, target.samples,
		(unsigned long)target.colors[0], (unsigned long)target.colors[1], (unsigned long)target.colors[2], (unsigned long)target.colors[3], (unsigned long)target.colors[4], (unsigned long)target.depth, (unsigned long)target.stencil,
		state.depthTest, state.depthWrite, unsigned(state.depthCompare), state.cullEnabled, unsigned(state.cull), state.alphaToCoverage, state.depthClamp, state.polygonOffset, state.offsetFactor, state.offsetUnits, state.stencilTest, transport.clipFlags,
		transport.emptyMask, transport.flipMask, transport.depthMask, transport.clampDepthCompareMask, state.colorWrite[0], state.colorWrite[1], state.colorWrite[2], state.colorWrite[3], d.surface.blend.enabled, unsigned(d.surface.blend.sourceColor), unsigned(d.surface.blend.destinationColor));
	for (unsigned slot = 0; slot < 12; ++slot) {
		const auto& b = d.textures->samplers[slot]; const auto* o = b.object.get(); const auto& im = images[slot];
		if (!im.texture && !(transport.emptyMask & (1u << slot))) continue;
		const auto& sampling = Sampling(b);
		TraceRecord("tex f=%llu seq=%llu slot=%u knowledge=%u object=%llu/%llu/%llu source=%.128s gpu=%llu sampler=%llu format=%u size=%ux%u mips=%u code=%u filters=%u/%u wrap=%u/%u/%u lod=%.6g/%.6g/%.6g aniso=%.6g levels=%d/%d swizzle=%u/%u/%u/%u",
			f, seq, slot, unsigned(b.knowledge), (unsigned long long)(o ? o->contextGeneration : 0), (unsigned long long)(o ? o->identity : 0), (unsigned long long)(o ? o->version : 0), o ? o->source.c_str() : "default",
			(unsigned long long)im.texture, (unsigned long long)im.sampler, im.format, im.width, im.height, im.levels, transport.sampleCodes[slot], sampling.minFilter, sampling.magFilter, sampling.wrapS, sampling.wrapT, sampling.wrapR,
			sampling.minLod, sampling.maxLod, sampling.lodBias, sampling.anisotropy, sampling.baseLevel, sampling.maxLevel, sampling.swizzle[0], sampling.swizzle[1], sampling.swizzle[2], sampling.swizzle[3]);
	}
	const auto& input = *d.modelInputs;
	uint64_t transformHash = 14695981039346656037ull, uniformHash = transformHash;
	size_t transformMismatch = 0, uniformMismatch = 0, checkedTransforms = 0, checkedUniforms = 0;
	for (const auto row : reads.transforms) {
		if (checkedTransforms == 4096) break;
		const auto bytes = ReadNativeCUSTransform(input, row);
		transformHash = TraceHash(&row, sizeof(row), transformHash); transformHash = TraceHash(bytes.data(), bytes.size(), transformHash);
		transformMismatch += std::memcmp(bytes.data(), static_cast<const uint8_t*>(transforms.contents) + size_t(row) * 32, 32) != 0; ++checkedTransforms;
	}
	for (const auto row : reads.modelUniforms) {
		if (checkedUniforms == 512) break;
		std::array<uint8_t, 128> bytes; ReadNativeCUSPublishedElement(*input.modelUniforms, row, bytes);
		uniformHash = TraceHash(&row, sizeof(row), uniformHash); uniformHash = TraceHash(bytes.data(), bytes.size(), uniformHash);
		uniformMismatch += std::memcmp(bytes.data(), static_cast<const uint8_t*>(models.contents) + size_t(row) * 128, 128) != 0; ++checkedUniforms;
	}
	TraceRecord("data f=%llu seq=%llu frames=texture:%llu,input:%llu,matrix:%llu,param:%llu,transform:%llu,model:%llu publications=%llu/%llu owners=%p/%p buffers=%llu/%llu rows=%zu/%zu,%zu/%zu uploadMismatch=%zu/%zu hashes=%016llx/%016llx packed=%016llx/%016llx",
		f, seq, (unsigned long long)d.textures->sourceDrawFrame, (unsigned long long)input.sourceDrawFrame, (unsigned long long)d.engineUniforms->matrices->sourceDrawFrame, (unsigned long long)d.engineUniforms->params->sourceDrawFrame, (unsigned long long)input.transforms->publishedDrawFrame, (unsigned long long)input.modelUniforms->publishedDrawFrame,
		(unsigned long long)input.transforms->publicationSerial, (unsigned long long)input.modelUniforms->publicationSerial, input.transforms.get(), input.modelUniforms.get(), (unsigned long long)transforms.gpuAddress, (unsigned long long)models.gpuAddress,
		checkedTransforms, reads.transforms.size(), checkedUniforms, reads.modelUniforms.size(), transformMismatch, uniformMismatch, (unsigned long long)transformHash, (unsigned long long)uniformHash, (unsigned long long)TraceHash(vertex.data(), vertex.size()), (unsigned long long)TraceHash(fragment.data(), fragment.size()));
	size_t selected = 0;
	for (size_t object = 0; object < d.submissions.size() && selected < 8; ++object) {
		const auto& sub = *d.submissions[object]; if (!TraceFocus(sub)) continue; ++selected;
		const auto instance = ReadNativeCUSInstance(input, object); const auto uniform = ReadNativeCUSModelUniform(input, object);
		const bool instanceMatches = std::memcmp(instance.data(), static_cast<const uint8_t*>(instances.contents) + object * 16, 16) == 0;
		std::array<uint32_t, 32> words; std::memcpy(words.data(), uniform.data(), uniform.size());
		char uniformText[257]; for (size_t i = 0; i < words.size(); ++i) std::snprintf(uniformText + i * 8, 9, "%08x", words[i]);
		TraceRecord("object f=%llu seq=%llu ordinal=%zu id=%d model=%.96s modelId=%d indices=%u pieces=%zu instance=%u/%u/%u/%u instanceMatches=%d uniformWords=%s",
			f, seq, object, sub.object.id, sub.geometry->sourceName.c_str(), sub.geometry->engineModelId, sub.originalIndexCount, sub.geometry->pieces.size(), instance[0], instance[1], instance[2], instance[3], instanceMatches, uniformText);
		// For rigid Solar pieces, preserve both original interpolation rows. Other
		// models are covered by the validated read-set fingerprint above.
		if (sub.geometry->sourceName.find("solar") == std::string::npos) continue;
		size_t pieceIndex = 0;
		for (const auto& piece : sub.geometry->pieces) {
			if (pieceIndex >= 16) break;
			const size_t p = pieceIndex++; if (piece.indices.empty()) continue;
			const auto& v = piece.vertices.at(piece.indices[0]); const uint32_t row = instance[0] + 2u * (1u + (v.boneIds[0] & 255u));
			const auto a = ReadNativeCUSTransform(input, row), b = ReadNativeCUSTransform(input, row + 1); std::array<float, 8> x, y; std::memcpy(x.data(), a.data(), 32); std::memcpy(y.data(), b.data(), 32);
			TraceRecord("solar f=%llu seq=%llu id=%d piece=%zu/%.48s firstBone=%u row=%u a=%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g b=%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g",
				f, seq, sub.object.id, p, piece.name.c_str(), v.boneIds[0] & 255u, row, x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], y[0], y[1], y[2], y[3], y[4], y[5], y[6], y[7]);
		}
	}
}
