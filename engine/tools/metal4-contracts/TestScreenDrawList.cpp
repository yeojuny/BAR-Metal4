/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Backend/ScreenDrawList.h"
#include "Rendering/Backend/NativeWeaponRangeData.h"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace rendering;
int checks = 0;

void Check(bool condition, const char* label)
{
	++checks;
	if (!condition) {
		std::cerr << "FAIL: " << label << '\n';
		std::exit(EXIT_FAILURE);
	}
}

template<typename F>
void Throws(F&& function, const char* label)
{
	bool failed = false;
	try { function(); } catch (const std::exception&) { failed = true; }
	Check(failed, label);
}

void Quad(ScreenDrawList& list)
{
	list.BeginQuads();
	list.Vertex(10, 20);
	list.Vertex(40, 20);
	list.Vertex(40, 60);
	list.Vertex(10, 60);
	list.End();
}
// CPU packet fixture for the real range validator and screen command producer.
std::shared_ptr<NativeWeaponRangeDraw> RangeMarker(bool mask)
{
	auto draw = std::make_shared<NativeWeaponRangeDraw>();
	auto defines = std::make_shared<NativeWeaponRangeDefines>();
	defines->family = mask ? NativeWeaponRangeFamily::Defense : NativeWeaponRangeFamily::Attack;
	defines->maskPass = mask;
	draw->program.defines = defines;
	draw->program.inMiniMap = mask ? 0 : 1;
	draw->program.staticUnits = 1;
	draw->primitive = mask ? NativeDrawPrimitive::TriangleFan : NativeDrawPrimitive::LineLoop;
	draw->sourceDrawFrame = 42;
	draw->vertexCount = 3;
	draw->instanceCount = 1;
	for (bool instance: {false, true}) {
		auto buffer = std::make_shared<NativeBufferSnapshot>();
		buffer->object = instance ? 2 : 1;
		buffer->revision = 1;
		buffer->layout.elements = instance ? 1 : 3;
		buffer->layout.stride = instance ? 96 : 16;
		for (unsigned i = 0; i < (instance ? 6u : 1u); ++i) {
			NativeBufferAttribute attribute;
			attribute.id = i + unsigned(instance);
			attribute.offset = i * 16;
			attribute.byteSize = 16;
			attribute.declaredCount = 4;
			attribute.scalarType = instance && i == 5 ? NativeScalarType::UInt32 : NativeScalarType::Float32;
			buffer->layout.attributes.push_back(attribute);
		}
		buffer->bytes.resize(CheckedNativeBufferByteSize(buffer->layout));
		(instance ? draw->instances : draw->vertices) = buffer;
	}
	auto height = std::make_shared<NativeCUSTextureObject>();
	height->identity = height->contextGeneration = height->version = 1;
	height->storage = NativeTextureStorageKnowledge::Published;
	draw->height.knowledge = NativeCUSBindingKnowledge::BoundObject;
	draw->height.object = height;
	NativeCUSUniformStorage uniforms;
	uniforms.NewContext();
	uniforms.PublishMatrices(42, std::array<std::uint8_t, NativeCUSMatrixBytes>{});
	uniforms.PublishParams(42, std::array<std::uint8_t, NativeCUSParamsBytes>{}, {});
	draw->original = uniforms.Current();
	return draw;
}

