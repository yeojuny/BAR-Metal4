/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#include "GeometryBuffer.h"
#ifdef RECOIL_METAL4_LIVE_SCENE
#include "Rendering/Backend/NativeTextureContext.h"
#include <limits>
#include <stdexcept>
#endif
#include "Rendering/GlobalRendering.h"
#include "System/Config/ConfigHandler.h"

#include <algorithm>
#include <cstring> //memset

#include "System/Misc/TracyDefs.h"

void GL::GeometryBuffer::Init(bool ctor) {
	RECOIL_DETAILED_TRACY_ZONE;
	// if dead, this must be a non-ctor reload
	assert(!dead || !ctor);

	memset(&bufferTextureIDs[0], 0, sizeof(bufferTextureIDs));
	memset(&bufferAttachments[0], 0, sizeof(bufferAttachments));

	// NOTE:
	//   initial buffer size must be 0 s.t. prevSize != currSize when !init
	//   (Lua can toggle drawDeferred and might be the first to cause a call
	//   to Create)
	prevBufferSize = GetWantedSize(false);
	currBufferSize = GetWantedSize(true);

	dead = false;
	bound = false;
	msaa |= configHandler->GetBool("AllowMultiSampledFrameBuffers");
#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& textureContext = rendering::OriginalNativeTextureContext();
	if (textureContext.RegisterGeometry(this, name))
		msaa &= textureContext.GeometryMultisampleSupported(this);
	else
#endif
	msaa &= globalRendering->supportMSAAFrameBuffer;
	// composite passes sample the color attachment with a plain sampler2D
	hasColorTex &= !msaa;
}

void GL::GeometryBuffer::Kill(bool dtor) {
	RECOIL_DETAILED_TRACY_ZONE;
	if (dead) {
		// if already dead, this must be final cleanup
		assert(dtor);
#ifdef RECOIL_METAL4_LIVE_SCENE
		auto& textureContext = rendering::OriginalNativeTextureContext();
		if (textureContext.OwnsGeometry(this)) textureContext.UnregisterGeometry(this);
#endif
		return;
	}

	if (Valid())
		DetachTextures(false);

#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& textureContext = rendering::OriginalNativeTextureContext();
	if (dtor && textureContext.OwnsGeometry(this)) textureContext.UnregisterGeometry(this);
#endif

	dead = true;
}

