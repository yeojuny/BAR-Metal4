#!/usr/bin/env python3
"""Compile actual LuaVBO method bodies against independent CPU byte oracles."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def extract(source, name):
    match = re.search(r"^(?:template[^\n]*\n)?[^\n]*LuaVBOImpl::" + name + r"\(", source, re.MULTILINE)
    if not match:
        raise ValueError(f"missing LuaVBOImpl::{name}")
    begin = source.index("{", match.end())
    depth = 0
    # Ignore braces inside comments and literals, including GetTypePtr's old comments.
    tokens = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/|[{}]'
    for token in re.finditer(tokens, source[begin:], re.DOTALL):
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():begin + token.end()]
    raise ValueError(f"unclosed LuaVBOImpl::{name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2] / "rts/Lua/LuaVBOImpl.cpp")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-snapshots", action="store_true", help="Also extract actual native snapshot/Clear/range methods.")
    parser.add_argument("--expect-failure", action="store_true", help="Negative control: the unfixed source must fail byte oracles cleanly.")
    args = parser.parse_args()
    source = args.source.read_text()
    args.output.mkdir(parents=True, exist_ok=True)
    names = ["GetTypePtr", "Download", "UploadImpl", "TransformAndWrite", "TransformAndRead"]
    preflight = "bool LuaVBOImpl::GetAttributeScalarCount(" in source
    if preflight:
        names[:0] = ["GetAttributeScalarCount", "IsElementByteRangeValid"]
    if args.native_snapshots:
        if not preflight or args.expect_failure:
            parser.error("--native-snapshots requires the current checked implementation")
        names += ["MakeNativeLayout", "CaptureNativeSnapshot", "CaptureNativeRange", "Clear", "BindBufferRangeImpl"]
    methods = {name: extract(source, name) for name in names}
    (args.output / "methods.inc").write_text("\n\n".join(methods.values()) + "\n")
    fixture = Path(__file__).with_name("luavbo_layout_test.cpp")
    binary = args.output / "luavbo-layout-test"
    command = ["/usr/bin/clang++", "-std=c++23", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Wno-unused-lambda-capture",
               "-Wno-sign-compare", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-ffp-contract=off",
               "-I", str(args.output), str(fixture), "-o", str(binary)]
    if preflight:
        command.insert(1, "-DLAYOUT_PREFLIGHT=1")
    if args.native_snapshots:
        command.insert(1, "-DRECOIL_METAL4_LIVE_SCENE=1")
        command += ["-I", str(Path(__file__).resolve().parents[2] / "rts")]
    compilation = subprocess.run(command, capture_output=True, text=True)
    (args.output / "compile.log").write_text(compilation.stdout + compilation.stderr)
    if compilation.returncode:
        print(compilation.stderr)
        return compilation.returncode
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    (args.output / "run.log").write_text(result.stdout + result.stderr)
    sanitizer_failure = any(marker in result.stderr for marker in ("AddressSanitizer", "runtime error:", "UndefinedBehaviorSanitizer"))
    passed = result.returncode == 0 if not args.expect_failure else result.returncode == 1 and "FAIL " in result.stderr and not sanitizer_failure
    receipt = {
        "source": str(args.source.resolve()), "source_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "method_sha256": {name: hashlib.sha256(body.encode()).hexdigest() for name, body in methods.items()},
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(), "compile_command": command,
        "run_command": [str(binary)], "compile_exit": compilation.returncode, "run_exit": result.returncode,
        "expect_failure": args.expect_failure, "native_snapshot_methods": args.native_snapshots, "sanitizer_failure": sanitizer_failure,
        "summary": result.stdout.strip(), "passed": passed,
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(result.stdout, end="")
    if args.expect_failure:
        print("Baseline failed independent byte oracles as expected." if passed else "Invalid negative control.")
    elif result.stderr:
        print(result.stderr, end="")
    return 0 if passed and not sanitizer_failure else 1


if __name__ == "__main__":
    raise SystemExit(main())
