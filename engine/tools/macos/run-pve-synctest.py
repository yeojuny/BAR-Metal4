#!/usr/bin/env python3
"""Run a bounded, synthetic original-content Raptor/Scav checksum scenario.

The stock synctest gadget feeds units to both teams. This is a local diagnostic,
not ordinary full PvE play or cross-platform synchronization certification.
"""
# This file is part of the Spring engine (GPL v2 or later), see LICENSE.html

import argparse
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import struct
import subprocess
import time


GAME = "Beyond All Reason test-30770-4e0b241"
MAP = "Jade Empress 1.41"
PACKAGE = "0c038cabef0706d6311d828c3eeb794a"
FRAMES = 13500
START_FRAME = 100
QUIT_FRAME = 13800
MODES = {
    "raptor": ("RaptorsAI", "Raptor Defense Spawner", "raptor_queen_veryeasy", "raptor_queentimemult", "raptor_queen_count"),
    "scav": ("ScavengersAI", "Scav Defense Spawner", "scavengerbossv4_veryeasy_scav", "scav_bosstimemult", "scav_boss_count"),
}


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def inspect_sources(data, mode):
    package = data / "packages" / (PACKAGE + ".sdp")
    raw = gzip.decompress(package.read_bytes())
    entries = {}
    offset = 0
    while offset < len(raw):
        length = raw[offset]
        offset += 1
        name = raw[offset:offset + length].decode("utf-8")
        offset += length
        digest = raw[offset:offset + 16].hex()
        offset += 16
        _, size = struct.unpack_from(">II", raw, offset)
        offset += 8
        entries[name] = (digest, size)
    names = ["luaai.lua", "modoptions.lua", "luarules/gadgets/dbg_synctest.lua",
             "luarules/configs/" + mode + "_spawn_defs.lua",
             "luarules/gadgets/" + mode + "_spawner_defense.lua"]
    result = {}
    for name in names:
        digest, size = entries[name]
        payload = gzip.decompress((data / "pool" / digest[:2] / (digest[2:] + ".gz")).read_bytes())
        if len(payload) != size or hashlib.md5(payload).hexdigest() != digest:
            raise ValueError("Original pool member failed size/MD5 validation: " + name)
        result[name] = {"md5": digest, "size": size, "sha256": hashlib.sha256(payload).hexdigest()}
        if name == "luaai.lua" and MODES[mode][0].encode() not in payload:
            raise ValueError("Requested LuaAI is not declared by the pinned game")
    return result


def render(template, mode):
    ai, _, _, timing, count = MODES[mode]
    options = {mode + "_difficulty": "veryeasy", timing: "0.1", count: "1",
               mode + "_graceperiodmult": "0.1", mode + "_spawncountmult": "1",
               mode + "_spawntimemult": "1", mode + "_endless": "0"}
    commands = "|".join([
        "2:cheat", "3:globallos", "4:luaui disable", "5:dumpstate 1000 1000 1 1",
        "15:luarules synctest " + str(FRAMES) + " 0.33 0.33 0.1 0.1 " + str(START_FRAME),
        # The engine's per-frame call resets DumpState's sampling period to 1.
        # Use single-frame ranges so diagnostics stay bounded without a patch.
        "20:setspeed 20", "21:speedcontrol 0", "1002:dumpstate 12000 12000 1 1",
        "12001:dumpstate 13000 13000 1 1", "13001:dumpstate 13500 13500 1 1",
        str(QUIT_FRAME) + ":quitforce",
    ])
    script = template.replace("@VERSION@", GAME.rsplit(" ", 1)[1]).replace("@MAPNAME@", MAP)
    script, changed_ai = re.subn(r"shortname=NullAI;", "shortname=" + ai + ";", script)
    option_lines = "\n".join("        " + key + "=" + value + ";" for key, value in sorted(options.items()))
    script, changed_commands = re.subn(r"        debugcommands=[^\n]+;", "        debugcommands=" + commands + ";\n" + option_lines, script)
    if changed_ai != 1 or changed_commands != 1:
        raise ValueError("Pinned template changed; refusing an ambiguous scenario edit")
    return script, options, commands


