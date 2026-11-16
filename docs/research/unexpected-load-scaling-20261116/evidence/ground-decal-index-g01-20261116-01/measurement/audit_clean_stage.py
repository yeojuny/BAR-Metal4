#!/usr/bin/env python3
"""No-GPU identity check of the six private G01 clean arms."""
import hashlib
import json
from pathlib import Path

M = Path(__file__).resolve().parent
root = M / 'runs/g01_clean01/clean_paced'
prepared = json.loads((root / 'PREPARED.json').read_text())
expected_order = [('A1_OFF', '0'), ('B1_ON', '1'), ('B2_ON', '1'),
                  ('A2_OFF', '0'), ('A3_OFF', '0'), ('B3_ON', '1')]
expected_binary = '962637cd9a0d71bb66896bedb0b807f2940097ae5cb58c2521eebd93bbb98559'
assert prepared['status'] == 'READY_NO_GAME'
assert [row['arm'] for row in prepared['order']] == [row[0] for row in expected_order]
binary = Path(json.loads((root / 'preparation/runs/A1_OFF/LAUNCH.json').read_text())['product']['binary'])
assert hashlib.sha256(binary.read_bytes()).hexdigest() == expected_binary
reference_checksums = reference_flags = reference_settings = None
profiles = []
for row, (arm, feature) in zip(prepared['order'], expected_order):
    assert row['arm'] == arm and row['binarySHA256'] == expected_binary
    out = Path(row['out'])
    plan = json.loads((out / 'LAUNCH.json').read_text())
    assert plan['product']['sha256'] == expected_binary
    assert plan['env']['BAR_NATIVE_GROUND_DECAL_INDEX_G01'] == feature
    assert plan['env']['BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE'] == '0'
    required = {'BAR_NATIVE_GRASS_BOUNDARY_S01':'1',
                'BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE':'0',
                'BAR_NATIVE_PREPARE_READY_PILOT':'1',
                'BAR_NATIVE_BEGIN_READY_PILOT':'0',
                'BAR_NATIVE_MATERIAL_STORAGE_POOL':'0',
                'BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP':'0',
                'BAR_NATIVE_SHARED_PAIR_KEY_VIEW':'0',
                'BAR_METAL4_SHARED_CUS_PILOT':'0',
                'BAR_NATIVE_FRAME_TRACE':'0',
                'MTL_DEBUG_LAYER':'0', 'MTL_SHADER_VALIDATION':'0'}
    assert all(plan['env'].get(k) == v for k,v in required.items())
    settings = plan['ultra']['settings']
    assert all(settings[k] == v for k,v in {'MSAA':0, 'VSync':0, 'VSyncGame':0,
                                           'MinSampleShadingRate':0, 'LimitIdleFps':0}.items())
    assert plan['ultra']['expectedView'] == [1512,950]
    assert plan['ultra']['actualSamples'] == 0
    assert not (out / 'OWNED.json').exists()
    normalized_checksums = {str(path).replace('/private/'+arm+'/', '/private/{arm}/'): value
                            for path,value in plan['checksums'].items()}
    normalized_flags = {key:value for key,value in plan['env'].items()
                        if (key.startswith('BAR_') or key.startswith('MTL_'))
                        and key not in ('BAR_NATIVE_GROUND_DECAL_INDEX_G01','BAR_STATE_PATH','BAR_ASSETS_PATH')}
    if reference_checksums is None:
        reference_checksums = normalized_checksums
        reference_flags = normalized_flags
        reference_settings = settings
    else:
        assert normalized_checksums == reference_checksums
        assert normalized_flags == reference_flags
        assert settings == reference_settings
    profiles.append({'arm':arm, 'feature':feature, 'output':str(out),
                     'profile':str(row['data']), 'binarySHA256':row['binarySHA256'],
                     'controllerSHA256':row['controllerSHA256'],
                     'configSHA256':row['configSHA256'],
                     'gameArchiveSHA256':row['gameArchiveSHA256'],
                     'mapSHA256':row['mapSHA256']})
assert len({p['profile'] for p in profiles}) == 6
assert len({p['output'] for p in profiles}) == 6
receipt = {'status':'READY_NO_GAME', 'sameSignedEngine':expected_binary,
           'order':profiles, 'sourceChecksumsIdenticalAcrossSix':True,
           'commonChildFlagsExceptG01Identical':True,
           'commonConfigAndSettingsIdentical':True,
           'pacingHashes':prepared['pacingHashes'],
           'intendedDifference':'BAR_NATIVE_GROUND_DECAL_INDEX_G01=0/1 only; run IDs, profile and output paths are private',
           'countedWindow':'sim540..1740, 40s after sim240..540 combat warmup',
           'noGamesYet':True}
(M / 'CLEAN-READY.json').write_text(json.dumps(receipt,indent=2)+'\n')
print('READY_NO_GAME six arms identical except G01 flag and private paths')
