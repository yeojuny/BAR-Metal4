/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef _SOUND_H_
#define _SOUND_H_

#include <atomic>
#include <string>
#include <vector>
#ifdef RECOIL_USE_SDL3
#include <array>
#include "System/SDL3Compat/SDL3Helpers.h"
#else
#include <SDL.h>
#endif
#include <al.h>
#include <alc.h>

#include "System/Sound/ISound.h"
#include "System/float3.h"
#include "System/UnorderedMap.hpp"
#include "System/UnorderedSet.hpp"
#include "System/Threading/SpringThreading.h"

#include "SoundItem.h"

class CSoundSource;
class SoundBuffer;
class SoundItem;

/// Default sound system implementation (OpenAL)
class CSound : public ISound
{
public:
	CSound();
	~CSound();

	void Init() override;
	void Kill() override;

	bool HasSoundItem(const std::string& name) const override;
	bool PreloadSoundItem(const std::string& name) override;
	size_t GetDefSoundId(const std::string& name) override;
	size_t GetSoundId(const std::string& name) override;

	SoundItem* GetSoundItem(size_t id) override;
	CSoundSource* GetNextBestSource(bool lock = true) override;

	void NewFrame() override;
	void UpdateListener(const float3& campos, const float3& camdir, const float3& camup) override {
		myPos  = campos;
		camDir = camdir;
		camUp  = camup;

		// schedule UpdateListenerReal
		updateListener = true;
	}

	/// @see ConfigHandler::ConfigNotifyCallback
	void ConfigNotify(const std::string& key, const std::string& value) override;
	void PitchAdjust(const float newPitch) override;

	bool Mute() override;
	bool IsMuted() const override { return mute; }

	void DeviceChanged(uint32_t sdlDeviceIndex, bool added) override;

	void Iconified(bool state) override;

	void PrintDebugInfo() override;

	bool SoundThreadQuit() const override { return soundThreadQuit; }
	bool CanLoadSoundDefs() const override { return canLoadDefs; }

	bool LoadSoundDefsImpl(LuaParser* defsParser) override;
	const float3& GetListenerPos() const override { return myPos; }

	ALCdevice* GetCurrentDevice() { return curDevice; }
	int GetFrameSize() const { return frameSize; }

	std::vector<std::string> GetSoundDevices() override;
private:
	typedef spring::unordered_map<std::string, std::string> SoundItemNameMap;
	typedef spring::unordered_map<std::string, SoundItemNameMap> SoundItemDefsMap;

private:
	void Cleanup();
	void OpenOpenALDevice(const std::string& deviceName);
	bool OpenSdlDevice(const std::string& deviceName, SDL_AudioSpec& obtainedSpec);
#ifdef RECOIL_USE_SDL3
	void CloseSdlStream();
	static void SDLCALL RenderSDL3Samples(void* userdata, SDL_AudioStream* stream, int additionalBytes, int totalBytes);
#endif
	void OpenLoopbackDevice(const std::string& deviceName);

	void InitThread(int cfgMaxSounds);
	void UpdateThread(int cfgMaxSounds);

	void Update();
	void UpdateListenerReal();

	int GetMaxMonoSources(ALCdevice* device, int cfgMaxSounds);
	void GenSources(int alMaxSounds);

	size_t MakeItemFromDef(const SoundItemNameMap& itemDef);
	size_t LoadSoundBuffer(const std::string& filename);

private:
	ALCdevice* curDevice = nullptr;
	ALCcontext* curContext = nullptr;
#ifdef RECOIL_USE_SDL3
	SDL_AudioDeviceID sdlDeviceID = 0; // Owned logical device; stream closes it.
	SDL_AudioDeviceID sdlPhysicalDeviceID = 0; // Explicit selection, not logical ID.
	SDL_AudioStream* sdlStream = nullptr;
	SDL_AudioSpec sdlInputSpec{}; // Stable OpenAL loopback PCM, not hardware spec.
	bool sdlAudioInitialized = false; // Main-thread Init/Kill own one SDL audio subsystem reference.
	bool sdlLoopbackReady = false; // Separate from extension availability.
	std::array<int16_t, 4096 * 2> sdlMixBuffer{}; // Aligned S16 stereo chunks.
	std::atomic<uint32_t> sdlCallbackFault = {0};
#else
	int sdlDeviceID = -1;
#endif
	bool hasAlcSoftLoopBack = false;

	std::string selectedDeviceName = "";

	spring::thread soundThread;
	spring::unordered_map<std::string, size_t> soundMap; // <name, id>
	spring::unordered_set<std::string> preloadSet;
	spring::unordered_set<std::string> failureSet;

	std::vector<SoundItem> soundItems;
	std::vector<CSoundSource> soundSources; // fixed-size

	std::vector<std::uint8_t> loadBuffer;

	SoundItemNameMap defaultItemNameMap;
	SoundItemDefsMap soundItemDefsMap; // parsed from sounds.lua

	float masterVolume = 0.0f;

	/// unscaled
	float3 myPos;
	float3 camDir;
	float3 camUp;
	float3 prevVelocity;

	int pitchAdjustMode = 0;
	int frameSize = -1;

	bool mute = false;

	/// we do not play if minimized / iconified
	bool appIsIconified = false;

	std::atomic<bool> updateListener = {false};
	std::atomic<bool> soundThreadQuit = {false};
	std::atomic<bool> canLoadDefs = {false};
};

#endif // _SOUND_H_
