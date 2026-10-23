/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/NativeBufferData.h"

#include <array>
#include <iostream>

using namespace rendering;
namespace {
size_t checks = 0;
void Check(bool condition, const char* name)
{
	++checks;
	if (!condition) throw std::runtime_error(name);
}
template<typename F> void Reject(F&& f, const char* name)
{
	bool rejected = false;
	try { f(); } catch (const std::exception&) { rejected = true; }
	Check(rejected, name);
}
NativeBufferLayout Mixed()
{
	return {NativeBufferKind::Vertex, 3, 8, {
		{0, "tag", NativeScalarType::UInt8, NativeValueShape::Scalar, 1, 0, 1, false},
		{1, "value", NativeScalarType::Float32, NativeValueShape::Scalar, 1, 4, 4, false}}};
}
void MixedPartial()
{
	const auto layout = Mixed();
	std::vector<uint8_t> bytes(24, 0xA5);
	// Independent bit image: byte tag and IEEE float addresses are explicit.
	const std::array<uint8_t, 3> tags{7, 8, 9};
	const std::array<uint8_t, 12> values{0,0,0x80,0x3F, 0,0,0,0x40, 0,0,0x40,0x40};
	std::array<NativeBufferAttributeWrite, 2> writes{{{0, tags}, {1, values}}};
	WriteNativeBufferAttributes(bytes, layout, 0, 3, writes);
	const std::array<uint8_t, 24> expected{7,0xA5,0xA5,0xA5,0,0,0x80,0x3F,
		8,0xA5,0xA5,0xA5,0,0,0,0x40, 9,0xA5,0xA5,0xA5,0,0,0x40,0x40};
	Check(std::equal(bytes.begin(), bytes.end(), expected.begin()), "Declared mixed offsets independently match");
	NativeBufferSnapshotSource source; source.Define(layout);
	const auto old = source.Capture(bytes);
	Check(source.Capture(bytes) == old, "Unchanged revision reuses immutable generation");
	const std::array<uint8_t, 4> middle{0,0,0xA0,0x40}; // 5.0, at record1 offset4
	const std::array<NativeBufferAttributeWrite, 1> partial{{{1, middle}}};
	WriteNativeBufferAttributes(bytes, layout, 1, 1, partial); source.InvalidateContents();
	auto next = source.Capture(bytes);
	auto expectedNext = expected; std::copy(middle.begin(), middle.end(), expectedNext.begin() + 12);
	Check(std::equal(bytes.begin(), bytes.end(), expectedNext.begin()), "Middle field preserves neighbors/padding");
	Check(old->bytes == std::vector<uint8_t>(expected.begin(), expected.end()), "Old generation owns pre-update byte image");
	Check(next->object == old->object && next->revision == old->revision + 1, "Mutation advances same-object revision");
	Check(next != old && next->bytes == bytes, "New generation owns actual current data");
	source.Delete(); bytes.clear();
	Check(old->bytes[0] == 7 && next->bytes[12] == 0, "Delete and producer release preserve generations");
	Reject([&] { source.Capture({}); }, "Deleted source cannot capture");
	source.Define(layout); std::vector<uint8_t> fresh(24, 0);
	Check(source.Capture(fresh)->object != old->object, "Re-define has fresh object identity");
}
void SeedAndOddStride()
{
	const NativeBufferLayout layout{NativeBufferKind::Vertex, 2, 5, {
		{0, "value", NativeScalarType::Float32, NativeValueShape::Scalar, 1, 0, 4, false},
		{1, "tag", NativeScalarType::UInt8, NativeValueShape::Scalar, 1, 4, 1, true}}};
	std::vector<uint8_t> independentlySeeded{0x34,0x12,0xC0,0x7F,0xA1, 0,0,0x80,0xBF,0xB2};
	NativeBufferSnapshotSource source; source.Define(layout);
	auto seeded = source.Capture(independentlySeeded);
	Check(seeded->bytes == independentlySeeded, "Capture uses independent seed, not upload roundtrip");
	Check(seeded->bytes[0] == 0x34 && seeded->bytes[3] == 0x7F, "Raw NaN payload retained");
	const std::array<uint8_t, 4> value{0,0,0,0x40};
	const std::array<NativeBufferAttributeWrite, 1> writes{{{0, value}}};
	WriteNativeBufferAttributes(independentlySeeded, layout, 1, 1, writes); source.InvalidateContents();
	const std::vector<uint8_t> expected{0x34,0x12,0xC0,0x7F,0xA1, 0,0,0,0x40,0xB2};
	Check(independentlySeeded == expected, "Legal stride5 unaligned field writes preserve neighbors");
	Check(source.Capture(independentlySeeded)->layout.attributes[1].normalized, "Normalization metadata retained without changing CPU bytes");
}
void RejectedAtomicWrites()
{
	const auto layout = Mixed(); std::vector<uint8_t> bytes(24, 0x69); const auto before = bytes;
	const std::array<uint8_t, 4> valid{1,2,3,4}; const std::array<uint8_t, 3> shortField{1,2,3};
	const std::array<uint8_t, 5> excess{1,2,3,4,5};
	const std::array<NativeBufferAttributeWrite, 2> lateFailure{{{1, valid}, {0, shortField}}};
	Reject([&] { WriteNativeBufferAttributes(bytes, layout, 1, 1, lateFailure); }, "Invalid later field prevents first-field prefix mutation");
	Check(bytes == before, "No prefix mutation after late failure");
	for (const auto& payload: {std::span<const uint8_t>(shortField), std::span<const uint8_t>(excess)}) {
		const std::array<NativeBufferAttributeWrite, 1> writes{{{1, payload}}};
		Reject([&] { WriteNativeBufferAttributes(bytes, layout, 0, 1, writes); }, "Incomplete/excess field rejected");
		Check(bytes == before, "Incomplete/excess input leaves all bytes unchanged");
	}
	const std::array<NativeBufferAttributeWrite, 1> writes{{{1, valid}}};
	for (const size_t offset: {size_t(3), std::numeric_limits<size_t>::max()}) {
		Reject([&] { WriteNativeBufferAttributes(bytes, layout, offset, 1, writes); }, "Element offset outside capacity rejected");
		Check(bytes == before, "Offset rejection leaves bytes unchanged");
	}
	Reject([&] { WriteNativeBufferAttributes(bytes, layout, 0, 4, writes); }, "Excess record count rejected");
	Reject([&] { WriteNativeBufferAttributes(bytes, layout, 0, 0, writes); }, "Empty record interval rejected");
	const std::array<NativeBufferAttributeWrite, 1> unknown{{{9, valid}}};
	Reject([&] { WriteNativeBufferAttributes(bytes, layout, 0, 1, unknown); }, "Unknown attribute rejected");
	const std::array<NativeBufferAttributeWrite, 2> duplicate{{{1, valid}, {1, valid}}};
	Reject([&] { WriteNativeBufferAttributes(bytes, layout, 0, 1, duplicate); }, "Duplicate write rejected");
	Reject([&] { WriteNativeBufferAttributes(std::span<uint8_t>(bytes.data(), 23), layout, 0, 1, writes); }, "Short mutable storage rejected");
	Check(bytes == before, "All rejected transactions preserve bytes");
}
void AliasWrite()
{
	const NativeBufferLayout layout{NativeBufferKind::Vertex, 3, 4, {{0, "tag", NativeScalarType::UInt32, NativeValueShape::Scalar, 1, 0, 4, false}}};
	std::vector<uint8_t> bytes{1,2,3,4, 5,6,7,8, 9,10,11,12};
	const std::array<NativeBufferAttributeWrite, 1> writes{{{0, std::span<const uint8_t>(bytes.data(), 8)}}};
	WriteNativeBufferAttributes(bytes, layout, 1, 2, writes);
	Check(bytes == std::vector<uint8_t>({1,2,3,4,1,2,3,4,5,6,7,8}), "Aliased overlapping update stages original source bytes");
}
void StorageAndRanges()
{
	// Pinned LuaVBO GetTypePtr: GL_FLOAT_VEC4 size4 is four vec4 values (64B).
	// The actual widget's padded32 floats fill two engine records, while the
	// shader consumes eight16B vec4 records from the raw prefix. No stride rewrite.
	const NativeBufferLayout layout{NativeBufferKind::Storage, 8, 64, {{0, "starttriangles", NativeScalarType::Float32, NativeValueShape::Vector4, 4, 0, 64, false}}};
	std::vector<uint8_t> bytes(512, 0xCC), stream(128);
	for (size_t i = 0; i < stream.size(); ++i) stream[i] = static_cast<uint8_t>(i);
	const std::array<NativeBufferAttributeWrite, 1> writes{{{0, stream}}};
	WriteNativeBufferAttributes(bytes, layout, 0, 2, writes);
	Check(std::equal(stream.begin(), stream.end(), bytes.begin()), "Original SSBO raw vec4 stream has exact prefix");
	Check(std::all_of(bytes.begin()+128, bytes.end(), [](auto x) { return x == 0xCC; }), "Original SSBO oversize capacity/padding preserved");
	NativeBufferSnapshotSource source; source.Define(layout); auto snapshot = source.Capture(bytes);
	Check(snapshot->layout.stride == 64 && snapshot->bytes.size() == 512, "Declared SSBO stride/capacity retained");
	const auto full = MakeNativeBufferRange(snapshot, 4, 0, 8, 8, 64);
	Check(full.offset == 0 && full.size == 512 && full.snapshot == snapshot, "Storage binding4 retains complete generation");
	const auto part = MakeNativeBufferRange(snapshot, 7, 2, 3, 8, 64);
	Check(part.offset == 128 && part.size == 192, "Explicit requested logical range independently matches");
	Reject([&] { MakeNativeBufferRange(snapshot, 3, 0, 8, 8); }, "Reserved storage binding rejected");
	Reject([&] { MakeNativeBufferRange(snapshot, 8, 0, 8, 8); }, "Unsupported storage binding rejected");
	Reject([&] { MakeNativeBufferRange(snapshot, 4, 7, 2, 8); }, "Range count overflow rejected before binding");
	Reject([&] { MakeNativeBufferRange(snapshot, 4, std::numeric_limits<size_t>::max(), 1, 8); }, "Huge range offset rejected");
	Reject([&] { MakeNativeBufferRange(snapshot, 4, 1, 1, 8, 128); }, "Backend offset alignment enforced");
	Reject([&] { MakeNativeBufferRange(snapshot, 4, 0, 1, 8, 0); }, "Zero alignment rejected");
	auto uniform = layout; uniform.kind = NativeBufferKind::Uniform;
	NativeBufferSnapshotSource u; u.Define(uniform); const auto us = u.Capture(bytes);
	Check(MakeNativeBufferRange(us, 6, 0, 1, 8).size == 64, "Original user UBO starts at binding6");
	Reject([&] { MakeNativeBufferRange(us, 5, 0, 1, 8); }, "Reserved uniform binding rejected");
}
void LayoutAndLifetime()
{
	const auto layout = Mixed(); NativeBufferSnapshotSource source; source.Define(layout);
	std::vector<uint8_t> bytes(24, 0xDD); const auto snapshot = source.Capture(bytes);
	Reject([&] { source.Define(layout); }, "Define stays immutable");
	Reject([&] { source.Capture(std::span<const uint8_t>(bytes.data(), 23)); }, "Snapshot capacity mismatch rejected");
	Check(source.Capture(bytes) == snapshot, "Rejected requests preserve cached generation");
	for (int variant = 0; variant < 6; ++variant) {
		auto bad = layout;
		if (variant == 0) bad.stride = 0;
		if (variant == 1) bad.elements = std::numeric_limits<size_t>::max();
		if (variant == 2) bad.attributes[1].offset = 7;
		if (variant == 3) bad.attributes[1].id = 0;
		if (variant == 4) bad.attributes[1].offset = 0;
		if (variant == 5) bad.attributes[1].byteSize = 3;
		Reject([&] { CheckedNativeBufferByteSize(bad); }, "Invalid layout rejected without allocation/mutation");
	}
	Reject([&] { MakeNativeBufferRange(snapshot, 4, 0, 1, 8); }, "Vertex buffer cannot be storage binding");
	const NativeBufferLayout index{NativeBufferKind::Index, 3, 2, {{0, "index", NativeScalarType::UInt16, NativeValueShape::Scalar, 1, 0, 2, false}}};
	Check(CheckedNativeBufferByteSize(index) == 6, "Typed unsigned index capacity validated");
	auto badIndex = index; badIndex.attributes[0].scalarType = NativeScalarType::Int16;
	Reject([&] { CheckedNativeBufferByteSize(badIndex); }, "Signed index declaration rejected");
	NativeBufferSnapshotSource moved(std::move(source));
	Reject([&] { source.Capture(bytes); }, "Moved-from producer cannot publish same identity");
	Check(moved.Capture(bytes) == snapshot, "Move preserves original live generation");
	moved.Delete();
	Check(snapshot->bytes == bytes, "Source destruction/deletion retains immutable bytes");
}
}
int main()
{
	try {
		MixedPartial(); SeedAndOddStride(); RejectedAtomicWrites(); AliasWrite(); StorageAndRanges(); LayoutAndLifetime();
		std::cout << "Native CPU buffer: 6 fixtures, " << checks << " checks passed\n";
		return 0;
	} catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
