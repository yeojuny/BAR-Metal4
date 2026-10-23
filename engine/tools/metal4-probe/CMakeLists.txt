# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
cmake_minimum_required(VERSION 3.27)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	project(RecoilMetal4Probe LANGUAGES CXX OBJCXX)
else()
	enable_language(OBJCXX)
endif()

if(NOT APPLE)
	message(FATAL_ERROR "The direct Metal 4 diagnostic requires macOS")
endif()

if(NOT TARGET recoil-metal4-renderer)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../rts/Rendering/Metal4" "${CMAKE_CURRENT_BINARY_DIR}/renderer")
endif()

set(METAL4_SHARED_S3O_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../rts/Rendering/Models/S3OData.cpp")
add_executable(recoil-metal4-probe AssetData.cpp Metal4Probe.mm ${METAL4_SHARED_S3O_SOURCE})
target_link_libraries(recoil-metal4-probe PRIVATE recoil-metal4-renderer)
set_target_properties(recoil-metal4-probe PROPERTIES
	CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
	OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
target_compile_options(recoil-metal4-probe PRIVATE
	-Wall -Wextra -ffp-contract=off
	$<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
foreach(framework Foundation Metal ImageIO CoreGraphics)
	find_library(METAL4_${framework}_FRAMEWORK ${framework} REQUIRED)
	target_link_libraries(recoil-metal4-probe PRIVATE ${METAL4_${framework}_FRAMEWORK})
endforeach()

# A renderer feasibility diagnostic, not the engine or an application bundle.
# Runtime capability checks forbid silently selecting a different renderer.

add_executable(recoil-metal4-asset-tests AssetData.cpp AssetDataTests.cpp ${METAL4_SHARED_S3O_SOURCE})
set_target_properties(recoil-metal4-asset-tests PROPERTIES
	CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_options(recoil-metal4-asset-tests PRIVATE -Wall -Wextra -ffp-contract=off)
enable_testing()
add_test(NAME Metal4AssetBoundary COMMAND recoil-metal4-asset-tests
	"${CMAKE_CURRENT_BINARY_DIR}/asset-test-data")

add_executable(recoil-metal4-screen-tests ../metal4-contracts/ScreenDrawListGPUHarness.mm)
set_target_properties(recoil-metal4-screen-tests PROPERTIES
	OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
target_compile_options(recoil-metal4-screen-tests PRIVATE -Wall -Wextra -fobjc-arc -fblocks)
target_link_libraries(recoil-metal4-screen-tests PRIVATE recoil-metal4-renderer
	${METAL4_ImageIO_FRAMEWORK} ${METAL4_CoreGraphics_FRAMEWORK})

add_executable(recoil-metal4-scissor-tests ../../rts/Rendering/Metal4/Metal4ScreenScissorTests.cpp)
set_target_properties(recoil-metal4-scissor-tests PROPERTIES
	CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_definitions(recoil-metal4-scissor-tests PRIVATE METAL4_SCREEN_SCISSOR_TEST_MAIN)
add_test(NAME Metal4ScreenScissor COMMAND recoil-metal4-scissor-tests)
add_executable(recoil-metal4-world-viewport-tests ../../rts/Rendering/Metal4/Metal4WorldViewportTests.cpp)
set_target_properties(recoil-metal4-world-viewport-tests PROPERTIES
	CXX_STANDARD 23 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
target_compile_definitions(recoil-metal4-world-viewport-tests PRIVATE METAL4_WORLD_VIEWPORT_TEST_MAIN)
add_test(NAME Metal4WorldViewport COMMAND recoil-metal4-world-viewport-tests)
# GPU fixture execution is explicit: it requires a native Metal4 device and a
# recorded original BAR FlowUI fixture, neither provided by a generic CTest host.

if(TARGET recoil-metal4-surface)
	add_executable(recoil-metal4-surface-smoke Metal4SurfaceSmoke.mm)
	set_target_properties(recoil-metal4-surface-smoke PROPERTIES
		OBJCXX_STANDARD 23 OBJCXX_STANDARD_REQUIRED YES OBJCXX_EXTENSIONS NO)
	target_compile_options(recoil-metal4-surface-smoke PRIVATE -Wall -Wextra -fobjc-arc -fblocks)
	target_link_libraries(recoil-metal4-surface-smoke PRIVATE recoil-metal4-surface)
	add_test(NAME Metal4SurfaceState COMMAND recoil-metal4-surface-state-tests)
	# The hidden-window smoke is deliberately manual: it needs a logged-in macOS
	# WindowServer and native Metal4 device, unlike the pure CPU state test.
endif()
