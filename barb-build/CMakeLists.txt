cmake_minimum_required(VERSION 3.24)
project(BARb220Standalone LANGUAGES C CXX)
set(RECOIL_SOURCE "" CACHE PATH "Extracted engine source directory")
if(NOT EXISTS "${RECOIL_SOURCE}/rts")
  message(FATAL_ERROR "Set RECOIL_SOURCE to the extracted engine directory")
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
add_compile_definitions(BUILDING_AI SSE2NEON SSE2NEON_SUPPRESS_WARNINGS _GNU_SOURCE=1 _REENTRANT ASIO_STANDALONE MACOSX_BUNDLE RECOIL_USE_SDL3=1 REPORT_LUANAN SDL_ENABLE_OLD_NAMES=1 SPRING_DATADIR="" SYNCCHECK SYNC_HISTORY THREADPOOL _GLIBCXX_USE_NANOSLEEP)
add_compile_options(-Wformat -Wformat-security "$<$<COMPILE_LANGUAGE:CXX>:-ffp-contract=off>" "$<$<COMPILE_LANGUAGE:CXX>:-frounding-math>" "$<$<COMPILE_LANGUAGE:CXX>:-fno-strict-aliasing>" "$<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>" "$<$<COMPILE_LANGUAGE:CXX>:-pthread>")
include_directories(
  "${RECOIL_SOURCE}/rts/System/SDL3Compat"
  "${RECOIL_SOURCE}/rts"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/src-generated/engine"
  "${RECOIL_SOURCE}/rts/ExternalAI/Interface"
  "${RECOIL_SOURCE}/AI/Wrappers"
  "${RECOIL_SOURCE}/rts/lib/sse2neon"
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated"
)
add_library(CUtils STATIC
  "${RECOIL_SOURCE}/AI/Wrappers/CUtils/SharedLibrary.c"
  "${RECOIL_SOURCE}/AI/Wrappers/CUtils/SimpleLog.c"
  "${RECOIL_SOURCE}/AI/Wrappers/CUtils/SimpleProfiler.cpp"
  "${RECOIL_SOURCE}/AI/Wrappers/CUtils/Util.c"
  "${RECOIL_SOURCE}/AI/Wrappers/CUtils/TimeUtil.cpp"
)
add_library(Cpp-AIWrapper STATIC
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src/AIEvent.cpp"
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src/AIFloat3.cpp"
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src/AIColor.cpp"
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src/AIException.cpp"
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src/CallbackAIException.cpp"
  "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src/EventAIException.cpp"
  "${RECOIL_SOURCE}/rts/System/float3.cpp"
  "${RECOIL_SOURCE}/rts/System/SafeCStrings.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractCheats.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractCommand.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractCommandDescription.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractDamage.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractDataDirs.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractEconomy.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractEngine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractFeature.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractFeatureDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractFigure.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractFile.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractFlankingBonus.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractGame.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractGroup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractInfo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractLine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractLog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractLua.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractMap.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractMod.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractMoveData.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractOOAICallback.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractOptionValues.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractOrderPreview.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractPathDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractPathing.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractPoint.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractResource.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractRoots.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractShield.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractSkirmishAI.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractTeam.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractUnit.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractUnitDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractVersion.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractWeaponDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractWeaponMount.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/AbstractWeapon.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubCheats.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubCommand.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubCommandDescription.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubDamage.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubDataDirs.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubEconomy.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubEngine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubFeature.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubFeatureDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubFigure.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubFile.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubFlankingBonus.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubGame.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubGroup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubInfo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubLine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubLog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubLua.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubMap.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubMod.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubMoveData.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubOOAICallback.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubOptionValues.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubOrderPreview.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubPathDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubPathing.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubPoint.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubResource.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubRoots.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubShield.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubSkirmishAI.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubTeam.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubUnit.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubUnitDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubVersion.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubWeaponDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubWeaponMount.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/StubWeapon.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappCheats.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappCurrentCommand.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappDamage.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappDataDirs.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappDebug.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappEconomy.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappEngine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappFeature.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappFeatureDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappFigure.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappFile.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappFlankingBonus.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappGame.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappGraphDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappGraphLine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappGroup.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappGroupSupportedCommand.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappInfo.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappLine.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappLog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappLua.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappMap.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappMod.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappMoveData.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappOOAICallback.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappOptionValues.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappOrderPreview.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappPathDrawer.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappPathing.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappPoint.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappResource.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappRoots.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappShield.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappSkirmishAI.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappTeam.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappUnit.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappUnitDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappUnitSupportedCommand.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappVersion.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappWeaponDef.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappWeaponMount.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/WrappWeapon.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated/CombinedCallbackBridge.c"
)
set_target_properties(Cpp-AIWrapper PROPERTIES OUTPUT_NAME Cpp)
set(Cpp_AIWRAPPER_TARGET Cpp-AIWrapper)
set(BUILD_Cpp_AIWRAPPER TRUE)
set(Cpp_AIWRAPPER_INCLUDE_DIRS "${RECOIL_SOURCE}/AI/Wrappers/Cpp/src" "${CMAKE_CURRENT_SOURCE_DIR}/generated/cpp-generated")
set(CIRCUIT_DEBUG OFF)
set(CIRCUIT_PROFILING OFF)
add_library(Tracy INTERFACE)
add_library(Tracy::TracyClient ALIAS Tracy)
set_property(TARGET Tracy PROPERTY INCLUDE_DIRECTORIES "${RECOIL_SOURCE}/rts/lib/tracy/public")
target_include_directories(Tracy INTERFACE "${RECOIL_SOURCE}/rts/lib/tracy/public")
macro(configure_native_skirmish_ai source_var extra_var flags_var libs_var)
  file(GLOB_RECURSE own_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/${${source_var}}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/${${source_var}}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/${${source_var}}/*.c++"
    "${CMAKE_CURRENT_SOURCE_DIR}/${${source_var}}/*.cxx")
  list(LENGTH own_sources own_count)
  if(NOT own_count EQUAL 218)
    message(FATAL_ERROR "Pinned upstream Circuit source inventory changed")
  endif()
  add_library(BARb MODULE ${own_sources} ${${extra_var}})
  target_compile_definitions(BARb PRIVATE BUILDING_SKIRMISH_AI)
  separate_arguments(upstream_flags NATIVE_COMMAND "${${flags_var}}")
  target_compile_options(BARb PRIVATE ${upstream_flags})
  target_link_libraries(BARb PRIVATE ${${libs_var}})
  set_target_properties(BARb PROPERTIES OUTPUT_NAME SkirmishAI PREFIX lib SUFFIX .dylib)
endmacro()
add_subdirectory(BARb)
