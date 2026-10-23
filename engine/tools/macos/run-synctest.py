#!/usr/bin/env python3
"""Run the pinned upstream scenario in a fresh, isolated macOS write directory.

This records one local simulation result. It does not certify cross-platform sync.
Content must already be downloaded with the workflow's exact GAME/MAP pins.
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
    parser.add_argument("--timeout", type=int, default=600)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    build, data, run = (p.resolve() for p in (args.build_dir, args.data_dir, args.run_dir))
    if run.exists() and any(run.iterdir()):
        parser.error("--run-dir must be new or empty; prior hashes must not be reused")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    binary = build / "spring-headless"
    if not binary.is_file() or not data.is_dir():
        parser.error("built spring-headless and downloaded content directory are required")
    workflow = (source / ".github/workflows/synctest.yml").read_text()
    game = re.search(r'^  GAME: "([^"]+)"$', workflow, re.MULTILINE).group(1)
    map_name = re.search(r'^  MAP: "([^"]+)"$', workflow, re.MULTILINE).group(1)
    template = source / "test/synctest/synctest-startscript.txt"
    rendered = template.read_text().replace("@VERSION@", game.rsplit(" ", 1)[1]).replace("@MAPNAME@", map_name)
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
    script.write_text(rendered)
    config = run / "springsettings.cfg"
    config.write_text("")
    command = [str(binary), "--isolation", "--isolation-dir", str(data),
               "--write-dir", str(run), "--config", str(config), "--only-local", str(script)]
    env = os.environ.copy()
    # These override isolation and would introduce unrelated game data.
    env.pop("SPRING_DATADIR", None)
    env.pop("SPRING_WRITEDIR", None)
    for key in list(env):
        if key.startswith("DYLD_"):
            env.pop(key)
    metadata = {
        "status": "running", "command": command, "game": game, "map": map_name,
        "platform": platform.platform(), "machine": platform.machine(),
        "engine_sha256": sha256(binary), "template_sha256": sha256(template),
        "startscript_sha256": sha256(script), "timeout_seconds": args.timeout,
        "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip(),
        "cross_platform_sync": "not tested by this runner",
    }
    report = run / "run.json"
    report.write_text(json.dumps(metadata, indent=2) + "\n")
    start = time.monotonic()
    try:
        with (run / "process.log").open("wb") as log:
            result = subprocess.run(command, cwd=run, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
        metadata["exit_code"] = result.returncode
        output = run / "synctest_synchash.json"
        if output.exists():
            metadata["synctest"] = json.loads(output.read_text())
            metadata["synctest_sha256"] = sha256(output)
        passed = result.returncode == 0 and bool(metadata.get("synctest", {}).get("digest"))
        metadata["status"] = "completed_with_digest" if passed else "failed"
    except subprocess.TimeoutExpired:
        metadata["status"] = "timeout"
        passed = False
    finally:
        metadata["elapsed_seconds"] = time.monotonic() - start
        report.write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({key: metadata[key] for key in ("status", "elapsed_seconds", "cross_platform_sync")}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
