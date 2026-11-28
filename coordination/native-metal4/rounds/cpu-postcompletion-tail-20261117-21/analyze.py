"""Read-only analysis of the single frozen CPU-tail run; never launches processes.

Raw files and the pre-run parser specification remain unchanged. All times used
for joins/differences are integer CPU monotonic nanoseconds. No FPS is computed.
"""
from pathlib import Path
import collections
import hashlib
import json
import math
import re
import statistics

HERE = Path(__file__).resolve().parent
E = HERE.parents[1]
RAW = E / 'staging/runs/D_CPU_POSTCOMPLETION_TAIL'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')


def stats(values):
    ordered = sorted(values)
    if not ordered:
        return None
    return {'n':len(values), 'min_ns':ordered[0], 'mean_ns':statistics.mean(values),
            'median_ns':statistics.median(values), 'p95_ns':ordered[math.ceil(.95 * len(values)) - 1],
            'max_ns':ordered[-1], 'sum_ns':sum(values), 'percentile_method':'nearest_rank'}


def identity(row, retired=False):
    keys = ('retired_job','retired_request','retired_draw','retired_sim','retired_lease',
            'retired_submission','retired_event','retired_runtime_index') if retired else (
            'job','job_request','job_draw','job_sim','lease','submission','event','runtime_index')
    return tuple(row[k] for k in keys)


def overlap(a, b, c, d):
    return max(0, min(b, d) - max(a, c))


