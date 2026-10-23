#!/bin/sh
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
# Experimental direct SDL3 + Metal4 world target. Not full graphics/UI/sound parity.
set -eu
if [ "$#" -ne 1 ]; then
	printf 'Usage: %s /absolute/path/to/build-directory\n' "$0" >&2
	exit 2
fi
case "$1" in
	/*) ;;
	*) printf 'Build directory must be absolute.\n' >&2; exit 2 ;;
esac
recoil_source=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
recoil_sdk=$(xcrun --sdk macosx --show-sdk-path)
recoil_deps=${RECOIL_DEPENDENCY_PREFIX:-/opt/homebrew}
cmake -S "$recoil_source" -B "$1" -G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_SYSROOT="$recoil_sdk" \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_PREFIX_PATH="$recoil_deps;$recoil_deps/opt/openal-soft" \
	-DRECOIL_USE_SDL3=ON -DBUILD_spring-metal4-scene=ON \
	-DBUILD_spring-legacy=OFF -DBUILD_spring-headless=OFF -DBUILD_spring-dedicated=OFF \
	-DBUILD_METAL4_ASSET_DIAGNOSTIC=OFF -DBUILD_METAL4_WINDOW_SURFACE=ON \
	-DAI_TYPES=NATIVE '-DAI_EXCLUDE_REGEX=CircuitAI|CppTestAI|NullJavaAI|NullOOJavaAI' \
	-DNO_SOUND=OFF -DMACOSX_BUNDLE=OFF -DENABLE_STREFLOP=ON -DSYNCCHECK=ON
