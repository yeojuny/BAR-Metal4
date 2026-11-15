#!/usr/bin/env python3
"""Portable semantic tests plus an ARC syntax check; no Metal runtime/FPS claim."""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tempfile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", default=os.environ.get("CXX", "clang++"))
    parser.add_argument("--native-syntax", action="store_true", help="Use the installed macOS SDK instead of Metal declaration stubs")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    here = Path(__file__).resolve().parent
    engine = here.parents[1]
    records: list[dict[str, object]] = []

    def run(command: list[str], **kwargs: object) -> str:
        result = subprocess.run(command, check=True, text=True, capture_output=True, **kwargs)
        records.append({"command": command, "stdout": result.stdout, "stderr": result.stderr})
        if result.stdout:
            print(result.stdout, end="")
        return result.stdout

    with tempfile.TemporaryDirectory(prefix="bar-material-perf-") as tmp:
        work = Path(tmp)
        # Copy unmodified production headers: quoted adjacent includes must use
        # modeled dependencies rather than the real Metal/runtime declarations.
        for relative in ("Rendering/Metal4/Metal4SharedProgramPilot.h", "Rendering/Backend/NativeSharedProgramPilot.h"):
            dst = work / relative
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(engine / "rts" / relative, dst)
        common = [args.cxx, "-std=c++20", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-Wall", "-Wextra", "-Werror", "-Wno-misleading-indentation"]
        includes = ["-I" + str(work), "-I" + str(engine / "rts"), "-I" + str(here / "stubs")]
        pair = work / "pair"
        run(common + includes + [str(here / "test_pair_lookup.cpp"), "-o", str(pair)])
        for mode in ("0", "1"):
            run([str(pair), mode], env={**os.environ, "BAR_NATIVE_SHARED_PAIR_KEY_VIEW": mode})
        memo = work / "memo"
        run(common + ["-I" + str(engine / "rts"), str(here / "test_binding_memo.cpp"), "-o", str(memo)])
        run([str(memo)])
        syntax = [args.cxx, "-std=c++20", "-x", "objective-c++", "-fobjc-arc", "-Wno-misleading-indentation", "-I" + str(engine / "rts")]
        if args.native_syntax:
            if platform.system() != "Darwin":
                raise SystemExit("--native-syntax requires macOS and its installed Metal SDK")
            sdk = run(["xcrun", "--sdk", "macosx", "--show-sdk-path"]).strip()
            syntax += ["-isysroot", sdk]
        else:
            syntax += ["-fobjc-runtime=macosx-10.13", "-I" + str(here / "stubs")]
        source = str(here / "syntax_material.mm")
        run(syntax + ["-fsyntax-only", source])
        ir = work / "material.ll"
        run(syntax + ["-S", "-emit-llvm", "-O0", source, "-o", str(ir)])
        text = ir.read_text()
        bodies = re.findall(r"^define[^\n]*NativeExactBindingMemo[^\n]*7Resolve[^\n]*\n.*?^}", text, re.M | re.S)
        if not bodies or not any("U8__strong" in body and "llvm.objc.storeStrong" in body for body in bodies):
            raise AssertionError("ARC-strong owner assignment was not instantiated in memo Resolve")
        print("Objective-C++ ARC integration: PASS (declarations only; no Metal runtime)")
    result = {"status": "PASS", "native_sdk_syntax": args.native_syntax, "full_engine_build": False, "metal_runtime": False, "fps_measured": False, "commands": records}
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
