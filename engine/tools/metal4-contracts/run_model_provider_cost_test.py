#!/usr/bin/env python3
"""Compile the actual provider scope with host adapters and test its log reader.

No Metal device, engine build or game execution is performed. Requires a C++20
Clang/GCC compiler; ASan/UBSan are enabled unless --no-sanitize is specified.
"""
from __future__ import annotations

import argparse
import importlib.util
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ENGINE = HERE.parents[1]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++"))
    parser.add_argument("--no-sanitize", action="store_true")
    args = parser.parse_args()
    if not args.cxx:
        parser.error("a C++20 compiler is required")
    spec = importlib.util.spec_from_file_location("provider_reader", HERE / "analyze_model_provider_cost.py")
    assert spec and spec.loader
    reader = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(reader)
    with tempfile.TemporaryDirectory(prefix="bar-model-provider-cost-") as temporary:
        temp = Path(temporary)
        binary = temp / "test"
        command = [args.cxx, "-std=c++20", "-O1", "-g", "-Wall", "-Wextra",
                   "-Wno-misleading-indentation", "-pthread", "-I" + str(ENGINE / "rts"),
                   str(HERE / "TestModelProviderCost.cpp"), "-o", str(binary)]
        if not args.no_sanitize:
            command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        subprocess.run(command, check=True)
        for mode in ("0", "1"):
            env = {**os.environ, "BAR_NATIVE_MODEL_TYPE_FACTS": mode}
            subprocess.run([str(binary)], check=True, env=env)
            text = subprocess.check_output([str(binary), "--emit"], text=True, env=env)
            report = reader.analyze(text)
            assert report["status"] == "VALID_DIAGNOSTIC_ONLY", report
            assert report["aggregate"]["requests"] == 3
            assert report["aggregate"]["completed_model"] == 2
            assert report["aggregate"]["completed_other"] == 1
            assert report["aggregate"]["type_fact_hits"] == int(mode)
            assert report["aggregate"]["provider_wall_ns"] > 0
            log = temp / ("mode-" + mode + ".log")
            log.write_text(text, encoding="utf-8")
            subprocess.run([sys.executable, str(HERE / "analyze_model_provider_cost.py"),
                            str(log), "--output", str(temp / ("mode-" + mode + ".json"))], check=True)
            rows = text.splitlines()
            mutations = {
                "missing_counter": "\n".join(r for r in rows if " kind=40 " not in r),
                "missing_parent": "\n".join(r for r in rows if " kind=36 " not in r),
                "duplicate_id": text + rows[0] + "\n",
                "dropped_records": text + "[NativeCPUTraceBuffer] dropped=1\n",
                "scope_mismatch": "\n".join(re.sub(r"count_b=\d+", "count_b=1", r) if " kind=35 " in r else r for r in rows),
                "duration_mismatch": "\n".join(re.sub(r"count_a=\d+", "count_a=999999999", r) if " kind=34 " in r else r for r in rows),
                "identity_mismatch": "\n".join(r.replace("job_draw=11", "job_draw=12") if " kind=40 " in r else r for r in rows),
                "invalid_wall": "\n".join(re.sub(r"count_b=\d+", "count_b=1", r) if " kind=38 " in r else r for r in rows),
                "invalid_cpu": "\n".join(re.sub(r"count_a=\d+", "count_a=1", r) if " kind=35 " in r else r for r in rows),
                "failed_status": "\n".join(r.replace("status=0", "status=2") if " kind=36 " in r else r for r in rows),
                "negative_counter": "\n".join(re.sub(r"count_a=\d+", "count_a=-1", r) if " kind=34 " in r else r for r in rows),
                "old_counter_only": "\n".join(r for r in rows if re.search(r" kind=3[0-5] ", r)),
            }
            for name, damaged in mutations.items():
                result = reader.analyze(damaged)
                assert result["status"] == "INVALID" and result["aggregate"] is None, (name, result)
        assert reader.analyze("")["status"] == "INVALID"

        # A negative control uses the same actual .inc and replaces ONLY its
        # wall clock with the historical disabled Clock. It must fail the
        # zero-clock assertion rather than silently emitting zero work.
        override = temp / "negative" / "Rendering" / "Metal4"
        override.mkdir(parents=True)
        actual = (ENGINE / "rts/Rendering/Metal4/Metal4ModelTypeFacts.inc").read_text()
        assert actual.count("std::chrono::steady_clock::now()") == 1
        (override / "Metal4ModelTypeFacts.inc").write_text(actual.replace("std::chrono::steady_clock::now()", "Clock::now()"))
        negative = command.copy()
        position = negative.index("-I" + str(ENGINE / "rts"))
        negative.insert(position, "-I" + str(temp / "negative"))
        negative[-1] = str(temp / "negative-test")
        subprocess.run(negative, check=True)
        def no_core() -> None:
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        failed = subprocess.run([negative[-1]], text=True, capture_output=True,
                                env={**os.environ, "BAR_NATIVE_MODEL_TYPE_FACTS": "0"},
                                preexec_fn=no_core if os.name == "posix" else None)
        assert failed.returncode != 0 and "Now()" in failed.stderr, failed
    print("PASS: OFF/ON host tests, 24 corrupt-log cases, CLI output and historical zero-clock negative control")
    print("NOT TESTED: Objective-C++ engine compilation, Metal runtime/visual parity, game performance")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
