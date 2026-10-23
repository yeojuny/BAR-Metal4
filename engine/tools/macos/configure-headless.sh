#!/bin/sh
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html
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
cmake -S "$recoil_source" -B "$1" -G Ninja \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DCMAKE_OSX_ARCHITECTURES=arm64 \
	-DCMAKE_OSX_SYSROOT="$recoil_sdk" \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DCMAKE_PREFIX_PATH="${RECOIL_DEPENDENCY_PREFIX:-/opt/homebrew}" \
	-DBUILD_spring-legacy=OFF -DBUILD_spring-headless=ON \
	-DBUILD_spring-dedicated=OFF -DAI_TYPES=NONE -DNO_SOUND=ON \
	-DMACOSX_BUNDLE=OFF -DENABLE_STREFLOP=ON -DSYNCCHECK=ON