void RangeCommands()
{
	for (bool mask: {false, true}) {
		ScreenDrawList list;
		if (mask) list.ClearAttachments(ScreenAttachmentColor | ScreenAttachmentDepth, {}, 16, 8, {}, {true, true, true, true}, true, 0);
		else Quad(list);
		const auto vertexCount = list.Vertices().size(), indexCount = list.Indices().size();
		auto packet = RangeMarker(mask);
		list.AppendWeaponRange(packet);
		const auto& command = list.Commands().back();
		std::cout << "Range marker mask=" << mask << " first=" << command.firstIndex << " count=" << command.indexCount << " indices=" << indexCount << '\n';
		Check(command.indexCount == 0 && command.firstIndex == indexCount, "typed range marker has an initialized empty fixed-function index range");
		Check(command.weaponRange == packet && command.weaponRange->vertexCount == 3 && command.weaponRange->instanceCount == 1, "typed range retains its actual geometry and instances");
		Check(list.Indices().size() == indexCount && list.Vertices().size() == vertexCount, "typed range does not append placeholder fixed geometry");
		ScreenDrawList copied = list;
		ScreenDrawList moved = std::move(copied);
		list.ClearGeometry();
		Check(moved.Commands().back().weaponRange == packet && moved.Commands().back().indexCount == 0, "target list copy and move retain the typed owner and range");
		Quad(moved);
		Check(moved.Commands().back().firstIndex == indexCount && moved.Commands().back().indexCount == 6, "following fixed draw retains its independent index range");
		auto invalid = std::make_shared<NativeWeaponRangeDraw>(*packet);
		invalid->vertexCount = 4;
		Throws([&] { moved.AppendWeaponRange(invalid); }, "range marker still rejects typed geometry outside the uploaded buffer");
	}
}
} // namespace

