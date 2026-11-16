#!/usr/bin/env python3
"""Read-only exact frozen source audit after G01 build."""
import hashlib
import json
from pathlib import Path

G = Path(__file__).resolve().parents[1]
source = G / "source/engine"
manifest = json.loads((G / "SOURCE-MANIFEST.json").read_text())
expected = {row["path"]: row for row in manifest}
observed = {}
for path in source.rglob("*"):
    if path.is_file():
        relative = path.relative_to(source).as_posix()
        observed[relative] = {
            "bytes": path.stat().st_size,
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        }
missing = sorted(expected.keys() - observed.keys())
extra = sorted(observed.keys() - expected.keys())
mismatched = sorted(
    key for key in expected.keys() & observed.keys()
    if observed[key] != {"bytes": expected[key]["bytes"], "sha256": expected[key]["sha256"]}
)
receipt = {
    "status": "PASS" if not (missing or extra or mismatched) else "FAIL",
    "manifestSHA256": hashlib.sha256((G / "SOURCE-MANIFEST.json").read_bytes()).hexdigest(),
    "expectedFiles": len(expected),
    "observedFiles": len(observed),
    "missing": missing[:20],
    "extra": extra[:20],
    "mismatched": mismatched[:20],
    "missingCount": len(missing),
    "extraCount": len(extra),
    "mismatchedCount": len(mismatched),
}
(G / "SOURCE-CLOSURE-BUILD.json").write_text(json.dumps(receipt, indent=2) + "\n")
print(json.dumps({k: v for k, v in receipt.items() if not isinstance(v, list)}))
raise SystemExit(receipt["status"] != "PASS")
