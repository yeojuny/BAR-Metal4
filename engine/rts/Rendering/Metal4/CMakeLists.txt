# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
if(NOT APPLE)
	message(FATAL_ERROR "The direct Metal 4 backend requires macOS")
endif()

enable_language(OBJCXX)
add_library(recoil-metal4-renderer STATIC
	Metal4RuntimeNative.mm
	Metal4ScreenNative.mm
	Metal4PiPNative.mm
	Metal4PanelShader.mm
	../Backend/NativePanelShaderData.cpp
	../Backend/NativePiPData.cpp
	Metal4PostprocessNative.mm
	../Backend/NativePostprocessData.cpp
	../Backend/NativePostprocessRecorder.cpp
	Metal4MiniMapNative.mm
	Metal4MetalInfo.mm
	../Backend/NativeWorldInfoRecorder.cpp
	Metal4WorldInfo.mm
	../Backend/NativeMetalInfoRecorder.cpp
	../Backend/NativeCUSMaterialControl.cpp
	../Backend/ScreenDrawList.cpp
	../Backend/NativeScreenLine.cpp
)
# Development executors are not compiled into the normal renderer archive.
option(BUILD_METAL4_DEVELOPMENT_TOOLS "Build standalone Metal4 diagnostics" OFF)
if(BUILD_METAL4_DEVELOPMENT_TOOLS OR BUILD_METAL4_ASSET_DIAGNOSTIC)
	target_sources(recoil-metal4-renderer PRIVATE
		Metal4RuntimeDiagnostics.mm Metal4AssetRenderer.mm Metal4ScreenRenderer.mm)
endif()
set_target_properties(recoil-metal4-renderer PROPERTIES
	CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
	OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
target_compile_options(recoil-metal4-renderer PRIVATE
	-Wall -Wextra -ffp-contract=off
	$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>
	$<$<COMPILE_LANGUAGE:OBJCXX>:-fblocks>)
foreach(framework Foundation Metal)
	find_library(RECOIL_METAL4_${framework} ${framework} REQUIRED)
	target_link_libraries(recoil-metal4-renderer PUBLIC ${RECOIL_METAL4_${framework}})
endforeach()

option(BUILD_METAL4_WINDOW_SURFACE "Build the engine-owned SDL Metal window surface" OFF)
if(BUILD_METAL4_WINDOW_SURFACE)
	if(NOT TARGET SDL3::SDL3)
		find_package(SDL3 CONFIG REQUIRED)
	endif()
	add_library(recoil-metal4-surface STATIC Metal4WindowSurface.mm
		../../System/Platform/Mac/MacSDL3WindowGeometry.mm)
	set_target_properties(recoil-metal4-surface PROPERTIES
		CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
		OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
	target_compile_options(recoil-metal4-surface PRIVATE -Wall -Wextra -fobjc-arc -fblocks)
	target_link_libraries(recoil-metal4-surface PUBLIC SDL3::SDL3)
	foreach(framework Foundation AppKit CoreGraphics QuartzCore Metal)
		find_library(RECOIL_SURFACE_${framework} ${framework} REQUIRED)
		target_link_libraries(recoil-metal4-surface PUBLIC ${RECOIL_SURFACE_${framework}})
	endforeach()
	if(BUILD_METAL4_DEVELOPMENT_TOOLS)
		add_executable(recoil-metal4-surface-state-tests Metal4SurfaceStateTests.cpp)
		set_target_properties(recoil-metal4-surface-state-tests PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES)
		target_compile_definitions(recoil-metal4-surface-state-tests PRIVATE METAL4_SURFACE_STATE_TEST_MAIN)
	endif()
endif()
