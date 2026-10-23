/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

#include "CommandColors.h"
#include "Rendering/GlobalRendering.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/Bitmap.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeUIRecorder.h"
#include "System/Exceptions.h"
#endif
#include "MouseCursor.h"
#include "HwMouseCursor.h"
#include "System/Log/ILog.h"
#include "System/SpringMath.h"
#include "System/SafeUtil.h"
#include "System/StringUtil.h"
#include "System/UnorderedMap.hpp"
#include "System/FileSystem/FileHandler.h"
#include "System/FileSystem/FileSystem.h"
#include "System/FileSystem/SimpleParser.h"

#include "System/Misc/TracyDefs.h"

CMouseCursor::CMouseCursor(const std::string& name_, HotSpot hs)
	: name(name_)
	, hotSpot(hs)
{
	frames.reserve(8);
	images.reserve(8);

	Build(name);

	for (FrameData& frame: frames) {
		frame.startTime = animPeriod;
		frame.endTime = (animPeriod += frame.length);

		xmaxsize = std::max(images[frame.imageIdx].xOrigSize, xmaxsize);
		ymaxsize = std::max(images[frame.imageIdx].yOrigSize, ymaxsize);
	}

	if (hotSpot == Center) {
		xofs = xmaxsize / 2;
		yofs = ymaxsize / 2;
	}
}

CMouseCursor::~CMouseCursor()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (hwCursor != nullptr) {
		hwCursor->Kill();
		IHardwareCursor::Free(hwCursor);
	}

#ifndef RECOIL_METAL4_LIVE_SCENE
	for (const auto& image: images) {
		glDeleteTextures(1, &image.texture);
	}
#endif
}


CMouseCursor& CMouseCursor::operator = (CMouseCursor&& mc) noexcept {
	name = std::move(mc.name);
	images = std::move(mc.images);
	frames = std::move(mc.frames);

	if (mc.hwCursor != nullptr) {
		memcpy(hwCursorMem, mc.hwCursorMem, sizeof(hwCursorMem));
		memset(mc.hwCursorMem, 0, sizeof(hwCursorMem));

		hwCursor = reinterpret_cast<IHardwareCursor*>(hwCursorMem);
		mc.hwCursor = nullptr;
	}

	hotSpot = mc.hotSpot;

	animTime = mc.animTime;
	animPeriod = mc.animPeriod;
	currentFrame = mc.currentFrame;

	xmaxsize = mc.xmaxsize;
	ymaxsize = mc.ymaxsize;

	xofs = mc.xofs;
	yofs = mc.yofs;

	hwValid = mc.hwValid;
	return *this;
}


bool CMouseCursor::Build(const std::string& name)
{
	RECOIL_DETAILED_TRACY_ZONE;
	int lastFrame = 1000;

	hwCursor = IHardwareCursor::Alloc(hwCursorMem);
	hwCursor->Init(hotSpot);

	if (!BuildFromSpecFile(name, lastFrame))
		BuildFromFileNames(name, lastFrame);

	hwCursor->Finish();

	return (hwValid = hwCursor->IsValid(), IsValid());
}

bool CMouseCursor::BuildFromSpecFile(const std::string& name, int& lastFrame)
{
	RECOIL_DETAILED_TRACY_ZONE;
	const std::string specFileName = "anims/" + name + ".txt";

	if (!CFileHandler::FileExists(specFileName, SPRING_VFS_RAW_FIRST))
		return false;

	CFileHandler specFile(specFileName);
	CSimpleParser specParser(specFile);

	const std::array<std::pair<std::string, int>, 3> commandsMap = {{{"frame", 0}, {"hotspot", 1}, {"lastframe", 2}}};
		  spring::unsynced_map<std::string, int>     imageIdxMap;

	for (std::string line = specParser.GetCleanLine(); !line.empty(); line = specParser.GetCleanLine()) {
		const std::vector<std::string>& words = specParser.Tokenize(line, 2);
		const std::string& command = StringToLower(words[0]);

		const auto cmdPred = [&](const std::pair<std::string, int>& p) { return (p.first == command); };
		const auto cmdIter = std::find_if(commandsMap.cbegin(), commandsMap.cend(), cmdPred);

		if (cmdIter == commandsMap.end()) {
			LOG_L(L_ERROR, "[MouseCursor::%s] unknown command \"%s\" in file \"%s\"", __func__, command.c_str(), specFileName.c_str());
			continue;
		}

		if ((cmdIter->second == 0) && (words.size() >= 2)) {
			const std::string& imageName = words[1];
			float length = MIN_FRAME_LENGTH;

			if (words.size() >= 3)
				length = std::max(length, float(std::atof(words[2].c_str())));

			const auto imageIdxIt = imageIdxMap.find(imageName);

			if (imageIdxIt != imageIdxMap.end()) {
				frames.emplace_back(imageIdxIt->second, length);
				hwCursor->PushFrame(imageIdxIt->second, length);
				continue;
			}

			ImageData image;

			if (LoadCursorImage(imageName, image)) {
				hwCursor->SetDelay(length);
				imageIdxMap[imageName] = images.size();

				images.push_back(image);
				frames.emplace_back(images.size() - 1, length);
			}

			continue;
		}

		if ((cmdIter->second == 1) && (!words.empty())) {
			if (words[1] == "topleft") {
				hwCursor->SetHotSpot(hotSpot = TopLeft);
				continue;
			}
			if (words[1] == "center") {
				hwCursor->SetHotSpot(hotSpot = Center);
				continue;
			}

			LOG_L(L_ERROR, "[MouseCursor::%s] unknown hotspot \"%s\" in file \"%s\"", __func__, words[1].c_str(), specFileName.c_str());
			continue;
		}

		if ((cmdIter->second == 2) && (!words.empty())) {
			lastFrame = atoi(words[1].c_str());
			continue;
		}
	}

	return (IsValid());
}


