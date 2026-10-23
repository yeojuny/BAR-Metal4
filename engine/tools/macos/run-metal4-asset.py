#!/usr/bin/env python3
"""Run the engine-owned Metal 4 asset diagnostic in a fresh isolated directory.

Requires BUILD_METAL4_ASSET_DIAGNOSTIC=ON. This verifies an engine asset path,
not interactive gameplay, Chobby, or application acceptance.
"""
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html

import argparse
import hashlib
import json
import os
from pathlib import Path
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
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--width", type=int, default=1024)
    parser.add_argument("--height", type=int, default=1024)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    build, assets, run = (p.resolve() for p in (args.build_dir, args.assets, args.run_dir))
    if run.exists() and any(run.iterdir()):
        parser.error("--run-dir must be new or empty; previous outputs must not be reused")
    if not (1 <= args.width <= 4096 and 1 <= args.height <= 4096):
        parser.error("explicit pixel dimensions must be between 1 and 4096")
    binary = build / "spring-headless"
    material = source / "tools/metal4-contracts/BarMaterialSemantics.metal"
    inputs = [binary, material, *(assets / name for name in ("armcom.s3o", "Arm_color.dds", "Arm_other.dds"))]
    if not all(p.is_file() for p in inputs) or not (build / "base").is_dir():
        parser.error("built engine/base archives and all three original commander assets are required")
    run.mkdir(parents=True, exist_ok=True)
    shutil.copytree(build / "base", run / "base")
    config = run / "springsettings.cfg"
    config.write_text("")
    output = run / "output"
    command = [str(binary), "--isolation", "--isolation-dir", str(run),
               "--write-dir", str(run), "--config", str(config),
               "--metal4-assets", str(assets), "--metal4-output", str(output),
               "--metal4-material", str(material),
               "--metal4-width", str(args.width), "--metal4-height", str(args.height)]
    env = {k: v for k, v in os.environ.items()
           if not k.startswith("DYLD_") and k not in ("SPRING_DATADIR", "SPRING_WRITEDIR")}
    env["MTL_DEBUG_LAYER"] = "1"
    metadata = {"status": "running", "command": command,
                "input_sha256": {str(p): sha256(p) for p in inputs},
                "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip(),
                "validation": "Metal API and pipeline shader validation; no capture requested",
                "gameplay_acceptance": False}
    report_path = run / "run.json"
    report_path.write_text(json.dumps(metadata, indent=2) + "\n")
    start = time.monotonic()
    passed = False
    try:
        with (run / "process.log").open("wb") as log:
            result = subprocess.run(command, cwd=run, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        metadata["exit_code"] = result.returncode
        engine_report = output / "engine-asset-report.json"
        if engine_report.is_file():
            metadata["engine_report"] = json.loads(engine_report.read_text())
        artifacts = [output / name for name in ("engine-commander.png", "engine-geometry.bin", "engine-asset-report.json")]
        passed = (result.returncode == 0 and all(p.is_file() for p in artifacts)
                  and metadata.get("engine_report", {}).get("status") == "engine_asset_rendered")
        metadata["status"] = "completed" if passed else "failed"
        metadata["output_sha256"] = {str(p.relative_to(run)): sha256(p) for p in artifacts if p.is_file()}
    except subprocess.TimeoutExpired:
        metadata["status"] = "timeout"
    finally:
        metadata["seconds"] = time.monotonic() - start
        report_path.write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({"status": metadata["status"], "run_dir": str(run), "seconds": metadata["seconds"]}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