int main()
{
	RangeCommands();
	ScreenDrawList list;
	Check(list.CurrentColor() == ScreenColor{1, 1, 1, 1}, "white entry color");
	Check(list.CurrentState() == ScreenDrawState{}, "explicit default alpha blend and disabled scissor");
	Throws([&] { list.Vertex(0, 0); }, "Vertex outside Begin fails");
	Throws([&] { list.End(); }, "End outside Begin fails");
	list.SetScissor(ScreenScissor{3, 7, 90, 80});
	list.BeginQuads();
	list.Color({1, 0, 0, 0.2f});
	list.Vertex(10, 20, 0);
	list.Color({0, 1, 0, 0.4f});
	list.Vertex(40, 20, 0);
	list.Color({0, 0, 1, 0.6f});
	list.Vertex(40, 60, 0);
	list.Color({1, 1, 0, 0.8f});
	list.Vertex(10, 60, 0);
	Throws([&] { list.BeginQuads(); }, "nested Begin fails");
	Throws([&] { list.SetBlend({}); }, "blend change during Begin fails");
	Throws([&] { list.SetScissor(std::nullopt); }, "scissor change during Begin fails");
	Throws([&] { list.ClearGeometry(); }, "clear during Begin fails");
	list.End();
	Check(!list.IsRecording(), "End closes batch");
	Check(list.Vertices().size() == 4 && list.Indices().size() == 6 && list.Commands().size() == 1, "quad has one indexed command");
	const std::array<std::uint32_t, 6> expected{0, 1, 2, 0, 2, 3};
	for (std::size_t i = 0; i < expected.size(); ++i)
		Check(list.Indices()[i] == expected[i], "ordered quad diagonal 0-2");
	Check(list.Vertices()[0].color == ScreenColor{1, 0, 0, 0.2f}, "first per-vertex color and alpha");
	Check(list.Vertices()[1].color == ScreenColor{0, 1, 0, 0.4f}, "second per-vertex color and alpha");
	Check(list.Vertices()[2].color == ScreenColor{0, 0, 1, 0.6f}, "third per-vertex color and alpha");
	Check(list.Vertices()[3].color == ScreenColor{1, 1, 0, 0.8f}, "fourth per-vertex color and alpha");
	Check(list.Vertices()[0].y == 20 && list.Vertices()[2].y == 60, "bottom-left coordinates retain y-up order");
	Check(list.CurrentColor() == ScreenColor{1, 1, 0, 0.8f}, "End preserves final color state leak");
	const auto firstState = list.Commands()[0].state;
	list.SetScissor(std::nullopt);
	ScreenBlendState additive;
	additive.destinationColor = additive.destinationAlpha = ScreenBlendFactor::One;
	list.SetBlend(additive);
	Quad(list);
	Check(list.Commands()[0].state == firstState, "later state mutation does not change previous command");
	Check(list.Commands()[1].state.blend == additive && !list.Commands()[1].state.scissor, "new draw carries its blend/scissor snapshot");
	Check(list.Commands()[1].firstIndex == 6 && list.Commands()[1].indexCount == 6, "draw index ranges retain order");
	Check(list.Indices()[6] == 4 && list.Indices()[11] == 7, "second draw global vertex indices");
	Check(list.Vertices()[4].color == ScreenColor{1, 1, 0, 0.8f}, "next draw inherits current color");

	const auto committedVertices = list.Vertices().size();
	list.BeginQuads();
	list.Color({0.2f, 0.3f, 0.4f, 0.37f});
	list.Vertex(1, 2);
	Throws([&] { list.End(); }, "incomplete quad errors");
	Check(list.IsRecording() && list.Vertices().size() == committedVertices, "incomplete batch cannot partially commit");
	list.Abort();
	Check(!list.IsRecording() && list.CurrentColor()[3] == 0.37f, "Abort retains color mutation but removes pending geometry");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	Throws([&] { list.Color({nan, 0, 0, 1}); }, "nonfinite color rejected");
	Check(list.CurrentColor() == ScreenColor{0.2f, 0.3f, 0.4f, 0.37f}, "invalid color leaves prior state intact");
	list.BeginQuads();
	Throws([&] { list.Vertex(nan, 0); }, "nonfinite vertex rejected");
	list.Abort();
	additive.sourceColor = static_cast<ScreenBlendFactor>(255);
	Throws([&] { list.SetBlend(additive); }, "invalid typed blend enum rejected");
	list.ClearGeometry();
	Check(list.Vertices().empty() && list.Indices().empty() && list.Commands().empty(), "ClearGeometry removes completed geometry");
	Check(list.CurrentColor()[3] == 0.37f && list.CurrentState().blend.destinationColor == ScreenBlendFactor::One, "ClearGeometry does not hide state leaks");
	list.BeginQuads();
	list.Vertex(1, 2);
	list.Vertex(1, 2);
	list.Vertex(4, 5);
	list.Vertex(6, 7);
	list.End();
	Check(list.Vertices().size() == 4 && list.Vertices()[0].x == list.Vertices()[1].x && list.Indices().size() == 6, "degenerate cut-corner quad retained");

	ScreenDrawList textured;
	auto atlas = std::make_shared<ScreenTexture>();
	atlas->sourceName = "real-font immutable-generation contract fixture";
	atlas->generation = 9;
	atlas->format = ScreenTextureFormat::R8Unorm;
	atlas->width = atlas->height = 1;
	atlas->bytesPerRow = 1;
	atlas->bytes = {128};
	textured.SetTexture(atlas, ScreenMaterial::FontCoverage, ScreenUVMode::Texel);
	textured.Color({0.1f, 0.2f, 0.3f, 0.4f});
	textured.TexCoord(3, 5);
	const auto fontEntry = textured.CaptureState();
	textured.Begin(ScreenPrimitive::Triangles);
	textured.Vertex(1, 2);
	textured.TexCoord(7, 11);
	textured.Vertex(3, 4);
	textured.TexCoord(13, 17);
	textured.Vertex(5, 6);
	textured.End();
	Check(textured.TexCoords().size() == textured.Vertices().size(), "parallel UV stream preserves vertex ABI");
	Check(textured.TexCoords()[0] == ScreenTexCoord{3, 5} && textured.TexCoords()[2] == ScreenTexCoord{13, 17}, "immediate per-vertex pixel UVs retained");
	Check(textured.Commands()[0].state.texture == atlas && textured.Commands()[0].state.uvMode == ScreenUVMode::Texel, "command owns original font generation and UV mode");
	const auto previousSnapshot = textured.Commands()[0].state.texture;
	auto replacement = std::make_shared<ScreenTexture>(*atlas);
	replacement->generation = 10;
	replacement->bytes = {255};
	textured.SetTexture(replacement);
	atlas.reset();
	Check(previousSnapshot->generation == 9 && previousSnapshot->bytes[0] == 128, "replacement cannot mutate retained atlas generation");
	textured.Color({1, 0, 0, 1});
	textured.TexCoord(0, 0);
	textured.RestoreState(fontEntry);
	Check(textured.CaptureState().color == fontEntry.color && textured.CurrentTexCoord() == fontEntry.texCoord && textured.CurrentState() == fontEntry.draw, "font Begin-End restores color UV blend texture alpha state");
	textured.SetAlphaTest({true, ScreenAlphaCompare::Greater, 2});
	Check(textured.CurrentState().alphaTest.reference == 1, "GL alpha reference clamps to one");
	textured.SetAlphaTest({true, ScreenAlphaCompare::Less, -2});
	Check(textured.CurrentState().alphaTest.reference == 0, "GL alpha reference clamps to zero");
	const auto validState = textured.CaptureState();
	Throws([&] { textured.TexCoord(nan, 0); }, "nonfinite UV rejected before current-state mutation");
	Throws([&] { textured.SetTexture(nullptr, ScreenMaterial::FontCoverage); }, "font material rejects missing resource");
	Throws([&] { textured.SetAlphaTest({true, ScreenAlphaCompare::Greater, nan}); }, "nonfinite alpha reference rejected");
	Check(textured.CurrentState() == validState.draw && textured.CurrentTexCoord() == validState.texCoord, "invalid texture alpha and UV calls leave previous state intact");
	const std::array<ScreenVertex, 3> fontVertices{{{4, 5, 0, {1, 1, 1, 0.3f}}, {8, 5, 0, {1, 1, 1, 0.3f}}, {8, 9, 0, {1, 1, 1, 0.3f}}}};
	const std::array<ScreenTexCoord, 3> fontUV{{{1, 2}, {5, 2}, {5, 6}}};
	const std::array<std::uint32_t, 3> fontIndices{0, 1, 2};
	textured.AppendTriangles(fontVertices, fontUV, fontIndices, fontEntry.draw);
	Check(textured.Commands().size() == 2 && textured.Indices()[3] == 3 && textured.Indices()[5] == 5, "source font triangles append in order with local index rebasing");
	Check(textured.CurrentState() == validState.draw && textured.CurrentTexCoord() == validState.texCoord, "source font append does not mutate recorder state");
	const auto committedCount = textured.Vertices().size();
	Throws([&] { textured.AppendTriangles(fontVertices, std::span(fontUV).first(2), fontIndices, fontEntry.draw); }, "source font UV count mismatch rejected");
	const std::array<std::uint32_t, 3> badIndices{0, 1, 3};
	Throws([&] { textured.AppendTriangles(fontVertices, fontUV, badIndices, fontEntry.draw); }, "source font local index out of range rejected");
	Check(textured.Vertices().size() == committedCount && textured.Commands().size() == 2, "invalid source font batch cannot partially commit");

	ScreenDrawList primitiveList;
	primitiveList.Begin(ScreenPrimitive::TriangleStrip);
	for (int i = 0; i < 4; ++i) primitiveList.Vertex(static_cast<float>(i), 0);
	primitiveList.End();
	const std::array<std::uint32_t, 6> stripIndices{0, 1, 2, 2, 1, 3};
	Check(std::equal(primitiveList.Indices().begin(), primitiveList.Indices().end(), stripIndices.begin(), stripIndices.end()), "strip alternating winding matches source primitive");
	primitiveList.ClearGeometry();
	primitiveList.Begin(ScreenPrimitive::TriangleFan);
	for (int i = 0; i < 4; ++i) primitiveList.Vertex(static_cast<float>(i), 0);
	primitiveList.End();
	const std::array<std::uint32_t, 6> fanIndices{0, 1, 2, 0, 2, 3};
	Check(std::equal(primitiveList.Indices().begin(), primitiveList.Indices().end(), fanIndices.begin(), fanIndices.end()), "fan source anchor order retained");
	std::cout << "PASS: " << checks << " screen draw recorder checks\n";
}
