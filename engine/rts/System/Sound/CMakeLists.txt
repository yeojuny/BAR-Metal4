
# only one of the two targets needs this
remove_definitions(-DNO_SOUND)

# Dummy Sound implementation
set(noSoundSources
		IAudioChannel.cpp
		ISound.cpp
		Null/SoundChannels.cpp
		Null/NullSound.cpp
	)

add_library(no-sound STATIC EXCLUDE_FROM_ALL ${noSoundSources})
set_target_properties(no-sound PROPERTIES COMPILE_FLAGS "-DNO_SOUND")
target_link_libraries(no-sound Tracy::TracyClient)

# Define default sound implementation
if    (NO_SOUND)
	set(sound-impl no-sound PARENT_SCOPE)
else  (NO_SOUND)
	set(sound-impl sound PARENT_SCOPE)
endif (NO_SOUND)


# Real Sound implementation
if    (NOT NO_SOUND)
	set(soundSources
			ISound.cpp
			IAudioChannel.cpp
			Null/NullSound.cpp
			OpenAL/ALShared.cpp
			OpenAL/EFX.cpp
			OpenAL/EFXfuncs.cpp
			OpenAL/EFXPresets.cpp
			OpenAL/AudioChannel.cpp
			OpenAL/OggDecoder.cpp
			OpenAL/Mp3Decoder.cpp
			OpenAL/MusicStream.cpp
			OpenAL/Sound.cpp
			OpenAL/SoundChannels.cpp
			OpenAL/SoundBuffer.cpp
			OpenAL/SoundItem.cpp
			OpenAL/SoundSource.cpp
			OpenAL/VorbisShared.cpp
		)

	if(APPLE AND RECOIL_USE_SDL3)
		# Apple's deprecated framework lacks the EFX/loopback APIs used here.
		# Keep the SDL3 native lane on the same OpenAL Soft implementation as
		# the original mixer, with its real headers and bundleable library.
		find_package(PkgConfig REQUIRED)
		pkg_check_modules(RECOIL_OPENAL_SOFT REQUIRED IMPORTED_TARGET "openal>=1.18.2")
		set(recoil_sound_openal PkgConfig::RECOIL_OPENAL_SOFT)
	else()
		find_package_static(OpenAL 1.18.2 REQUIRED)
		set(recoil_sound_openal OpenAL::OpenAL)
	endif()
	find_package_static(OggVorbis 1.3.4 REQUIRED)

	if(NOT RECOIL_USE_SDL3)
		find_package(SDL2 MODULE REQUIRED)
	endif()

	include_directories(${CMAKE_SOURCE_DIR}/include/)
	include_directories(${CMAKE_SOURCE_DIR}/include/AL)

	add_library(sound STATIC EXCLUDE_FROM_ALL ${soundSources})
	target_link_libraries(sound ${RECOIL_SDL_TARGET})
	target_link_libraries(sound vorbis::vorbisfile vorbis::vorbis Ogg::ogg)
	target_link_libraries(sound ${recoil_sound_openal})
	target_link_libraries(sound Tracy::TracyClient)
	target_link_libraries(sound nowide::nowide)
endif (NOT NO_SOUND)
