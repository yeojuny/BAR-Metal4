cmake_minimum_required(VERSION 3.14)
project(streflop-float-test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- STREFLOP mode selection ---
# Override with -DSTREFLOP_MODE=SSE|NEON|SOFT|X87 (default: auto-detect)
if(NOT DEFINED STREFLOP_MODE)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|armv8")
		set(STREFLOP_MODE "NEON")
	else()
		set(STREFLOP_MODE "SSE")
	endif()
endif()

string(TOUPPER "${STREFLOP_MODE}" STREFLOP_MODE)

if(NOT STREFLOP_MODE MATCHES "^(SSE|NEON|SOFT|X87)$")
	message(FATAL_ERROR "Invalid STREFLOP_MODE '${STREFLOP_MODE}'. Must be SSE, NEON, SOFT, or X87.")
endif()

message(STATUS "STREFLOP mode: ${STREFLOP_MODE}")
message(STATUS "Architecture:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# --- Locate streflop source ---
set(STREFLOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../rts/lib/streflop")
if(NOT EXISTS "${STREFLOP_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "Cannot find streflop at ${STREFLOP_DIR}")
endif()

# --- Compiler flags ---
# Match the engine's compiler flags (from root CMakeLists.txt + TestCXXFlags.cmake)
# to ensure STREFLOP results are identical to in-engine behavior.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
	# FMA fusion prevention — critical for cross-arch reproducibility
	add_compile_options(-ffp-contract=off)
	# Strict aliasing and IEEE compliance
	add_compile_options(-fno-strict-aliasing)

	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|armv8")
		add_compile_options(-march=armv8-a+simd)
	else()
		# x86: SSE instructions available, but FPU math mode varies by STREFLOP mode
		add_compile_options(-msse -msse2)
		if(STREFLOP_MODE STREQUAL "X87")
			# X87 mode: use 387 FPU to match streflop library
			# Force IEEE 754 compliance - store to memory after each FPU operation
			add_compile_options(-mfpmath=387 -ffloat-store)
		else()
			# SSE/SOFT/NEON: use SSE for FPU math
			add_compile_options(-mfpmath=sse)
		endif()
		# Restrict to SSE2 baseline
		# However my tests (ivand, 14/03/2026) show that removing these next two lines is not harmful for the sync
		add_compile_options(-mno-sse3 -mno-ssse3 -mno-sse4.1 -mno-sse4.2 -mno-sse4 -mno-sse4a)
		add_compile_options(-mno-avx -mno-fma -mno-fma4 -mno-xop -mno-lwp -mno-avx2)
	endif()

	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		# GCC-specific flags the engine uses
		add_compile_options(-fsingle-precision-constant -frounding-math)
		include(CheckCXXCompilerFlag)
		check_cxx_compiler_flag("-mieee-fp" HAS_IEEE_FP)
		if(HAS_IEEE_FP)
			add_compile_options(-mieee-fp)
		endif()
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
		# Clang: -frounding-math if supported
		include(CheckCXXCompilerFlag)
		check_cxx_compiler_flag("-frounding-math" HAS_FROUNDING_MATH)
		if(HAS_FROUNDING_MATH)
			add_compile_options(-frounding-math)
		endif()
	endif()
endif()

# --- Build streflop library from source ---
# Select the mode before streflop chooses its source files and compiler flags.
# Reset all mode options so reconfiguring an existing build directory is safe.
# Leave STREFLOP_NO_DENORMALS to the upstream option and the caller's choice.
set(STREFLOP_AUTO OFF CACHE BOOL "Autodetect the most appropriate Streflop implementation" FORCE)
foreach(_mode SSE NEON SOFT)
	if("${STREFLOP_MODE}" STREQUAL "${_mode}")
		set(_enabled ON)
	else()
		set(_enabled OFF)
	endif()
	set(STREFLOP_${_mode} ${_enabled} CACHE BOOL "Use ${_mode} implementation" FORCE)
endforeach()

# The upstream target supplies the matching libm sources and, for SOFT mode,
# exactly one wrapper for each precision plus the SoftFloat implementation.
add_subdirectory("${STREFLOP_DIR}" "${CMAKE_BINARY_DIR}/streflop")

# Suppress upstream SoftFloat operator-precedence warnings (pre-existing, harmless)
target_compile_options(streflop PRIVATE -Wno-parentheses)

# --- Test binary ---
add_executable(streflop-float-test streflop_float_test.cpp)
target_link_libraries(streflop-float-test PRIVATE streflop)
target_include_directories(streflop-float-test PRIVATE "${STREFLOP_DIR}")

# Pass mode and compiler info as defines for runtime reporting
target_compile_definitions(streflop-float-test PRIVATE
	"SFTEST_MODE=\"STREFLOP_${STREFLOP_MODE}\""
	"SFTEST_COMPILER=\"${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\""
)