void GL::GeometryBuffer::Clear() const {
	RECOIL_DETAILED_TRACY_ZONE;
	assert(bound);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void GL::GeometryBuffer::SetDepthRange(float nearDepth, float farDepth) const {
	RECOIL_DETAILED_TRACY_ZONE;
	#if 0
	if (globalRendering->supportClipSpaceControl) {
		// TODO: need to inform shaders about this, modify PM instead
		glDepthRangef(nearDepth, farDepth);
		glClearDepth(farDepth);
		glDepthFunc((nearDepth <= farDepth)? GL_LEQUAL: GL_GREATER);
	}
	#else
	glClearDepth(std::max(nearDepth, farDepth));
	glDepthFunc(GL_LEQUAL);
	#endif
}

void GL::GeometryBuffer::DetachTextures(const bool init) {
	RECOIL_DETAILED_TRACY_ZONE;
	// nothing to detach yet during init
	if (init)
		return;

#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& textureContext = rendering::OriginalNativeTextureContext();
	if (textureContext.OwnsGeometry(this)) {
		textureContext.DetachGeometry(this, init);
		memset(&bufferTextureIDs[0], 0, sizeof(bufferTextureIDs));
		memset(&bufferAttachments[0], 0, sizeof(bufferAttachments));
		return;
	}
#endif

	buffer.Bind();

	// detach only actually attached textures, ATI drivers might crash
	for (unsigned int i = 0; i < (ATTACHMENT_COUNT - 1); ++i) {
		if (bufferTextureIDs[i] == 0)
			continue;

		buffer.Detach(GL_COLOR_ATTACHMENT0_EXT + i);
	}

	buffer.Detach(GL_DEPTH_ATTACHMENT_EXT);
	buffer.Unbind();

	glDeleteTextures(ATTACHMENT_COUNT, &bufferTextureIDs[0]);

	// return to incomplete state
	memset(&bufferTextureIDs[0], 0, sizeof(bufferTextureIDs));
	memset(&bufferAttachments[0], 0, sizeof(bufferAttachments));
}

void GL::GeometryBuffer::DrawDebug(const unsigned int texID, const float2 texMins, const float2 texMaxs) const {
	RECOIL_DETAILED_TRACY_ZONE;
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glActiveTexture(GL_TEXTURE0);
	glEnable(GetTextureTarget());
	glBindTexture(GetTextureTarget(), texID);
	glBegin(GL_QUADS);
	glTexCoord2f(texMins.x, texMins.y); glNormal3fv(&UpVector.x); glVertex2f(texMins.x, texMins.y);
	glTexCoord2f(texMaxs.x, texMins.y); glNormal3fv(&UpVector.x); glVertex2f(texMaxs.x, texMins.y);
	glTexCoord2f(texMaxs.x, texMaxs.y); glNormal3fv(&UpVector.x); glVertex2f(texMaxs.x, texMaxs.y);
	glTexCoord2f(texMins.x, texMaxs.y); glNormal3fv(&UpVector.x); glVertex2f(texMins.x, texMaxs.y);
	glEnd();
	glBindTexture(GetTextureTarget(), 0);
	glDisable(GetTextureTarget());

	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

bool GL::GeometryBuffer::Create(const int2 size) {
	RECOIL_DETAILED_TRACY_ZONE;
	const unsigned int texTarget = GetTextureTarget();
#ifdef RECOIL_METAL4_LIVE_SCENE
	auto& textureContext = rendering::OriginalNativeTextureContext();
	if (textureContext.OwnsGeometry(this)) {
		if (size.x < 0 || size.y < 0) throw std::runtime_error("Native GBuffer observed a negative original extent");
		const auto target = msaa ? rendering::NativeCUSTextureTarget::Image2DMultisample : rendering::NativeCUSTextureTarget::Image2D;
		const bool complete = textureContext.CreateGeometry(this, size.x, size.y, target, msaa ? globalRendering->msaaLevel : 1, hasColorTex);
		for (unsigned n = 0; n < ATTACHMENT_COUNT; ++n) {
			if (n == ATTACHMENT_COLORTEX && !hasColorTex) continue;
			// The original engine reserves optional color at slot 5; native
			// allocation keeps depth at native5 and appends optional color at6.
			const unsigned nativeAttachment = n == ATTACHMENT_ZVALTEX
				? rendering::NativeGeometryTextureNames::DepthAttachment : n == ATTACHMENT_COLORTEX
				? rendering::NativeGeometryTextureNames::CombinedColorAttachment : n;
			const auto logicalName = textureContext.GeometryName(this, nativeAttachment);
			if (logicalName > std::numeric_limits<GLuint>::max()) throw std::runtime_error("Original GBuffer texture name exceeds GLuint");
			bufferTextureIDs[n] = static_cast<GLuint>(logicalName);
			bufferAttachments[n] = n == ATTACHMENT_ZVALTEX ? GL_DEPTH_ATTACHMENT_EXT : GL_COLOR_ATTACHMENT0_EXT + n;
		}
		return complete;
	}
#endif

	for (unsigned int n = 0; n < ATTACHMENT_COUNT; n++) {
		if (n == ATTACHMENT_COLORTEX && !hasColorTex)
			continue;

		glGenTextures(1, &bufferTextureIDs[n]);
		glBindTexture(texTarget, bufferTextureIDs[n]);

		glTexParameteri(texTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(texTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(texTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(texTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		if (n == ATTACHMENT_ZVALTEX) {
			glTexParameteri(texTarget, GL_DEPTH_TEXTURE_MODE, GL_LUMINANCE);

			if (texTarget == GL_TEXTURE_2D)
				glTexImage2D(texTarget, 0, GL_DEPTH_COMPONENT32F, size.x, size.y, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
			else
				glTexImage2DMultisample(texTarget, globalRendering->msaaLevel, GL_DEPTH_COMPONENT32F, size.x, size.y, GL_TRUE);

			bufferAttachments[n] = GL_DEPTH_ATTACHMENT_EXT;
		} else {
			if (texTarget == GL_TEXTURE_2D)
				glTexImage2D(texTarget, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			else
				glTexImage2DMultisample(texTarget, globalRendering->msaaLevel, GL_RGBA8, size.x, size.y, GL_TRUE);

			bufferAttachments[n] = GL_COLOR_ATTACHMENT0_EXT + n;
		}
	}

	// sic; Mesa complains about an incomplete FBO if calling Bind before TexImage (?)
	buffer.Bind();
	buffer.AttachTextures(bufferTextureIDs, bufferAttachments, texTarget, ATTACHMENT_COUNT);

	glBindTexture(GetTextureTarget(), 0);
	// define the attachments we are going to draw into
	// note: the depth-texture attachment does not count
	// here and will be GL_NONE implicitly, same as any
	// skipped attachment slot!
	glDrawBuffers(ATTACHMENT_COUNT - 1, &bufferAttachments[0]);

	// FBO must have been valid from point of construction
	// if we reached CreateGeometryBuffer, but CheckStatus
	// can still invalidate it
	assert(buffer.IsValid());

	const bool ret = buffer.CheckStatus(name);

	buffer.Unbind();
	return ret;
}

bool GL::GeometryBuffer::Update(const bool init) {
	RECOIL_DETAILED_TRACY_ZONE;
	currBufferSize = GetWantedSize(true);

	// FBO must be valid from point of construction
	if (!Valid())
		return false;

	// buffer isn't bound by calling context, can not call
	// GetStatus to check for GL_FRAMEBUFFER_COMPLETE_EXT
	//
	if (HasAttachments()) {
		// technically a buffer can not be complete yet during
		// initialization, however the GL spec says that FBO's
		// with only empty attachments are complete by default
		// assert(!init);

		// FBO was already initialized (during init or from Lua)
		// so it will have attachments -> check if they need to
		// be regenerated, eg. if a window resize event happened
		if (prevBufferSize == currBufferSize)
			return true;

		DetachTextures(init);
	}

	return (Create(prevBufferSize = currBufferSize));
}

bool GL::GeometryBuffer::Valid() const {
#ifdef RECOIL_METAL4_LIVE_SCENE
	const auto& textureContext = rendering::OriginalNativeTextureContext();
	if (textureContext.OwnsGeometry(this)) return textureContext.GeometryValid(this);
#endif
	return buffer.IsValid();
}

int2 GL::GeometryBuffer::GetWantedSize(bool allowed) const {
	RECOIL_DETAILED_TRACY_ZONE;
	return {globalRendering->viewSizeX * allowed, globalRendering->viewSizeY * allowed};
}

void GL::GeometryBuffer::LoadViewport()
{
	RECOIL_DETAILED_TRACY_ZONE;
	glViewport(0, 0, globalRendering->viewSizeX, globalRendering->viewSizeY);
}