bool CMouseCursor::BuildFromFileNames(const std::string& name, int lastFrame)
{
	RECOIL_DETAILED_TRACY_ZONE;
	// find the image file type to use
	const char* ext = "";
	const char* exts[] = {"png", "tga", "bmp"};

	char animFrameStr[512];

	for (auto& e: exts) {
		memset(animFrameStr, 0, sizeof(animFrameStr));
		snprintf(animFrameStr, sizeof(animFrameStr) - 1, "anims/%s_%d.%s", name.c_str(), 0, ext = e);

		if (CFileHandler::FileExists(animFrameStr, SPRING_VFS_RAW_FIRST))
			break;
	}

	while (int(frames.size()) < lastFrame) {
		memset(animFrameStr, 0, sizeof(animFrameStr));
		snprintf(animFrameStr, sizeof(animFrameStr) - 1, "anims/%s_%d.%s", name.c_str(), static_cast<int>(frames.size()), ext);

		ImageData image;

		if (!LoadCursorImage(animFrameStr, image))
			break;

		images.push_back(image);
		frames.emplace_back(images.size() - 1, float(DEF_FRAME_LENGTH));
	}

	return (IsValid());
}

float4 CMouseCursor::CalcFrameMatrixParams(const float3& winCoors, const float2& winScale) const
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (winCoors.z > 1.0f || frames.empty())
		return { 0.0f, 0.0f, 0.0f, 0.0f };

	const FrameData& frame = frames[currentFrame];
	const ImageData& image = images[frame.imageIdx];

	const float qis = winScale.x;
	const float  xs = image.xAlignedSize * qis;
	const float  ys = image.yAlignedSize * qis;

	const float rxs = xs * globalRendering->pixelX;
	const float rys = ys * globalRendering->pixelY;

	// center on hotspot
	const float xp = (winCoors.x - (xofs * qis)) * globalRendering->pixelX;
	const float yp = (winCoors.y - winScale.y * (ys - (yofs * qis))) * globalRendering->pixelY;

	return { xp, yp, rxs, rys };
}

#if 0
}
bool CMouseCursor::LoadDummyImage()
{
	RECOIL_DETAILED_TRACY_ZONE;
	ImageData id;
	CBitmap bn;

	bn.Alloc(16, 16, 4);
	bn.Fill(SColor(255, 255 * 0, 255, 255));

	id.texture = bn.CreateTexture();
	id.xOrigSize = bn.xsize;
	id.yOrigSize = bn.ysize;
	id.xAlignedSize = bn.xsize;
	id.yAlignedSize = bn.ysize;

	images.emplace_back(id);
	frames.emplace_back(images.size() - 1, float(DEF_FRAME_LENGTH));

	hwCursor->PushImage(bn.xsize, bn.ysize, bn.GetRawMem());
}
#endif

bool CMouseCursor::LoadCursorImage(const std::string& name, ImageData& image)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!CFileHandler::FileExists(name, SPRING_VFS_RAW_FIRST))
		return false;

	CBitmap b;
	if (!b.Load(name)) {
		LOG_L(L_ERROR, "[MouseCursor::%s] bad image file \"%s\"", __func__, name.c_str());
		return false;
	}

#ifdef RECOIL_METAL4_LIVE_SCENE
	const auto requireRGBA8 = [&](const CBitmap& bitmap) {
		if (bitmap.compressed || bitmap.channels != 4 || bitmap.GetDataTypeSize() != 1 || bitmap.xsize <= 0 || bitmap.ysize <= 0)
			throw content_error("Original native cursor image is not RGBA8: " + name);
	};
	requireRGBA8(b);
