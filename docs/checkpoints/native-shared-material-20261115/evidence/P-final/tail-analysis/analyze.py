#!/usr/bin/env python3
"""Recompute screen-state-elision frame tails from existing read-only CSVs."""
import csv
import hashlib
import json
import math
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path('$P/measurement')
OUT = Path(__file__).with_name('ANALYSIS.json')
RUNS = {
    'A1_OFF': 'clean_paced/preparation/runs/A1_ELISION0',
    'B1_ON': 'clean_paced/preparation/runs/B1_ELISION1',
    'B2_ON': 'clean_paced/preparation/runs/B2_ELISION1',
    'A2_OFF': 'clean_paced/preparation/runs/A2_ELISION0',
    'A3_OFF_SUPPLEMENTAL': 'supplemental_off_paced/preparation/runs/A3_ELISION0_SUPPLEMENTAL',
}

def close(a, b, tol=1e-6):
    assert abs(a-b) <= tol, (a,b)

def one(name, relative):
    path=ROOT/relative
    result=json.loads((path/'RESULT.json').read_text())
    raw=(path/'TERRAIN211-FRAMES.csv').read_bytes()
    assert hashlib.sha256(raw).hexdigest()==result['frameStats']['rawSHA256']
    records=list(csv.DictReader(raw.decode().splitlines()))
    assert records and list(records[0])==['draw','seconds']
    data=[{'draw':int(r['draw']),'seconds':float(r['seconds'])} for r in records]
    draws=[r['draw'] for r in data]
    seconds=[r['seconds'] for r in data]
    assert all(b==a+1 for a,b in zip(draws,draws[1:]))
    assert all(math.isfinite(x) and x>0 for x in seconds)
    assert len(data)==result['draws']-1==result['frameStats']['samples']
    close(sum(seconds),result['frameStats']['coveredSeconds'])
    samples=result['samples']
    assert len(samples)==40
    assert int(samples[0]['start_simulation'])==300 and int(samples[-1]['end_simulation'])==1500
    assert all(int(s['end_draw'])-int(s['start_draw'])==int(s['draws']) for s in samples)
    assert all(int(a['end_draw'])==int(b['start_draw']) for a,b in zip(samples,samples[1:]))
    assert sum(int(s['draws']) for s in samples)==result['draws']
    assert draws[0]==int(samples[0]['start_draw'])+2 and draws[-1]==int(samples[-1]['end_draw'])
    assert all(float(s['interval_seconds'])>0 and s['paused']=='0' for s in samples)
    assert result['pacingWitness']['status']=='PASS' and result['performanceAccepted']
    assert bool(result['screenStateWitness']['requestedElision'])==('_ON' in name)
    top_n=math.ceil(len(data)*.01)
    ranked=sorted(data,key=lambda r:r['seconds'],reverse=True)
    slow=ranked[:top_n]
    slow_sum=sum(r['seconds'] for r in slow)
    slow_fps=top_n/slow_sum
    p99=sorted(seconds)[math.ceil(.99*len(seconds))-1]*1000
    close(slow_fps,result['frameStats']['slow1PercentFPS'])
    close(p99,result['frameStats']['p99Ms'])
    close(max(seconds)*1000,result['frameStats']['maximumMs'])
    close(result['draws']/result['seconds'],result['FPS'])
    bins=Counter(round(s*1000) for s in seconds)
    def sample_index(draw):
        return next(i+1 for i,s in enumerate(samples)
                    if int(s['start_draw'])<draw<=int(s['end_draw']))
    ge8_by_second=Counter(sample_index(r['draw']) for r in data if r['seconds']>=.008-1e-9)
    streaks=[]; current=[]; elapsed=0.0
    slow10=[]
    for r in data:
        elapsed+=r['seconds']
        if r['seconds']>=.010-1e-9:
            slow10.append({'draw':r['draw'],'time_from_first_recorded_interval_s':round(elapsed,6),
                           'interval_ms':round(r['seconds']*1000,6),'counted_second':sample_index(r['draw'])})
        if r['seconds']>=.008-1e-9:
            if current and r['draw']!=current[-1]['draw']+1:
                streaks.append(current);current=[]
            current.append(r)
        elif current:
            streaks.append(current);current=[]
    if current:streaks.append(current)
    longest=sorted(streaks,key=lambda s:(len(s),sum(x['seconds'] for x in s)),reverse=True)[:4]
    return {'name':name,'source_result':str(path/'RESULT.json'),'source_intervals':str(path/'TERRAIN211-FRAMES.csv'),
        'mode':'ON' if '_ON' in name else 'OFF','supplemental':'SUPPLEMENTAL' in name,
        'mean_fps':result['FPS'],'draws':result['draws'],'counted_seconds':result['seconds'],
        'frame_interval_count':len(data),'frame_interval_covered_seconds':sum(seconds),
        'sample_count':len(samples),'first_simulation':int(samples[0]['start_simulation']),
        'last_simulation':int(samples[-1]['end_simulation']),
        'warmup_observed_seconds':result['warmupObservedSeconds'],
        'original_wall_gate_seconds':result['pacingWitness']['originalWallGateSeconds'],
        'pacing_module_sha256':result['pacingWitness']['moduleSHA256'],
        'pacing_widget_sha256':result['pacingWitness']['widgetSHA256'],
        'source_raw_sha256_verified':True,'zero_negative_nonfinite_intervals':0,
        'one_percent_rule':'ceil(0.01*N) slowest positive DrawScreen-to-DrawScreen intervals; FPS=count/sum(seconds)',
        'one_percent_count':top_n,'slowest_one_percent_total_ms':round(slow_sum*1000,6),
        'one_percent_low_fps':slow_fps,'p99_ms_nearest_rank':p99,'maximum_ms':max(seconds)*1000,
        'over_16_667_ms':sum(s>.016667 for s in seconds),
        'interval_histogram_ms':{str(k):bins[k] for k in sorted(bins)},
        'at_least_8_ms':sum(s>=.008-1e-9 for s in seconds),
        'at_least_10_ms':len(slow10),
        'slowest_one_percent_makeup_ms':{str(k):v for k,v in sorted(Counter(round(r['seconds']*1000) for r in slow).items())},
        'at_least_8_ms_by_counted_second':dict(sorted((str(k),v) for k,v in ge8_by_second.items())),
        'at_least_10_ms_intervals':slow10,
        'longest_consecutive_at_least_8_ms_streaks':[
            {'start_draw':s[0]['draw'],'end_draw':s[-1]['draw'],'count':len(s),
             'sum_ms':round(sum(r['seconds'] for r in s)*1000,6),
             'counted_seconds':sorted(set(sample_index(r['draw']) for r in s))}
            for s in longest]}

