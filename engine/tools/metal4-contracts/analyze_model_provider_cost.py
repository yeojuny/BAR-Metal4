#!/usr/bin/env python3
"""Read bounded ModelProviderSummary records; never infer FPS or GPU activity.

One input must be one engine process's complete infolog. Separate files stay
separate populations. Counter payloads (count_a/count_b) are cumulative ns;
total_us/thread_cpu_ns on the emission records are NOT provider costs.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any

SUMMARY = 36
CHILD_KINDS = set(range(30, 36)) | set(range(37, 43))
IDENTITY = ("request", "request_draw", "request_sim", "job", "job_request",
            "job_draw", "job_sim", "lease", "submission", "event", "runtime_index")
LIMITS = [
    "Diagnostic inclusive wall/thread-CPU envelopes include observer cost.",
    "Model dispatch includes lookup/RTTI/validation/fact insertion, not RTTI alone.",
    "Post-dispatch includes Texture/sampler/registration and any nested waits.",
    "Other successful providers are not broken down by family.",
    "No repeated binding, completed input, GPU active time or FPS gain is proved.",
    "Do not add these subtotals to a containing Prepare/worker/Join duration.",
]


def analyze(text: str) -> dict[str, Any]:
    rows: dict[int, dict[str, int]] = {}
    errors: list[str] = []
    for line_no, line in enumerate(text.splitlines(), 1):
        # Reject dropped trace data, even when the lost summary cannot be seen.
        if "NativeCPU" in line:
            for dropped in re.findall(r"\bdropped=(\d+)", line):
                if int(dropped):
                    errors.append(f"line {line_no}: dropped trace records")
        if "[NativeCPUTrace]" not in line:
            continue
        fields = dict(re.findall(r"\b([A-Za-z_][A-Za-z_0-9]*)=([^\s]+)", line))
        try:
            names = ("id", "parent", "kind", "count_a", "count_b", "status", *IDENTITY)
            row = {key: int(fields[key]) for key in names}
            if row["id"] <= 0 or row["parent"] < 0 or row["count_a"] < 0 or row["count_b"] < 0:
                raise ValueError("invalid id/parent/count")
        except (KeyError, ValueError) as exc:
            errors.append(f"line {line_no}: malformed trace row ({exc})")
            continue
        if row["id"] in rows:
            errors.append(f"line {line_no}: duplicate id {row['id']} (duplicate/mixed process logs?)")
        else:
            rows[row["id"]] = row

    summaries = [r for r in rows.values() if r["kind"] == SUMMARY]
    if not summaries:
        errors.append("no ModelProviderSummary; old or incomplete log is not zero cost")
    children: dict[int, list[dict[str, int]]] = {}
    for row in rows.values():
        if row["kind"] not in CHILD_KINDS:
            continue
        parent = rows.get(row["parent"])
        if not parent or parent["kind"] != SUMMARY:
            errors.append(f"row {row['id']}: orphan provider counter")
        children.setdefault(row["parent"], []).append(row)

    scopes: list[dict[str, Any]] = []
    for summary in summaries:
        label = f"summary {summary['id']}"
        parts = children.get(summary["id"], [])
        by_kind = {r["kind"]: r for r in parts}
        if len(parts) != len(CHILD_KINDS) or set(by_kind) != CHILD_KINDS:
            errors.append(f"{label}: missing/duplicate counter kinds")
            continue
        if summary["job"] == 0 or summary["job_request"] == 0:
            errors.append(f"{label}: missing executing job identity")
        if any(r["status"] != 0 for r in [summary, *parts]):
            errors.append(f"{label}: failed/nonordinary trace status")
        if any(any(r[k] != summary[k] for k in IDENTITY) for r in parts):
            errors.append(f"{label}: mixed request/job/lease identities")
        pair = lambda kind: (by_kind[kind]["count_a"], by_kind[kind]["count_b"])
        requests, hits = pair(30)
        model, other = pair(37)
        failed, invalid_wall = pair(38)
        invalid_cpu, mismatch = pair(35)
        total = pair(34)
        durations = {"model_dispatch": pair(39), "model_preparation": pair(40),
                     "other_success": pair(41), "failed_calls": pair(42)}
        if invalid_wall or invalid_cpu or mismatch:
            errors.append(f"{label}: invalid wall={invalid_wall}, CPU={invalid_cpu}, scope={mismatch}")
        if requests != model + other + failed or summary["count_a"] != requests or summary["count_b"] != model + other:
            errors.append(f"{label}: call partition mismatch")
        if failed:
            errors.append(f"{label}: {failed} failed provider calls; exclude no rows silently")
        if hits > requests or pair(32)[0] != hits * 10:
            errors.append(f"{label}: legacy fact-hit/cast counters disagree")
        if total != tuple(sum(v[i] for v in durations.values()) for i in (0, 1)):
            errors.append(f"{label}: duration partition mismatch")
        if requests and not total[0]:
            errors.append(f"{label}: nonempty provider scope with zero wall time")
        scopes.append({
            "summary_id": summary["id"],
            "identity": {k: summary[k] for k in IDENTITY},
            "requests": requests, "type_fact_hits": hits,
            "completed_model": model, "completed_other": other, "failed": failed,
            "provider_wall_ns": total[0], "provider_thread_cpu_ns": total[1],
            "durations_ns": {k: {"wall": v[0], "thread_cpu": v[1]} for k, v in durations.items()},
        })

    result: dict[str, Any] = {"status": "INVALID" if errors else "VALID_DIAGNOSTIC_ONLY",
                              "errors": errors, "scopes": scopes, "limitations": LIMITS}
    # Partial/corrupt records remain visible, but never produce a usable aggregate.
    if errors:
        result["aggregate"] = None
        return result
    calls = sum(s["requests"] for s in scopes)
    aggregate: dict[str, Any] = {"scope_count": len(scopes), "requests": calls,
        "completed_model": sum(s["completed_model"] for s in scopes),
        "completed_other": sum(s["completed_other"] for s in scopes),
        "type_fact_hits": sum(s["type_fact_hits"] for s in scopes),
        "provider_wall_ns": sum(s["provider_wall_ns"] for s in scopes),
        "provider_thread_cpu_ns": sum(s["provider_thread_cpu_ns"] for s in scopes)}
    aggregate["durations_ns"] = {
        kind: {metric: sum(s["durations_ns"][kind][metric] for s in scopes)
               for metric in ("wall", "thread_cpu")}
        for kind in ("model_dispatch", "model_preparation", "other_success", "failed_calls")}
    aggregate["mean_provider_wall_us"] = aggregate["provider_wall_ns"] / calls / 1000 if calls else None
    aggregate["mean_provider_thread_cpu_us"] = aggregate["provider_thread_cpu_ns"] / calls / 1000 if calls else None
    result["aggregate"] = aggregate
    if not calls:
        result["status"] = "NO_PROVIDER_CALLS"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    parser.add_argument("--output", type=Path, help="Optional JSON report; input logs are not changed")
    args = parser.parse_args()
    if args.output and args.output.resolve() in {p.resolve() for p in args.logs}:
        parser.error("output must not overwrite an input log")
    reports = []
    for path in args.logs:
        try:
            report = analyze(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError) as exc:
            report = {"status": "INVALID", "errors": [str(exc)], "aggregate": None}
        reports.append({"file": str(path), **report})
    rendered = json.dumps({"schema": "model-provider-cost-v1", "runs": reports,
                           "pooled_between_runs": False}, indent=2) + "\n"
    if args.output:
        try:
            args.output.write_text(rendered, encoding="utf-8")
        except OSError as exc:
            parser.error(str(exc))
    else:
        sys.stdout.write(rendered)
    return 2 if any(r["status"] == "INVALID" for r in reports) else 0


if __name__ == "__main__":
    raise SystemExit(main())