def dump_evidence(run, boss):
    snapshots = {}
    for path in sorted(run.glob("*GameState-*.txt")):
        if path.stat().st_size > 64 * 1024 * 1024:
            continue
        current = None
        unit = None
        with path.open(errors="replace") as stream:
            for line in stream:
                match = re.match(r"^frame: (\d+),", line)
                if match:
                    current = int(match[1])
                    snapshots.setdefault(current, {"file": path.name, "boss_units": []})
                    unit = None
                    continue
                match = re.match(r"^\t\tunitID: (\d+) \(name: ([^)]+)\)", line)
                if match and current is not None:
                    unit = None
                    if match[2] == boss:
                        unit = {"id": int(match[1]), "name": match[2]}
                        snapshots[current]["boss_units"].append(unit)
                    continue
                if unit is not None:
                    match = re.match(r"^\t\t\thealth: (.+)", line)
                    if match:
                        unit["health_raw_text"] = match[1].strip()
                        # TapFloats writes a raw hash before the actual value.
                        # Only the angle-bracketed scalar establishes health.
                        value = re.search(r"<([^<>]+)>", match[1])
                        unit["positive_health"] = False
                        if value:
                            try:
                                health = float(value[1])
                                unit["positive_health"] = math.isfinite(health) and health > 0
                            except ValueError:
                                pass
                    match = re.match(r"^\t\t\tisDead: (\d+)", line)
                    if match:
                        unit["is_dead"] = bool(int(match[1]))
    return snapshots


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=MODES, required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--run-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 240:
        parser.error("--timeout must be between 1 and the bounded 240 seconds")
    source = Path(__file__).resolve().parents[2]
    build, data, run = (path.resolve() for path in (args.build_dir, args.data_dir, args.run_dir))
    if run.exists() and any(run.iterdir()):
        parser.error("--run-dir must be new or empty; prior results cannot be reused")
    binary = build / "spring-headless"
    package = data / "packages" / (PACKAGE + ".sdp")
    map_file = data / "maps/jade_empress_1.41.sd7"
    if not binary.is_file() or not package.is_file() or not map_file.is_file():
        parser.error("Built headless engine and exact already downloaded game/map are required")
    if any((data / name).exists() for name in ("LuaRules", "luarules", "LuaUI", "luaui", "games", "mods")):
        parser.error("Data directory contains potential local content overlays")
    workflow = (source / ".github/workflows/synctest.yml").read_text()
    if re.search(r'^  GAME: "([^"]+)"$', workflow, re.MULTILINE)[1] != GAME or re.search(r'^  MAP: "([^"]+)"$', workflow, re.MULTILINE)[1] != MAP:
        parser.error("Workflow content pins changed; this bounded scenario needs source review")
    try:
        member_hashes = inspect_sources(data, args.mode)
        template = source / "test/synctest/synctest-startscript.txt"
        rendered, options, commands = render(template.read_text(), args.mode)
    except (OSError, ValueError, KeyError, struct.error) as error:
        parser.error(str(error))
    run.mkdir(parents=True, exist_ok=True)
    shutil.copytree(build / "base", run / "base")
    script = run / "startscript.txt"
    script.write_text(rendered)
    config = run / "springsettings.cfg"
    config.write_text("WorkerThreadCount = 2\n")
    command = [str(binary), "--isolation", "--isolation-dir", str(data), "--write-dir", str(run),
               "--config", str(config), "--only-local", str(script)]
    env = os.environ.copy()
    env.pop("SPRING_DATADIR", None)
    env.pop("SPRING_WRITEDIR", None)
    for key in list(env):
        if key.startswith("DYLD_"):
            env.pop(key)
    ai, spawner, boss, _, _ = MODES[args.mode]
    metadata = {"status": "running", "scenario": "synthetic stock synctest collector with original PvE spawner",
                "mode": args.mode, "lua_ai": ai, "boss_unit_name": boss, "command": command,
                "game": GAME, "map": MAP, "package": PACKAGE, "fixed_rng_seed": 123123,
                "frames": FRAMES, "start_frame": START_FRAME, "quit_frame": QUIT_FRAME,
                "modoptions": options, "debugcommands": commands, "worker_thread_count_including_main": 2,
                "platform": platform.platform(), "machine": platform.machine(), "timeout_seconds": args.timeout,
                "engine_sha256": sha256(binary), "template_sha256": sha256(template),
                "startscript_sha256": sha256(script), "config_sha256": sha256(config),
                "package_sha256": sha256(package), "map_sha256": sha256(map_file), "verified_source_members": member_hashes,
                "source_head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip(),
                "cross_platform_sync": "not tested", "full_pve_acceptance": "not tested"}
    report = run / "run.json"
    report.write_text(json.dumps(metadata, indent=2) + "\n")
    start = time.monotonic()
    passed = False
    try:
        with (run / "process.log").open("wb") as log:
            result = subprocess.run(command, cwd=run, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=args.timeout)
        metadata["exit_code"] = result.returncode
    except subprocess.TimeoutExpired:
        metadata["status"] = "timeout"
    finally:
        metadata["elapsed_seconds"] = time.monotonic() - start
        log_path = run / "infolog.txt"
        log = log_path.read_text(errors="replace") if log_path.exists() else ""
        # Spring.Log section filtering can suppress the Activated! message.
        # The exact synced gadget only loads when its original mode guard passes.
        active_lines = [line for line in log.splitlines() if
                        (spawner + " Activated!" in line or
                         ("Loaded synced gadget:" in line and spawner in line and
                          "<" + args.mode + "_spawner_defense.lua>" in line))]
        metadata["mode_activation_lines"] = active_lines
        metadata["lua_ai_control_lines"] = [line for line in log.splitlines() if
            'local skirmish AI "' + ai + '"' in line and "control of team 1" in line]
        metadata["nan_or_invalid_rule_lines"] = [line for line in log.splitlines() if re.search(r"\bNaN\b|invalid.*(?:GameRules|rule.*param)|SetGameRulesParam.*invalid", line, re.IGNORECASE)]
        metadata["dump_snapshots"] = dump_evidence(run, boss)
        metadata["oversized_dump_files_skipped"] = [path.name for path in run.glob("*GameState-*.txt")
            if path.stat().st_size > 64 * 1024 * 1024]
        snapshots = metadata["dump_snapshots"]
        metadata["preboss_checkpoint_covered"] = 1000 in snapshots and not snapshots[1000]["boss_units"]
        metadata["live_boss_checkpoint_frames"] = sorted(frame for frame, snapshot in snapshots.items() if any(
            unit.get("positive_health") and unit.get("is_dead") is False for unit in snapshot["boss_units"]))
        output = run / "synctest_synchash.json"
        complete = False
        if output.exists():
            try:
                payload = json.loads(output.read_text())
                rows = payload.get("checksums", [])
                complete = (payload.get("frameCount") == FRAMES and payload.get("firstFrame") == 1 and
                            payload.get("lastFrame") == FRAMES and isinstance(payload.get("digest"), str) and
                            bool(payload["digest"]) and len(rows) == FRAMES and
                            all(isinstance(row, dict) and row.get("frame") == index and
                                isinstance(row.get("checksum"), str) and bool(row["checksum"])
                                for index, row in enumerate(rows, 1)))
                metadata["synctest_summary"] = {key: payload.get(key) for key in ("digest", "frameCount", "firstFrame", "lastFrame")}
                metadata["synctest_sha256"] = sha256(output)
            except (ValueError, TypeError):
                metadata["invalid_synctest_json"] = True
        metadata["complete_frame_artifact"] = complete
        metadata["package_unchanged"] = sha256(package) == metadata["package_sha256"]
        metadata["map_unchanged"] = sha256(map_file) == metadata["map_sha256"]
        passed = (metadata.get("exit_code") == 0 and complete and bool(active_lines) and
                  bool(metadata["lua_ai_control_lines"]) and
                  metadata["preboss_checkpoint_covered"] and bool(metadata["live_boss_checkpoint_frames"]) and
                  not metadata["nan_or_invalid_rule_lines"] and metadata["package_unchanged"] and metadata["map_unchanged"])
        if metadata["status"] != "timeout":
            metadata["status"] = "bounded_checkpoints_passed" if passed else "incomplete_or_failed"
        report.write_text(json.dumps(metadata, indent=2) + "\n")
    print(json.dumps({key: metadata.get(key) for key in ("status", "mode", "elapsed_seconds", "exit_code",
          "preboss_checkpoint_covered", "live_boss_checkpoint_frames", "complete_frame_artifact", "synctest_summary")}))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