def pooled(named, raw):
    chosen=[raw[n] for n in named]
    intervals=[]
    for name in named:
        path=ROOT/RUNS[name]/'TERRAIN211-FRAMES.csv'
        intervals.extend(float(r['seconds']) for r in csv.DictReader(path.open()))
    k=math.ceil(.01*len(intervals))
    return {'members':named,'draws':sum(x['draws'] for x in chosen),
            'counted_seconds':sum(x['counted_seconds'] for x in chosen),
            'mean_fps':sum(x['draws'] for x in chosen)/sum(x['counted_seconds'] for x in chosen),
            'frame_intervals':len(intervals),'slowest_one_percent_count':k,
            'slowest_one_percent_total_ms':round(sum(sorted(intervals,reverse=True)[:k])*1000,6),
            'one_percent_low_fps':k/sum(sorted(intervals,reverse=True)[:k])}

def main():
    raw={name:one(name,relative) for name,relative in RUNS.items()}
    assert len({r['pacing_module_sha256'] for r in raw.values()})==1
    assert len({r['pacing_widget_sha256'] for r in raw.values()})==1
    original_off=pooled(['A1_OFF','A2_OFF'],raw)
    original_on=pooled(['B1_ON','B2_ON'],raw)
    a1,a2,a3=(raw[n] for n in ('A1_OFF','A2_OFF','A3_OFF_SUPPLEMENTAL'))
    out={'method':'Read-only recomputation from original RESULT.json and hashed TERRAIN211-FRAMES.csv, no game or new test.',
         'metric_limit':'CPU-observed DrawScreen-to-DrawScreen wall intervals, not GPU execution or display-present latency.',
         'runs':raw,'original_abba':{'off':original_off,'on':original_on,
            'mean_fps_relative_change_percent':100*(original_on['mean_fps']/original_off['mean_fps']-1),
            'supplemental_a3_excluded_from_pool':True},
         'off_tail_explanation':{
            'a1_vs_a2_slowest_74_total_excess_ms':round(a1['slowest_one_percent_total_ms']-a2['slowest_one_percent_total_ms'],6),
            'a1_vs_a3_slowest_74_total_excess_ms':round(a1['slowest_one_percent_total_ms']-a3['slowest_one_percent_total_ms'],6),
            'a1_at_least_8_ms_counted_seconds_1_and_14':{
                '1':a1['at_least_8_ms_by_counted_second'].get('1',0),
                '14':a1['at_least_8_ms_by_counted_second'].get('14',0)},
            'interpretation':'A1 has more and longer slow frame intervals, including bursts near counted seconds 1 and 14. This fully explains its lower reciprocal-mean slowest-1% statistic; saved intervals cannot identify the underlying runtime cause.'},
         'comparison_limit':'A3 is a separately authorized OFF replicate, not a member of the original ABBA pool. Do not drop A1 or infer an ON effect from the later OFF variation.'}
    OUT.write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({'output':str(OUT),'original_off':original_off,'original_on':original_on,
                      'off_runs':{n:{k:raw[n][k] for k in ('mean_fps','one_percent_low_fps','slowest_one_percent_total_ms','at_least_8_ms','at_least_10_ms')}
                                  for n in ('A1_OFF','A2_OFF','A3_OFF_SUPPLEMENTAL')}},indent=2))

if __name__=='__main__': main()
