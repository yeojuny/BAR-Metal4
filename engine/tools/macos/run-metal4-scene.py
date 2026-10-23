#!/usr/bin/env python3
"""Run a bounded, isolated original BAR match in the experimental native scene.

This runner records presentation evidence, not gameplay or synchronization acceptance.
It never adds simulation debug commands or changes downloaded game content.
"""
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import time


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--seconds", type=int, default=90)
    parser.add_argument("--capture-png", action="store_true",
                        help="Copy one actual presented frame to a PNG for visual inspection")
    parser.add_argument("--capture-frame", type=int, default=150,
                        help="Earliest simulation frame for --capture-png, after a live unit exists")
    parser.add_argument("--map-start-positions", action="store_true",
                        help="Use the original map's fixed start positions instead of the synctest's corner coordinates")
    parser.add_argument("--input-probe", action="store_true",
                        help="Explicitly exercise camera, selection and movement through queued SDL3 input")
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    build, data, run = (p.resolve() for p in (args.build_dir, args.data_dir, args.run_dir))
    binary = build / "spring-metal4-scene"
    if not binary.is_file() or not data.is_dir():
        parser.error("built spring-metal4-scene and downloaded content directory are required")
    if args.seconds <= 0 or args.seconds > 600:
        parser.error("--seconds must be within 1..600")
    if args.capture_frame < 0 or args.capture_frame > 1_000_000:
        parser.error("--capture-frame must be within 0..1000000")
    if args.input_probe and not args.map_start_positions:
        parser.error("--input-probe requires --map-start-positions")
    if run.exists() and any(run.iterdir()):
        parser.error("--run-dir must be new or empty")
    workflow = (source / ".github/workflows/synctest.yml").read_text()
    game = re.search(r'^  GAME: "([^"]+)"$', workflow, re.MULTILINE).group(1)
    map_name = re.search(r'^  MAP: "([^"]+)"$', workflow, re.MULTILINE).group(1)
    template = source / "test/synctest/synctest-startscript.txt"
    script_text = template.read_text().replace("@VERSION@", game.rsplit(" ", 1)[1]).replace("@MAPNAME@", map_name)
    # Reuse only the official fixed match setup; remove its synthetic collector,
    # cheats, visibility override, UI disabling, speed and scripted quit block.
    script_text, removed = re.subn(r"\[modoptions\]\s*\{[^{}]*\}", "", script_text, flags=re.IGNORECASE)
    if removed != 1 or "debugcommands" in script_text.lower():
        parser.error("official template structure changed; inspect it before running")
    if args.map_start_positions:
        script_text, count = re.subn(r"StartPosType\s*=\s*3\s*;", "StartPosType = 0;", script_text, flags=re.IGNORECASE)
        if count != 1:
            parser.error("official template's start-position mode changed; inspect it before running")
        script_text, count = re.subn(r"^\s*StartPos[XZ]\s*=\s*[^;]+;\s*$", "", script_text,
                                    flags=re.MULTILINE | re.IGNORECASE)
        if count != 4:
            parser.error("official template's two team coordinates changed; inspect it before running")
    run.mkdir(parents=True, exist_ok=True)
    shutil.copytree(build / "base", run / "base")
    for kind, name, info, library in (
        ("Interfaces", "C", "InterfaceInfo.lua", "libAIInterface.dylib"),
        ("Skirmish", "NullAI", "AIInfo.lua", "libSkirmishAI.dylib"),
    ):
        relative = Path("AI") / kind / name
        version = (source / relative / "VERSION").read_text().strip()
        destination = run / relative / version
        destination.mkdir(parents=True)
        shutil.copy2(source / relative / "data" / info, destination / info)
        shutil.copy2(build / relative / "data" / library, destination / library)
        options = source / relative / "data/AIOptions.lua"
        if options.exists():
            shutil.copy2(options, destination / options.name)
    script = run / "startscript.txt"
    script.write_text(script_text)
    config = run / "springsettings.cfg"
    config.write_text("Fullscreen = 0\nWindowBorderless = 0\nXResolution = 1280\nYResolution = 720\n"
                      "XResolutionWindowed = 1280\nYResolutionWindowed = 720\nWorkerThreadCount = 2\n")
    command = [str(binary), "--isolation", "--isolation-dir", str(data), "--write-dir", str(run),
               "--config", str(config), "--only-local", str(script)]
    env = os.environ.copy()
    for key in list(env):
        if key in ("SPRING_DATADIR", "SPRING_WRITEDIR", "RECOIL_METAL4_SCENE_PNG",
                   "RECOIL_METAL4_SCENE_CAPTURE_FRAME", "RECOIL_METAL4_SCENE_INPUT_PROBE") or key.startswith("DYLD_"):
            env.pop(key)
    env.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1")
    if args.capture_png:
        env.update(RECOIL_METAL4_SCENE_PNG=str(run / "native-scene.png"),
                   RECOIL_METAL4_SCENE_CAPTURE_FRAME=str(args.capture_frame))
    if args.input_probe:
        env["RECOIL_METAL4_SCENE_INPUT_PROBE"] = "1"
    metadata = {
        "status": "running", "command": command, "game": game, "map": map_name,
        "platform": platform.platform(), "machine": platform.machine(),
        "engine_sha256": sha256(binary), "template_sha256": sha256(template),
        "startscript_sha256": sha256(script), "initial_config_sha256": sha256(config),
        "duration_limit_seconds": args.seconds, "explicit_render_pixels": [1280, 720],
        "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip(),
        "validation": "Metal API and shader validation enabled",
        "gameplay_acceptance": "not established by first-frame presentation",
        "cross_platform_sync": "not tested by this runner",
        "capture_png_requested": args.capture_png,
        "capture_minimum_frame": args.capture_frame if args.capture_png else None,
        "start_positions": "original map fixed positions" if args.map_start_positions else "official synctest coordinates",
        "input_probe_requested": args.input_probe,
    }
    report = run / "run.json"
    start = time.monotonic()
    with (run / "process.log").open("wb") as log:
        process = subprocess.Popen(command, cwd=run, env=env, stdout=log, stderr=subprocess.STDOUT)
        metadata["pid"] = process.pid
        report.write_text(json.dumps(metadata, indent=2) + "\n")
        try:
            process.wait(timeout=args.seconds)
            metadata["stopped_at_limit"] = False
        except subprocess.TimeoutExpired:
            metadata["stopped_at_limit"] = True
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        metadata["exit_code"] = process.returncode
    log_text = (run / "process.log").read_text(errors="replace")
    metadata["first_presentation"] = next((line for line in log_text.splitlines()
                                           if "[Metal4Scene] First presented" in line), None)
    metadata["first_units"] = next((line for line in log_text.splitlines()
                                    if "[Metal4Scene] First presented units" in line), None)
    metadata["native_gpu_objects"] = next((line for line in log_text.splitlines()
                                           if "[Metal4Scene] Native GPU objects" in line), None)
    metadata["resource_summary"] = next((line for line in log_text.splitlines()
                                         if "[Metal4Scene] First-frame resident" in line), None)
    metadata["snapshot"] = next((line for line in log_text.splitlines()
                                 if "[Metal4Scene] Saved actual presented frame" in line), None)
    metadata["presentation_summary"] = next((line for line in log_text.splitlines()
                                             if "[Metal4Scene] Retired world after" in line), None)
    metadata["input_probe_passed"] = "[Metal4InputProbe] PASSED" in log_text
    metadata["input_probe_failed"] = "[Metal4InputProbe] FAILED" in log_text
    metadata["elapsed_seconds"] = time.monotonic() - start
    metadata["status"] = "presentation_observed" if metadata["first_presentation"] else "no_presentation"
    report.write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({key: metadata[key] for key in ("status", "exit_code", "elapsed_seconds", "first_presentation",
                                                   "snapshot", "presentation_summary", "input_probe_passed", "input_probe_failed")}))
    accepted = (metadata["exit_code"] == 0 and metadata["first_presentation"] and (not args.capture_png or metadata["snapshot"]) and
                (not args.input_probe or (metadata["input_probe_passed"] and not metadata["input_probe_failed"])))
    return 0 if accepted else 1


if __name__ == "__main__":
    raise SystemExit(main())
