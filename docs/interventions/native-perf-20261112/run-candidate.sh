#!/bin/sh
# Separate intervention. Run only a candidate binary you built from this branch.
# Does not replace an app, install packages, alter security, or change defaults.
set -eu
if [ "$#" -lt 1 ] || [ ! -x "$1" ]; then
 echo "Usage: $0 /absolute/path/to/candidate-engine [engine arguments...]" >&2; exit 2
fi
export BAR_NATIVE_INTERVENTION_POOLS=1
export BAR_NATIVE_INTERVENTION_TWO_INFLIGHT=1
export BAR_NATIVE_INTERVENTION_RENDER_WORKER=1
export BAR_NATIVE_INTERVENTION_PERSISTENT_RESIDENCY=1
export BAR_NATIVE_INTERVENTION_PRIVATE_TEXTURES=1
export BAR_NATIVE_INTERVENTION_HOST_VERTEX=1
export BAR_NATIVE_INTERVENTION_EMPTY_PASS_FUSION=1
export BAR_NATIVE_INTERVENTION_MRT_LUA_REUSE=1
export BAR_NATIVE_INTERVENTION_FAST_MATH=1
# Existing native233 options remain opt-in here, not release-wide defaults.
export BAR_NATIVE_RESIDENCY_DIFF220=1
export BAR_NATIVE_RESIDENCY_RELEASE220=1
export BAR_NATIVE_TERRAIN_UNIFORM_RELEASE220=1
export BAR_NATIVE_TERRAIN_PROGRAM_VALIDATION220=1
export BAR_NATIVE_WORLD_PARTICLE_UPLOAD220=1
export BAR_NATIVE_FRAME_OVERLAP=1
export BAR_NATIVE_PREPARE_OVERLAP=1
printf '%s\n' 'Experimental combined profile; graphics/performance unqualified. Not a Homebrew release.' >&2
exec "$@"
