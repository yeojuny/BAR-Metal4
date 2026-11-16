#!/usr/bin/env python3
"""Read material storage COUNTERS from one complete engine log per input.

No timing/FPS inference. Old kinds43-46 without the new summary are not a
zero-cost result. Keep processes/scenes and clean/diagnostic runs separate.
"""
from __future__ import annotations
import argparse
import json
import re
from pathlib import Path

IDENTITY = ('request', 'request_draw', 'request_sim', 'job', 'job_request',
            'job_draw', 'job_sim', 'lease', 'submission', 'event', 'runtime_index')
CHILDREN = {43, 44, 45, 46, 48, 49, 50}


def analyze(text: str) -> dict:
    rows, errors = {}, []
    for number, line in enumerate(text.splitlines(), 1):
        if 'NativeCPU' in line and any(int(x) for x in re.findall(r'\bdropped=(\d+)', line)):
            errors.append(f'line {number}: dropped records')
        if '[NativeCPUTrace]' not in line:
            continue
        fields = dict(re.findall(r'\b(\w+)=([^\s]+)', line))
        try:
            row = {k: int(fields[k]) for k in ('id', 'parent', 'kind', 'status', 'count_a', 'count_b', *IDENTITY)}
            if row['id'] <= 0 or min(row['parent'], row['count_a'], row['count_b']) < 0:
                raise ValueError('negative counter/id')
            if row['id'] in rows:
                raise ValueError('duplicate id or mixed process logs')
            rows[row['id']] = row
        except (KeyError, ValueError) as exc:
            errors.append(f'line {number}: {exc}')
    summaries = [r for r in rows.values() if r['kind'] == 47]
    if not summaries:
        errors.append('no storage summary; missing/old log is not zero cost')
    groups = []
    for summary in summaries:
        children = [r for r in rows.values() if r['parent'] == summary['id'] and r['kind'] in CHILDREN]
        by_kind = {r['kind']: r for r in children}
        if len(children) != 7 or set(by_kind) != CHILDREN:
            errors.append(f"summary {summary['id']}: incomplete/duplicate counter group")
            continue
        if not summary['job'] or not summary['job_request']:
            errors.append('missing executing job identity')
        if any(r['status'] or any(r[k] != summary[k] for k in IDENTITY) for r in [summary, *children]):
            errors.append('mixed identity or unsuccessful scope')
        pair = lambda k: (by_kind[k]['count_a'], by_kind[k]['count_b'])
        gets, hits = pair(45); misses, encoded = pair(46)
        new, reused = pair(48); allocated, retained = pair(49)
        slots, capacity = pair(50); enabled = summary['count_b']
        if summary['count_a'] != gets or enabled not in (0, 1) or hits + misses != gets or new + reused != misses:
            errors.append('get/miss/allocation partition mismatch')
        if sum(pair(44)) != gets * 24 or pair(43)[0] > hits:
            errors.append('identity reader/hit counters disagree')
        if capacity != 256 or slots > capacity or (not enabled and (reused or slots or retained)):
            errors.append('pool state/capacity mismatch')
        if (not new and allocated) or (not misses and encoded) or (misses and (not encoded or encoded > misses * 4096)):
            errors.append('byte counters inconsistent')
        groups.append({'identity': {k: summary[k] for k in IDENTITY}, 'enabled': bool(enabled),
                       'gets': gets, 'hits': hits, 'encodes': misses, 'encoded_bytes': encoded,
                       'new_buffers': new, 'reused_buffers': reused, 'new_allocated_bytes': allocated,
                       'retained_pool_slots': slots, 'retained_pool_bytes': retained})
    for row in rows.values():
        if row['kind'] in CHILDREN and rows.get(row['parent'], {}).get('kind') != 47:
            errors.append('orphan storage counter')
    if len({g['enabled'] for g in groups}) > 1:
        errors.append('mixed pool modes in one process')
    aggregate = None
    if not errors:
        additive = ('gets', 'hits', 'encodes', 'encoded_bytes', 'new_buffers', 'reused_buffers', 'new_allocated_bytes')
        aggregate = {k: sum(g[k] for g in groups) for k in additive}
        aggregate['groups'] = len(groups)
        # State samples are not additive; caches can be shared across batches.
        aggregate['max_observed_pool_slots'] = max(g['retained_pool_slots'] for g in groups)
        aggregate['max_observed_pool_bytes'] = max(g['retained_pool_bytes'] for g in groups)
    return {'status': 'INVALID' if errors else 'VALID_WORK_COUNTS_ONLY', 'errors': errors,
            'groups': groups, 'aggregate': aggregate,
            'limits': ['Not CPU time, peak RAM, GPU activity or FPS gain.',
                       'Pool bytes overlap content-cache/draw owners and exclude uncached outstanding spills.',
                       'Keep diagnostics separate from clean performance; reject dropped groups.']}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('logs', nargs='+', type=Path); p.add_argument('--output', type=Path)
    args = p.parse_args()
    if args.output and args.output.resolve() in {x.resolve() for x in args.logs}:
        p.error('output must not overwrite an input')
    runs = []
    for path in args.logs:
        try: result = analyze(path.read_text(encoding='utf-8'))
        except (OSError, UnicodeError) as exc: result = {'status': 'INVALID', 'errors': [str(exc)], 'aggregate': None}
        runs.append({'file': str(path), **result})
    data = json.dumps({'schema': 'material-storage-v1', 'runs': runs, 'pooled_between_runs': False}, indent=2) + '\n'
    if args.output: args.output.write_text(data, encoding='utf-8')
    else: print(data, end='')
    return 2 if any(r['status'] == 'INVALID' for r in runs) else 0

if __name__ == '__main__':
    raise SystemExit(main())
