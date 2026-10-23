/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

// TODO: move this out of Sim, this is rendering code!

#include "LineDrawer.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeUIRecorder.h"
#include <algorithm>
#endif

#include <cmath>
#include <stdexcept>

#include "Rendering/GlobalRendering.h"
#include "Game/UI/CommandColors.h"

CLineDrawer lineDrawer;


CLineDrawer::CLineDrawer()
	: lineStipple(false)
	, useColorRestarts(false)
	, useRestartColor(false)
	, restartAlpha(0.0f)
	, restartColor(NULL)
	, lastPos(ZeroVector)
	, lastColor(NULL)
	, stippleTimer(0.0f)
{
	lines.reserve(32);
	stippled.reserve(32);
}


void CLineDrawer::UpdateLineStipple()
{
	stippleTimer += (globalRendering->lastFrameTime * 0.001f * cmdColors.StippleSpeed());
	stippleTimer = std::fmod(stippleTimer, (16.0f / 20.0f));
}


void CLineDrawer::SetupLineStipple()
{
	const unsigned int stipPat = (0xffff & cmdColors.StipplePattern());
	if ((stipPat != 0x0000) && (stipPat != 0xffff)) {
		lineStipple = true;
	} else {
		lineStipple = false;
		return;
	}
	const unsigned int fullPat = (stipPat << 16) | (stipPat & 0x0000ffff);
	const int shiftBits = 15 - (int(stippleTimer * 20.0f) % 16);
	glLineStipple(cmdColors.StippleFactor(), (fullPat >> shiftBits));
#ifdef RECOIL_METAL4_LIVE_SCENE
	const auto factor=std::clamp(static_cast<int>(cmdColors.StippleFactor()),1,256);
	const auto pattern=static_cast<std::uint16_t>(fullPat>>shiftBits);
	rendering::ui::GetRecorder().UpdateWorldState([factor,pattern](auto& state){state.stippleFactor=factor;state.stipplePattern=pattern;},false,true);
#endif
}

#ifdef RECOIL_METAL4_LIVE_SCENE
std::pair<int,std::uint16_t> CLineDrawer::GetNativeStippleSettings() const
{
	const unsigned int pattern=0xffff & cmdColors.StipplePattern();
	const unsigned int fullPattern=(pattern<<16)|pattern;
	const int shift=15-(int(stippleTimer*20.0f)%16);
	return {cmdColors.StippleFactor(),static_cast<std::uint16_t>(fullPattern>>shift)};
}
std::vector<CLineDrawer::NativePath> CLineDrawer::ConsumeNativePaths()
{
	std::vector<NativePath> result;
	const unsigned int pattern = 0xffff & cmdColors.StipplePattern();
	const unsigned int repeated = (pattern << 16) | pattern;
	const int shift = 15 - (int(stippleTimer * 20.0f) % 16);
	const auto append = [&](const std::vector<LinePair>& paths, bool stippled) {
		for (const auto& source: paths) {
			if (source.colors.empty()) continue;
			if (source.verts.size()%3 || source.colors.size()%4 || source.verts.size()/3 != source.colors.size()/4)
				throw std::logic_error("Native command path has inconsistent original vertex/color extents");
			if (source.type != GL_LINES && source.type != GL_LINE_STRIP)
				throw std::logic_error("Native command path topology is unsupported");
			NativePath path;
			path.lineStrip = source.type == GL_LINE_STRIP;
			path.stippled = stippled;
			path.pattern = static_cast<std::uint16_t>(repeated >> shift);
			path.factor = cmdColors.StippleFactor();
			for (size_t vertex=0;vertex<source.verts.size()/3;++vertex) {
				path.positions.push_back({source.verts[vertex*3],source.verts[vertex*3+1],source.verts[vertex*3+2]});
				path.colors.push_back({source.colors[vertex*4],source.colors[vertex*4+1],source.colors[vertex*4+2],source.colors[vertex*4+3]});
			}
			result.push_back(std::move(path));
		}
	};
	append(lines,false); append(stippled,true);
	lines.clear(); stippled.clear();
	return result;
}
#endif


void CLineDrawer::DrawAll()
{
	if (lines.empty() && stippled.empty())
		return;
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glPushAttrib(GL_ENABLE_BIT);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_LINE_STIPPLE);

	for (int i = 0; i<lines.size(); ++i) {
		int size = lines[i].colors.size();
		if(size > 0) {
			glColorPointer(4, GL_FLOAT, 0, &lines[i].colors[0]);
			glVertexPointer(3, GL_FLOAT, 0, &lines[i].verts[0]);
			glDrawArrays(lines[i].type, 0, size/4);
		}
	}

	if (!stippled.empty()) {
		glEnable(GL_LINE_STIPPLE);
		for (int i = 0; i<stippled.size(); ++i) {
			int size = stippled[i].colors.size();
			if(size > 0) {
				glColorPointer(4, GL_FLOAT, 0, &stippled[i].colors[0]);
				glVertexPointer(3, GL_FLOAT, 0, &stippled[i].verts[0]);
				glDrawArrays(stippled[i].type, 0, size/4);
			}
		}
		glDisable(GL_LINE_STIPPLE);
	}

	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_VERTEX_ARRAY);
	glPopAttrib();

	lines.clear();
	stippled.clear();
}
