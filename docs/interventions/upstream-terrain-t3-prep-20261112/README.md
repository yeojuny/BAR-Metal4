# Terrain T3 preparation: explicit observation boundaries

AI-assisted source preparation on the isolated upstream branch, based on `e4beaa3eb9856115508635f1147bf8a36bc220ae`. **This does not implement lazy uniform sealing or complete T3.**

## Why this precedes lazy sealing

A draw-only Freeze would miss nested ActiveShader save/restore. Making every internal ownership/kind check seal a snapshot would instead erase the intended saving. `CALLERS.json` and `AUDIT.md` classify actual binding calls, excluding unrelated shadow/texture classes with similarly named methods.

Three APIs now make the distinction explicit while retaining eager publication:

- `CurrentKind()` performs a scalar kind query.
- `InspectCurrent()` borrows the current selection for immediate inspection, without owner increments. Never keep its reference across mutation/callbacks.
- `SaveCurrent()` retains an owning selection for nested state restoration. It currently delegates to the existing owning `Current()`, whose semantics remain unchanged.

Only mechanically safe callers were migrated: Terrain's internal ownership/no-op/teardown checks, UI and distortion kind queries, and the three ActiveShader save points. Capture remains owning. In particular, UnitGlass teardown keeps its owning copy because it unbinds and clears owners during iteration.

## Deleted-writer rejection

A saved selection retains definition and uniform content, not the source writer. Previously a restored saved Terrain selection could pass a deleted writer's provenance pointer to dynamic_cast during capture. Capture now checks the existing live-program registry before dereferencing it, then performs exact publication matching. This rejects deleted source and stale lifetime definitions without changing valid captures. A coincidentally reused address still cannot match the old definition owner. The tests do not force allocator address reuse, so that part is source reasoning rather than an observed allocator event.

## Verification

Run `python3 engine/tools/metal4-contracts/run_native_terrain_program_test.py --output /tmp/terrain-t3-prep-tests` from repository root. `RESULTS.json` records the completed host run. Tests exercise actual source definitions with shader/VFS/consumer stand-ins, nested save/restore and exception flow, no-allocation/refcount inspection and deleted-writer rejection. These are not actual Lua VM integration tests.

The source remains synchronous/eager. No waits, queue depth, source callback/RNG frequency, GPU binding or render math change. Actual Objective-C++ full build, Lua/Metal integration, frame parity and performance remain unverified. Selected MacBook was rechecked and offline; another Mac was not substituted.

## Before implementation of T3 proper

1. Close T2 certificate/source validation migration and run the actual Mac build/Metal acceptance checks.
2. Define how owning Current/SaveCurrent and actual draw capture force sealing, while InspectCurrent/CurrentKind do not.
3. Preserve restored historic uniforms independently of the mutable writer; equal-to-writer writes after Restore may still need to republish.
4. Preserve inactive named values/defaults and failure ordering across reload, import, Release and source/context deletion.
5. Add differential eager-versus-lazy tests at every observation boundary before changing storage. No lazy path is enabled by this preparatory patch.
