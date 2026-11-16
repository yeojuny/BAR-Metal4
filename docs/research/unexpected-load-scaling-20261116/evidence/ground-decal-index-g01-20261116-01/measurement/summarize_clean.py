#!/usr/bin/env python3
"""Postprocess only the completed G01 clean six raw receipts."""
import csv
import json
import statistics
from pathlib import Path

m = Path(__file__).resolve().parent
run = m / 'runs/g01_clean01/clean_paced'
rows = json.loads((run / 'RUNS.json').read_text())
order = ['A1_OFF', 'B1_ON', 'B2_ON', 'A2_OFF', 'A3_OFF', 'B3_ON']
assert [row['run'] for row in rows] == order
result = []
for row in rows:
    out = run / 'preparation/runs' / row['run']
    child = json.loads((out / 'CHILD-MODE.json').read_text())
    cleanup = json.loads((out / 'CLEANUP.json').read_text())
    pacing = json.loads((out / 'PACING-WITNESS.json').read_text())
    ultra = json.loads((out / 'ULTRA-EVIDENCE.json').read_text())
    item = {
        'arm': row['run'],
        'G01': child['actual']['BAR_NATIVE_GROUND_DECAL_INDEX_G01'],
        'meanFPS': row['FPS'],
        'low1FPS': row['frameStats']['slow1PercentFPS'],
        'p99Ms': row['frameStats']['p99Ms'],
        'maxMs': row['frameStats']['maximumMs'],
        'seconds': row['seconds'],
        'draws': row['draws'],
        'created': ultra['officialCreated'],
        'destroyed': ultra['officialDestroyed'],
        'normalExit': cleanup['normalExit'],
        'ownedGroupGone': cleanup['ownedGroupGone'],
        'pacing': pacing['status'],
        'power': ultra['powerWitness'],
        'image': str(out / 'scene.png'),
    }
    assert item['normalExit'] and item['ownedGroupGone'] and item['pacing'] == 'PASS'
    assert (item['created'], item['destroyed']) == (600, 433)
    assert 'LimitIdleFps=0' in item['power'] and 'option_limitoffscreenfps=false' in item['power']
    assert Path(item['image']).is_file()
    result.append(item)
off = [item for item in result if item['G01'] == '0']
on = [item for item in result if item['G01'] == '1']
assert len(off) == len(on) == 3
receipt = {
    'status': 'COMPLETE_CLOSED',
    'engineSHA256': '962637cd9a0d71bb66896bedb0b807f2940097ae5cb58c2521eebd93bbb98559',
    'order': order,
    'meanFPS': {'OFF': statistics.mean(item['meanFPS'] for item in off),
                'ON': statistics.mean(item['meanFPS'] for item in on)},
    'meanLow1FPS': {'OFF': statistics.mean(item['low1FPS'] for item in off),
                   'ON': statistics.mean(item['low1FPS'] for item in on)},
    'allValidRunsRetained': True,
    'historicalPooling': False,
    'rows': result,
    'sourceRuns': str(run / 'RUNS.json'),
    'sourceCompletion': str(run / 'COMPLETION.json'),
}
(m / 'CLEAN-RESULT.json').write_text(json.dumps(receipt, indent=2) + '\n')
with (m / 'CLEAN-FPS.csv').open('w', newline='') as handle:
    fields = ['arm', 'G01', 'meanFPS', 'low1FPS', 'p99Ms', 'maxMs', 'seconds', 'draws']
    writer = csv.DictWriter(handle, fieldnames=fields)
    writer.writeheader()
    for item in result:
        writer.writerow({field: item[field] for field in fields})
print(receipt['meanFPS'], receipt['meanLow1FPS'])
