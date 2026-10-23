# Place executables and shared libs under "build-dir/",
# instead of under "build-dir/rts/"
# This way, we have the build-dir structure more like the install-dir one,
# which makes testing spring in the builddir easier, eg. like this:
# cd build-dir
# SPRING_DATADIR=$(pwd) ./spring
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

add_definitions(-DHEADLESS)
add_definitions(-DNO_SOUND)
remove_definitions(-DAVI_CAPTURING)

set(OpenGL_GL_PREFERENCE LEGACY)

find_package(OpenGL 3.0 REQUIRED)


# headlessstubs are our stubs that replace libGL, libGLU, libGLEW, libSDL (yes really!)
list(APPEND engineHeadlessLibraries headlessStubs)
list(APPEND engineHeadlessLibraries ${SPRING_SIM_LIBRARIES})
list(APPEND engineHeadlessLibraries engineSystemNet)
list(APPEND engineHeadlessLibraries ${engineCommonLibraries})
#list(APPEND engineHeadlessLibraries engineaGui)
list(APPEND engineHeadlessLibraries no-sound)
list(APPEND engineHeadlessLibraries engineSim)
list(APPEND engineHeadlessLibraries squish)
list(APPEND engineHeadlessLibraries pr-downloader)
list(APPEND engineHeadlessLibraries RmlUi::Core)
list(APPEND engineHeadlessLibraries RmlUi::Debugger)
list(APPEND engineHeadlessLibraries lunasvg)

include_directories(${ENGINE_SRC_ROOT_DIR}/lib/assimp/include)
include_directories(${ENGINE_SRC_ROOT_DIR}/lib/asio/include)
include_directories(${ENGINE_SRC_ROOT_DIR}/lib/slimsig/include)
include_directories(${ENGINE_SRC_ROOT_DIR}/lib/cereal/include)

# Build the shared game sources with this directory's headless definitions.
# The interactive Game target may be disabled in a headless-only build.
add_library(GameHeadless STATIC ${sources_engine_Game})
add_dependencies(GameHeadless generateVersionFiles)
target_link_libraries(GameHeadless PRIVATE
	Tracy::TracyClient
	headlessStubs
	RmlUi::Core
	prd::jsoncpp
	streflop
	nowide::nowide
	fmt::fmt
)
### Build the executable
add_executable(engine-headless ${engineSources} ${ENGINE_ICON})
target_link_libraries(engine-headless no-sound ${engineHeadlessLibraries} GameHeadless no-sound)

# Optional engine asset diagnostic, before normal headless/game startup. It uses
# the real CPU model/texture preparation and the shared native Metal 4 backend.
option(BUILD_METAL4_ASSET_DIAGNOSTIC "Enable engine-owned Metal 4 asset diagnostic on macOS" OFF)
if(BUILD_METAL4_ASSET_DIAGNOSTIC)
	if(NOT APPLE)
		message(FATAL_ERROR "The Metal 4 asset diagnostic requires macOS")
	endif()
	if(NOT TARGET recoil-metal4-renderer)
		add_subdirectory("${ENGINE_SRC_ROOT_DIR}/Rendering/Metal4" "${CMAKE_CURRENT_BINARY_DIR}/metal4-renderer")
	endif()
	target_sources(engine-headless PRIVATE "${ENGINE_SRC_ROOT_DIR}/Game/Metal4AssetController.cpp")
	set_property(SOURCE "${ENGINE_SRC_ROOT_DIR}/System/SpringApp.cpp" APPEND PROPERTY
		COMPILE_DEFINITIONS RECOIL_METAL4_ASSET_DIAGNOSTIC)
	target_link_libraries(engine-headless recoil-metal4-renderer prd::jsoncpp)
endif()

# Export symbols for plugin access (replaces CMP0065 OLD behavior)
set_target_properties(engine-headless PROPERTIES ENABLE_EXPORTS TRUE)

if(MSVC)
    target_link_libraries(engine-headless engine-natvis)
endif()

if    (MINGW)
	# To enable console output/force a console window to open
	set_target_properties(engine-headless PROPERTIES LINK_FLAGS "-Wl,-subsystem,console")
endif (MINGW)


### Install the executable
install(TARGETS engine-headless DESTINATION ${BINDIR})

# Only build & install spring-headless executable & dependencies
# use cases:
# * make spring-headless
# * make install-spring-headless
create_engine_build_and_install_target(headless)