def main():
    text = (RAW / 'engine.log').read_text()
    lines = text.splitlines()
    records, buffers, emissions, errors = [], [], [], []
    current = None
    budget_bytes = 0
    for number, line in enumerate(lines, 1):
        for tag in ('NativeCPUTraceBuffer', 'NativeCPUTraceEmission', 'NativeCPUTrace'):
            token = '[' + tag + ']'
            if token not in line:
                continue
            payload = line[line.index(token):]
            pairs = re.findall(r'(\w+)=([^\s]+)', payload)
            if len(pairs) != len(dict(pairs)):
                errors.append(['duplicate_field', number])
            row = {k:float(v) if k.endswith('_us') or re.fullmatch(r'p\d+', k) else int(v) for k,v in pairs}
            row['line'] = number
            budget_bytes += len(payload.encode()) + 160
            if tag == 'NativeCPUTraceBuffer':
                row['observed_record_ids'] = []
                buffers.append(row)
                current = row
            elif tag == 'NativeCPUTraceEmission':
                emissions.append(row)
                current = None
            else:
                records.append(row)
                if current is None:
                    errors.append(['record_without_manifest', row['id']])
                else:
                    current['observed_record_ids'].append(row['id'])
                    row['buffer_request'] = current['request']
                    row['buffer_job'] = current['job']
    byid = {row['id']:row for row in records}
    if len(byid) != len(records):
        errors.append(['duplicate_ids'])
    for row in records:
        if row['end_ns'] < row['start_ns']:
            errors.append(['negative_interval', row['id']])
        if row['parent'] and row['parent'] not in byid:
            errors.append(['missing_parent', row['id'], row['parent']])
    for buf in buffers:
        if len(buf['observed_record_ids']) != buf['records'] or buf['dropped'] or buf['records'] > 128:
            errors.append(['buffer_incomplete', buf['request'], buf['job']])
    if sum(x['buffers'] for x in emissions) != len(buffers) or not emissions:
        errors.append(['emission_cardinality'])
    if any(x['outside_window'] != 1 for x in emissions):
        errors.append(['emission_not_outside_window'])
    if budget_bytes >= 4 * 1024 * 1024 or len(records) + len(buffers) + len(emissions) >= 8192:
        errors.append(['global_cap_reached'])
    if sorted({r['request_sim'] for r in records}) != list(range(900,916)):
        errors.append(['missing_ROI_simulation'])
    jobs = collections.defaultdict(list)
    for row in records:
        if row['buffer_job']:
            jobs[row['buffer_job']].append(row)
    submits = {identity(r):r for r in records if r['kind'] == 25}
    if len(submits) != sum(r['kind'] == 25 for r in records):
        errors.append(['duplicate_submission_identity'])
    chains, boundary = [], []
    for job, group in sorted(jobs.items()):
        def one(kind):
            selected = [r for r in group if r['kind'] == kind]
            if len(selected) != 1:
                raise ValueError(('ambiguous_scope', job, kind, len(selected)))
            return selected[0]
        scope = {k:one(k) for k in (6,18,19,20,21,22,23,24,25,26,29,3,27)}
        retire, feedback, submit, encode = (scope[k] for k in (18,20,25,6))
        pred = submits.get(identity(retire, retired=True))
        if pred is None:
            boundary.append({'classification':'MISSING_UNSAMPLED_PRE_ROI_PREDECESSOR',
                'successor_job':job, 'request_sim':submit['request_sim'], 'retire':retire,
                'submit':submit, 'included_in_exact_chain_statistics':False})
            if job != min(jobs) or retire['status'] != 4 or retire['retired_job'] != 0:
                errors.append(['internal_missing_predecessor', job])
            continue
        local_errors = []
        common_parent = submit['parent']
        if any(scope[k]['parent'] != common_parent for k in (6,18,24,29,26)):
            local_errors.append('successor_ancestry_mismatch')
        if byid[common_parent]['kind'] != 3 or byid[byid[common_parent]['parent']]['kind'] != 27:
            local_errors.append('worker_parentage_mismatch')
        for k in (19,20,21,22,23):
            r = scope[k]
            if r['parent'] != retire['id'] or identity(r,True) != identity(pred):
                local_errors.append('retired_child_identity_mismatch_' + str(k))
            if not retire['start_ns'] <= r['start_ns'] <= r['end_ns'] <= retire['end_ns']:
                local_errors.append('retired_child_interval_mismatch_' + str(k))
        for r in scope.values():
            if any(r[k] != submit[k] for k in ('job','job_request','job_draw','job_sim','request','request_draw','request_sim')):
                local_errors.append('successor_job_identity_mismatch')
            if r['status'] != 0:
                local_errors.append('nonordinary_status_' + str(r['kind']))
        if not all(identity(pred)) or not all(identity(submit)):
            local_errors.append('incomplete_submission_identity')
        ordered = (pred['end_ns'] <= retire['start_ns'] <= scope[19]['start_ns'] <= scope[19]['end_ns'] <=
                   feedback['start_ns'] <= feedback['end_ns'] <= scope[21]['start_ns'] <= scope[21]['end_ns'] <=
                   scope[22]['start_ns'] <= scope[22]['end_ns'] <= scope[23]['start_ns'] <= scope[23]['end_ns'] <=
                   retire['end_ns'] <= scope[29]['start_ns'] <= scope[29]['end_ns'] <= submit['start_ns'] <= submit['end_ns'])
        if not ordered:
            local_errors.append('order_mismatch')
        if not encode['end_ns'] <= scope[24]['start_ns'] <= scope[24]['end_ns'] <= retire['start_ns']:
            local_errors.append('early_drain_or_resource_commit_order')
        start, end = feedback['end_ns'], submit['start_ns']
        components = {str(k):overlap(start,end,scope[k]['start_ns'],scope[k]['end_ns']) for k in (21,22,23,29)}
        intervals = sorted((max(start,scope[k]['start_ns']),min(end,scope[k]['end_ns'])) for k in (21,22,23,29))
        merged = []
        for a,b in intervals:
            if b <= a:
                continue
            if merged and a <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], b)
            else:
                merged.append([a,b])
        union = sum(b-a for a,b in merged)
        if local_errors:
            errors.append(['chain_invalid', job, local_errors])
        chains.append({'classification':'EXACT_ORDINARY' if not local_errors else 'INVALID',
            'predecessor':pred, 'successor':submit, 'scopes':{str(k):v for k,v in scope.items()},
            'tail_ns':end-start, 'submit_call_ns':submit['end_ns']-submit['start_ns'],
            'component_intersections_ns':components, 'component_union_ns':union,
            'unclassified_tail_ns':end-start-union,
            'submit_to_submit_ns':submit['start_ns']-pred['start_ns'],
            'tail_overlaps_trace_emission_ns':sum(overlap(start,end,x['start_ns'],x['end_ns']) for x in emissions),
            'encode_finished_before_retirement':encode['end_ns'] <= retire['start_ns'],
            'resource_commit_precedes_tail':scope[24]['end_ns'] <= start,
            'errors':local_errors})
    observed = (RAW / 'CHILD-ENV-OBSERVED.txt').read_text()
    actual = dict(word.split('=',1) for word in observed.split() if '=' in word)
    env = json.loads((RAW / 'ENV.json').read_text())
    allow = {k:v for k,v in env.items() if k.startswith(('BAR_NATIVE_','BAR_METAL4_','MTL_'))}
    mismatches = [k for k,v in allow.items() if actual.get(k) != v]
    unset = ['BAR_NATIVE_INTERVENTION_HOST_VERTEX','BAR_NATIVE_COMBINED_OVERLAP220','BAR_METAL4_SEPIA_PATH']
    if mismatches or any(k in actual for k in unset):
        errors.append(['actual_child_environment_mismatch', mismatches])
    save('CHILD-ENV-ALLOWLIST.json', {'status':'PASS' if not mismatches else 'MISMATCH',
        'observed':{k:actual.get(k) for k in allow}, 'expected':allow,
        'unset':{k:k not in actual for k in unset}, 'raw_local_sha256':sha(RAW / 'CHILD-ENV-OBSERVED.txt'),
        'raw_publication':'LOCAL_ONLY; allowlist excludes HOME, USER, LOGNAME, PATH, TMPDIR, unrelated env and full argv'})
    anchors = re.findall(r'\[MatchedAnchor\] boundary=(start|end) [^\n]*simulation=(\d+)',text)
    scene = {
        'anchors':anchors,
        'same_start_script':sha(RAW / 'actual-start-script.txt')=='a4e0a94af13c43d7b26431b42cdb7808f43b9776ade16a750856573b4dc8941c',
        'rng_seed_154658555':'fixedrngseed=154658555;' in (RAW / 'actual-start-script.txt').read_text(),
        'benchmark_seed_7654321':'command_frame=240 seed=7654321 command=luarules_benchmark_armbull_armbull_80_8_1400' in text,
        'ultra4_AA0':'graphicsPreset=ultra selected_index=4 view=1512,950 window=1512,982 MSAA=0 MSAALevel=0' in text,
        'fullview_at_240_and_540':all(f'frame={f} spectating=true fullview=true' in text for f in [240,540]),
        'pacing_removed_before_count':'event=removed_before_count simulation=180' in text,
        'native_color_1512x982':'active=1 samples=1 present_order_original=1 width=1512 height=982' in text,
        'native_postcount_image_succeeded':'[H1NativeImage] success=true simulation=1740' in text,
        'hard_failure_lines':[l for l in lines if re.search(r'FAILED_|failed assertion|GPU reported deferred failure|GPU completion shared-event timeout|MTL4 commit feedback timeout',l,re.I)],
        'metal_validation_enabled':'Metal API Validation Enabled' in text,
        'G01_probe_rows':text.count('[NativeGroundDecalIndexG01]'),
        'camera_open':[l for l in lines if '[BattleCameraOpen]' in l],
        'camera_at_postcount':[l for l in lines if '[H1ValidationFrame]' in l],
        'warmup_receipts':[l for l in lines if '[WarmupPacing]' in l or 'combat_warmup_wall_seconds=' in l],
        'counted_wall_seconds':float(re.search(r'\[MatchedAnchor\] boundary=end seconds=([\d.]+)',text).group(1)),
        'diagnostic_FPS':'NOT_COMPUTED_OR_ACCEPTED; trace overhead and all original bins retained'}
    if anchors != [('start','540'),('end','1740')] or scene['hard_failure_lines'] or scene['metal_validation_enabled'] or scene['G01_probe_rows']:
        errors.append(['scene_or_disabled_probe_failure'])
    if any(value is False for key,value in scene.items() if key != 'metal_validation_enabled'):
        errors.append(['scene_check_false'])
    save('SCENE-CHECKS.json',scene)
    completeness = {'trace_rows':len(records),'buffers':len(buffers),'buffer_manifests':buffers,
        'emissions':emissions,'budget_records':len(records)+len(buffers)+len(emissions),'budget_bytes':budget_bytes,
        'maximum_buffer_records':max(b['records'] for b in buffers),'dropped':sum(b['dropped'] for b in buffers),
        'duplicate_ids':len(records)-len(byid),'missing_parent_ids':sorted({r['parent'] for r in records if r['parent'] and r['parent'] not in byid}),
        'kind_counts':dict(collections.Counter(r['kind'] for r in records)),
        'status_counts':dict(collections.Counter(r['status'] for r in records)),
        'nonzero_status_rows':[r for r in records if r['status'] not in (0,1)],
        'no_job_status_1_count':sum(r['status']==1 for r in records),
        'request_simulations':sorted({r['request_sim'] for r in records}),
        'worker_jobs':len(jobs),'exact_chains':len(chains),'boundary_censored':boundary,
        'right_boundary':'Last sampled submission job34 has no sampled future successor; no future tail inferred',
        'errors':errors}
    save('TRACE-COMPLETENESS.json',completeness)
    save('CHAINS.json',{'clock':'CPU monotonic nanoseconds','raw':'engine.log','chains':chains,'boundary_censored':boundary})
    good = [x for x in chains if x['classification']=='EXACT_ORDINARY']
    summary = {'status':'INSUFFICIENT' if errors else 'EXACT_INTERIOR_CHAINS_WITH_BOUNDARY_LIMITS',
        'round_id':'cpu-postcompletion-tail-20261117-21','attempt_id':'cpu-postcompletion-tail-01',
        'source_commit':'c34129af9158bbdef1321d17825f1f60721b74ec',
        'jobs':len(jobs),'exact_chains':len(good),'boundary_censored_count':len(boundary),
        'trace_rows':len(records),'buffers':len(buffers),'dropped':completeness['dropped'],
        'ROI_request_simulations':completeness['request_simulations'],'errors':errors,
        'postcompletion_tail':stats([x['tail_ns'] for x in good]),
        'submit_call':stats([x['submit_call_ns'] for x in good]),
        'components':{k:stats([x['component_intersections_ns'][k] for x in good]) for k in ['21','22','23','29']},
        'component_union':stats([x['component_union_ns'] for x in good]),
        'unclassified_tail':stats([x['unclassified_tail_ns'] for x in good]),
        'event_wait_not_savings':stats([x['scopes']['19']['end_ns']-x['scopes']['19']['start_ns'] for x in good]),
        'feedback_wait_not_savings':stats([x['scopes']['20']['end_ns']-x['scopes']['20']['start_ns'] for x in good]),
        'submit_to_submit':stats([x['submit_to_submit_ns'] for x in good]),
        'emission_overlap_jobs':[x['successor']['job'] for x in good if x['tail_overlaps_trace_emission_ns']],
        'all_exact_chains_included_in_statistics':True,'early_drains_in_exact_chains':sum(not x['encode_finished_before_retirement'] for x in good),
        'performanceAccepted':False,'retries':0,'game_count':1,
        'interpretation':'Only the exact interior chains are quantified; no full-boundary coverage or exact GPU idle claim. Last admitted job overlaps first post-window emission and is retained. No optimization or FPS gain is established.'}
    save('SUMMARY.json',summary)
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    main()