#endif
	const auto applyBMPMask = [&]() {
#if defined(HEADLESS) && defined(RECOIL_METAL4_LIVE_SCENE)
		// Bitmap::SetTransparent is a HEADLESS no-op. Preserve its original
		// RGBA8 RGB-key match and transparent-black replacement locally.
		auto* const bytes = b.GetRawMem();
		for (size_t offset = 0; offset < b.GetMemSize(); offset += 4) {
			if (bytes[offset] == 84 && bytes[offset + 1] == 84 && bytes[offset + 2] == 252)
				std::fill_n(bytes + offset, 4, uint8_t(0));
		}
#else
		b.SetTransparent(SColor(84, 84, 252));
#endif
	};
	const auto reverseYAxis = [&]() {
#if defined(HEADLESS) && defined(RECOIL_METAL4_LIVE_SCENE)
		// Bitmap::ReverseYAxis is also a HEADLESS no-op. Swap actual rows
		// here, before the original NPOT padding/copy and native snapshot.
		auto* const bytes = b.GetRawMem();
		const size_t rowBytes = size_t(b.xsize) * 4;
		for (int y = 0; y < b.ysize / 2; ++y) {
			auto* const low = bytes + size_t(y) * rowBytes;
			auto* const high = bytes + size_t(b.ysize - 1 - y) * rowBytes;
			std::swap_ranges(low, low + rowBytes, high);
		}
#else
		b.ReverseYAxis();
#endif
	};

	// hardcoded bmp transparency mask
	if (FileSystem::GetExtensionLowerCase(name) == "bmp")
		applyBMPMask();

	if (hwCursor->NeedsYFlip()) {
		// WINDOWS
		reverseYAxis();
		hwCursor->PushImage(b.xsize, b.ysize, b.GetRawMem());
	} else {
		// X11
		hwCursor->PushImage(b.xsize, b.ysize, b.GetRawMem());
		reverseYAxis();
	}


#ifdef RECOIL_METAL4_LIVE_SCENE
	// Snapshot the same flipped and padded upload bytes as CreateTexture.
	// Frame records keep their original image indices and animation lengths.
	const auto publishImage = [&](const CBitmap& bitmap) {
		requireRGBA8(bitmap);
		auto texture = std::make_shared<rendering::ScreenTexture>();
		texture->sourceName = name;
		texture->width = bitmap.xsize;
		texture->height = bitmap.ysize;
		texture->bytesPerRow = size_t(bitmap.xsize) * 4;
		texture->bytes.assign(bitmap.GetRawMem(), bitmap.GetRawMem() + bitmap.GetMemSize());
		texture->generateMipmaps = bitmap.GetReqNumLevels() > 1;
		image.nativeTexture = std::move(texture);
	};
#endif

	const int nx = std::bit_ceil <uint32_t> (b.xsize);
	const int ny = std::bit_ceil <uint32_t> (b.ysize);

	if (b.xsize != nx || b.ysize != ny) {
		CBitmap bn;
		bn.Alloc(nx, ny);
		bn.CopySubImage(b, 0, ny - b.ysize);

#ifdef RECOIL_METAL4_LIVE_SCENE
		publishImage(bn);
#else
		image.texture = bn.CreateTexture();
#endif
		image.xOrigSize = b.xsize;
		image.yOrigSize = b.ysize;
		image.xAlignedSize = bn.xsize;
		image.yAlignedSize = bn.ysize;
	} else {
#ifdef RECOIL_METAL4_LIVE_SCENE
		publishImage(b);
#else
		image.texture = b.CreateTexture();
#endif
		image.xOrigSize = b.xsize;
		image.yOrigSize = b.ysize;
		image.xAlignedSize = b.xsize;
		image.yAlignedSize = b.ysize;
	}

	return true;
}


void CMouseCursor::Draw(int x, int y, float scale) const
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (frames.empty())
		return;

	const FrameData& frame = frames[currentFrame];
	const ImageData& image = images[frame.imageIdx];

	const float3 winCoors = { x * 1.0f, (globalRendering->viewSizeY - y) * 1.0f, 0.0f };
	const float2 winScale = { std::max(scale, -scale), 1.0f };
	const float4& matParams = CalcFrameMatrixParams(winCoors, winScale);

