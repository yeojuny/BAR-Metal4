#!/usr/bin/env python3
"""Apply the accepted G01 private runtime's exact loader mapping, then sign."""
import hashlib
import json
import subprocess
from pathlib import Path

G = Path(__file__).resolve().parents[1]
C = G.parents[1]
S = C / "evidence/ground-decal-index-g01-20261116-01"
engine = G / "runtime/native/bin/spring-metal4-scene"
mapping = json.loads((S / "CANDIDATE.json").read_text())["relocations"]
assert len(mapping) == 9
raw = hashlib.sha256(engine.read_bytes()).hexdigest()
for old, new in mapping:
    subprocess.run(["install_name_tool", "-change", old, new, str(engine)], check=True)
subprocess.run(["codesign", "--force", "--sign", "-", "--timestamp=none", str(engine)], check=True)
subprocess.run(["codesign", "--verify", "--strict", "--verbose=2", str(engine)], check=True)
deps = subprocess.run(["otool", "-L", str(engine)], check=True, capture_output=True, text=True).stdout
assert "/opt/homebrew/" not in deps
assert all(new in deps for _, new in mapping)
libs = G / "runtime/native/lib"
library_receipt = [{"name": p.name, "sha256": hashlib.sha256(p.read_bytes()).hexdigest()}
                   for p in sorted(libs.glob("*.dylib"))]
receipt = {"status": "PASS", "rawSHA256": raw,
           "signedSHA256": hashlib.sha256(engine.read_bytes()).hexdigest(),
           "engine": str(engine), "relocations": mapping, "relocationCount": len(mapping),
           "libraries": library_receipt, "libraryCount": len(library_receipt),
           "codesignVerify": "PASS", "homebrewDependencies": 0}
(G / "CANDIDATE.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps({k: v for k, v in receipt.items() if not isinstance(v, list)}))