#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& recorder = rendering::ui::GetRecorder();
	// Draw takes primary-viewport-local coordinates (also used by Centroid).
	// The host's full-window viewport keeps a pointer on the dual minimap
	// visible. Rebase the original view-normalized geometry exactly once;
	// the screen consumer alone maps logical window pixels to render pixels.
	const float4 nativeMatParams = {
		(matParams.x * globalRendering->viewSizeX + globalRendering->viewPosX) / globalRendering->winSizeX,
		(matParams.y * globalRendering->viewSizeY + globalRendering->viewPosY) / globalRendering->winSizeY,
		matParams.z * globalRendering->viewSizeX / globalRendering->winSizeX,
		matParams.w * globalRendering->viewSizeY / globalRendering->winSizeY
	};
	recorder.PushAttrib(rendering::ui::All);
	recorder.MatrixMode(rendering::ui::NativeUIMatrixMode::Projection);
	recorder.PushMatrix();
	recorder.LoadMatrix(rendering::ui::OrthoMatrix(0, 1, 0, 1, -1, 1));
	recorder.MatrixMode(rendering::ui::NativeUIMatrixMode::ModelView);
	recorder.PushMatrix();
	recorder.LoadMatrix(rendering::ui::TranslationMatrix(nativeMatParams.x, nativeMatParams.y, 0));
	recorder.MultMatrix(rendering::ui::ScaleMatrix(nativeMatParams.z, nativeMatParams.w, 1));
	recorder.MatrixMode(rendering::ui::NativeUIMatrixMode::Texture);
	recorder.PushMatrix();
	recorder.LoadMatrix(rendering::ui::IdentityMatrix());
	rendering::ScreenSampler sampler;
	sampler.wrapS = sampler.wrapT = rendering::ScreenWrap::Repeat;
	sampler.mipFilter = image.nativeTexture->generateMipmaps ? rendering::ScreenMipFilter::Linear : rendering::ScreenMipFilter::None;
	recorder.SetTexture(image.nativeTexture, sampler);
	recorder.SetTextureEnabled(false, 1);
	recorder.SetBlend({}); // original SRC_ALPHA / ONE_MINUS_SRC_ALPHA
	recorder.SetAlphaTest({true, rendering::ScreenAlphaCompare::Greater, 0.01f});
	// Same diagonal and winding as RenderBuffer::AddQuadTrianglesImpl.
	recorder.Begin(rendering::ScreenPrimitive::Triangles, "CMouseCursor::Draw " + name);
	for (const unsigned index: {3u, 0u, 1u, 3u, 1u, 2u}) {
		const auto& vertex = CURSOR_VERTS[index];
		const float4 color = vertex.c;
		recorder.Color({color.x, color.y, color.z, color.w});
		recorder.TexCoord(vertex.s, vertex.t);
		recorder.Vertex(vertex.pos.x, vertex.pos.y, vertex.pos.z);
	}
	recorder.End();
	recorder.PopMatrix();
	recorder.MatrixMode(rendering::ui::NativeUIMatrixMode::Projection);
	recorder.PopMatrix();
	recorder.MatrixMode(rendering::ui::NativeUIMatrixMode::ModelView);
	recorder.PopMatrix();
	recorder.PopAttrib();
#else
	auto& rb = RenderBuffer::GetTypedRenderBuffer<VA_TYPE_TC>();
	auto& sh = rb.GetShader();

	CMatrix44f cursorMat;
	cursorMat.Translate(matParams.x, matParams.y, 0.0f);
	cursorMat.Scale({ matParams.z, matParams.w, 1.0f });

	rb.AddQuadTriangles(CURSOR_VERTS);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadMatrixf(cursorMat);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadMatrixf(CMatrix44f::ClipOrthoProj01(globalRendering->supportClipSpaceControl * 1.0f));


	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, image.texture);

	sh.Enable();
	sh.SetUniform("alphaCtrl", 0.01f, 1.0f, 0.0f, 0.0f); // test > 0.01

	rb.Submit(GL_TRIANGLES);

	sh.SetUniform("alphaCtrl", 0.0f, 0.0f, 0.0f, 1.0f); // no test
	sh.Disable();

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glDisable(GL_BLEND);
#endif
}

void CMouseCursor::Update()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (frames.empty())
		return;

	animTime = math::fmod(animTime + globalRendering->lastFrameTime * 0.001f, animPeriod);

	if (animTime < frames[currentFrame].startTime) {
		currentFrame = 0;
		return;
	}

	while (animTime > frames[currentFrame].endTime) {
		currentFrame += 1;
		currentFrame %= frames.size();
	}
}


void CMouseCursor::BindTexture() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (frames.empty())
		return;

	const FrameData& frame = frames[currentFrame];
	const ImageData& image = images[frame.imageIdx];

	glBindTexture(GL_TEXTURE_2D, image.texture);
}

void CMouseCursor::BindHwCursor() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	assert(hwCursor != nullptr);
	hwCursor->Bind();
}

void CMouseCursor::UpdateHwCursor() const
{
	RECOIL_DETAILED_TRACY_ZONE;
    assert(hwCursor != nullptr);
    hwCursor->Update(animTime);
}
