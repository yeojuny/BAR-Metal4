"""PR7 official Seths tank battle: storage OFF/ON diagnostics, then clean ABBA."""

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import threading
import traceback

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parents[2]
BASE_RUNNER = EVIDENCE / 'screen-state-elision-20261115-01/measurement/run_ac0_prelaunch.py'
Q_PACING = EVIDENCE / 'shared-program-v2-20261115-01/measurement/pacing'
MAP_SOURCE = Path('${AUDIT_USER_HOME}/Library/Application Support/BAR-Run91/assets/maps/seths_ravine_remake_1.3.1.sd7')
MAP_SHA256 = 'cd674ba4e1a1ced8524fe0b9bf1abb33170a04026289a139bbd3dbc17c2bb887'
GADGET_SOURCE = EVIDENCE / 'material-identity-pr6-20261115-01/battle-scene/dbg_benchmark.lua'
GADGET_SHA256 = '403e769e0df9a3224932266754ace7610ea04b7ab46ca3e6192274bd9ae56147'
COMMAND = 'luarules benchmark armbull armbull 80 8 1400'
ORDERS = {
    'diagnostic_paced': [('D0_BATTLE_STORAGE_OFF', 'OFF'), ('D1_BATTLE_STORAGE_ON', 'ON')],
    'clean_paced': [('A1_BATTLE_STORAGE_OFF', 'OFF'), ('B1_BATTLE_STORAGE_ON', 'ON'),
                    ('B2_BATTLE_STORAGE_ON', 'ON'), ('A2_BATTLE_STORAGE_OFF', 'OFF')],
}
STORAGE_SCHEMA = (43, 44, 45, 46, 47, 48, 49, 50)

spec = importlib.util.spec_from_file_location('s_profile_base', BASE_RUNNER)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
base.HERE = HERE
base.PRODUCT = HERE / 'PRODUCT.json'
base.RELEASE_CONTRACT = HERE / 'RELEASE-CONTRACT.json'
base.PACING = Q_PACING
base.ORDERS.update({key: [(arm, 'ELISION1') for arm, _ in value] for key, value in ORDERS.items()})
base.PACED = tuple(ORDERS)
base.VALIDATION = ()
base.SCREEN = ()
base.CLEAN = ('clean_paced',)
base.CHILD_KEYS += ('BAR_NATIVE_MODEL_TYPE_FACTS', 'BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP',
                    'BAR_NATIVE_MATERIAL_IDENTITY_DIAGNOSTICS',
                    'BAR_NATIVE_MATERIAL_STORAGE_POOL', 'BAR_NATIVE_MATERIAL_STORAGE_DIAGNOSTICS',
                    'BAR_NATIVE_SHARED_PAIR_KEY_VIEW', 'BAR_METAL4_SHARED_CUS_PILOT',
                    'BAR_METAL4_SEPIA_PATH', 'BAR_NATIVE_FRAME_TRACE_START_SIM',
                    'BAR_NATIVE_FRAME_TRACE_END_SIM')
shared_spec = importlib.util.spec_from_file_location('pr7_storage_common', HERE.parent / 'storage_common.py')
storage = importlib.util.module_from_spec(shared_spec)
shared_spec.loader.exec_module(storage)
memory = storage.MemoryRecorder(base)

IMAGE_SOURCE = EVIDENCE / 'h1-pool220-runtime01/native_image.py'
base.image = {'__file__': str(IMAGE_SOURCE), '__name__': 'battle_image'}
exec(compile(IMAGE_SOURCE.read_text().replace('2250', '1740'), str(IMAGE_SOURCE), 'exec'), base.image)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


metrics = base.common.load(base.common.OLD / 'run/metrics.py', 'battle_metrics')


def battle_parse(directory, data, arm, stage):
    directory = Path(directory)
    data = Path(data)
    info = (directory / 'infolog.txt').read_text()
    assert not re.search(r'\[MatchedGameplay\] FAILED|\[BattleBenchmark\] (?:wrong_map|armbull_absent)|Error in GameFrame\(\)', info)
    assert sha(data / 'script.txt') == sha(HERE / 'start.txt')
    assert info.count('Starting Benchmark') == 1, 'Official benchmark did not start once'
    assert re.findall(r'\[BattleBenchmark\] command_frame=(\d+) seed=(\d+) command=([^\s]+)', info) == [
        ('240', '7654321', 'luarules_benchmark_armbull_armbull_80_8_1400')]
    assert re.findall(r'\[BattleBenchmark\] stop_after_count=(\d+) frame=(\d+)', info) == [('1', '1740')]
    initial = re.findall(r'\[BattleInitialState\] (.+)', info)
    assert len(initial) == 1 and 'frame=240 map=Seths Ravine Remake 1.3.1 mapx=8192 mapz=6144 units=' in initial[0]
    camera = re.findall(r'\[BattleCameraOpen\] (.+)', info)
    assert len(camera) == 1 and 'view=1512,950,0,0' in camera[0]
    created = re.findall(r'Benchmark complete, #created = (\d+), #destroyed = (\d+)', info)
    assert len(created) == 1 and int(created[0][0]) > 0 and int(created[0][1]) > 0, 'Actual unit combat absent'
    anchors = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in re.findall(r'\[MatchedAnchor\] (.+)', info)]
    assert [(a['boundary'], a['simulation'], a['callback']) for a in anchors] == [
        ('start', '540', 'GameFrame'), ('end', '1740', 'GameFrame')]
    total = re.findall(r'\[NativeShortRun\] total draws=(\d+) seconds=([\d.]+)', info)
    assert len(total) == 1
    draws, seconds = int(total[0][0]), float(total[0][1])
    samples = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in re.findall(r'\[MatchedFPS\] (.+)', info)]
    assert 39 <= len(samples) <= 41 and 39.9 < seconds < 40.1
    assert draws == sum(int(s['draws']) for s in samples)
    assert int(samples[0]['start_simulation']) == 540 and int(samples[-1]['end_simulation']) == 1740
    assert all(a['end_draw'] == b['start_draw'] and a['end_simulation'] == b['start_simulation']
               for a, b in zip(samples, samples[1:]))
    assert all(s['paused'] == '0' and float(s['speed_a']) == 1 and float(s['speed_b']) > 0 and
               int(s['draws']) == int(s['end_draw']) - int(s['start_draw']) and
               float(s['interval_seconds']) > 0 and s['camera'] == camera[0]
               for s in samples), 'Counted speed/camera/draw continuity changed'
    window = base.common.read(directory / 'WINDOW.json')
    assert abs((window['end'] - window['start']) / 1e9 - seconds) < 0.10
    assert int(anchors[1]['draw']) - int(anchors[0]['draw']) == draws
    frame_stats = metrics.validate_frames(directory, info, samples)
    return dict(run=arm, FPS=draws / seconds, draws=draws, seconds=seconds,
                samples=samples, frameStats=frame_stats, window=window,
                battleInitialState=initial[0], battleCameraOpen=camera[0],
                officialCreated=int(created[0][0]), officialDestroyed=int(created[0][1]),
                battleCommand=COMMAND, gameSeed=154658555, benchmarkSeed=7654321,
                battleMap='Seths Ravine Remake 1.3.1',
                source='Official current local26 dbg_benchmark.lua; no AI or extra commander give')


base.harness.parse = battle_parse


def save(path, value):
    base.common.save(path, value)


def stage(campaign):
    assert campaign in ORDERS
    assert (HERE / 'PRODUCT.json').is_file() and (HERE / 'RELEASE-CONTRACT.json').is_file(), 'Signed candidate pending'
    assert sha(MAP_SOURCE) == MAP_SHA256 and sha(GADGET_SOURCE) == GADGET_SHA256
    overlay = HERE / 'bar-official-combat-pr6.sdz'
    listing = subprocess.check_output(['/usr/bin/unzip', '-Z1', str(overlay)], text=True).splitlines()
    assert listing == ['modinfo.lua'], listing
    prepared = base.prepare(campaign)
    stage_dir = HERE / campaign
    plan_rows = []
    for arm, mode in ORDERS[campaign]:
        data, out, env, plan = prepared[arm]
        controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
        shutil.copyfile(HERE / 'battle_control.lua', controller)
        plan['checksums'][str(controller)] = sha(controller)
        plan['stagedControllerSHA256'] = sha(controller)
        shutil.copyfile(HERE / 'start.txt', data / 'script.txt')
        plan['checksums'][str(data / 'script.txt')] = sha(data / 'script.txt')
        old_overlay = data / 'games/bar-commander-control-213c1.sdz'
        old_overlay.unlink()
        plan['checksums'].pop(str(old_overlay))
        staged_overlay = data / 'games/bar-official-combat-pr6.sdz'
        shutil.copyfile(overlay, staged_overlay)
        plan['checksums'][str(staged_overlay)] = sha(staged_overlay)
        staged_map = data.parents[1] / 'assets/maps/seths_ravine_remake_1.3.1.sd7'
        staged_map.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['/bin/cp', '-c', str(MAP_SOURCE), str(staged_map)], check=True)
        assert sha(staged_map) == MAP_SHA256
        plan['checksums'][str(staged_map)] = MAP_SHA256
        cfg = (data / 'LuaUI/Config/BC213.lua').read_text()
        assert 'Sepia Tone' not in cfg and not (data / 'LuaUI/Widgets/zzz_v2_functional_probe.lua').exists()
        assert re.search(r'\["Limit idle FPS"\]\s*=\s*0\b', cfg)
        settings = {
            'BAR_NATIVE_MODEL_TYPE_FACTS': '1',
            'BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP': '0',
            'BAR_NATIVE_MATERIAL_IDENTITY_DIAGNOSTICS': '0',
            'BAR_NATIVE_MATERIAL_STORAGE_POOL': str(int(mode == 'ON')),
            'BAR_NATIVE_MATERIAL_STORAGE_DIAGNOSTICS': str(int(campaign == 'diagnostic_paced')),
            'BAR_NATIVE_SHARED_PAIR_KEY_VIEW': '0',
            'BAR_METAL4_SHARED_CUS_PILOT': '0',
            'BAR_NATIVE_FRAME_TRACE': str(int(campaign == 'diagnostic_paced')),
            'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP': str(int(campaign == 'diagnostic_paced')),
            'MTL_DEBUG_LAYER': str(int(campaign == 'diagnostic_paced')),
            'MTL_SHADER_VALIDATION': '0',
        }
        if campaign == 'diagnostic_paced':
            settings.update(BAR_NATIVE_FRAME_TRACE_START_SIM='540',
                            BAR_NATIVE_FRAME_TRACE_END_SIM='546', MTL_LOG_TO_STDERR='1')
            base.image['configure'](controller)
            plan['checksums'][str(controller)] = base.common.sha(controller)
            plan['stagedControllerSHA256'] = base.common.sha(controller)
        else:
            assert all(env.get(key) in (None, '', '0') for key in
                ('BAR_NATIVE_FRAME_TRACE', 'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP',
                 'MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION'))
        env.pop('BAR_METAL4_SEPIA_PATH', None)
        plan['env'].pop('BAR_METAL4_SEPIA_PATH', None)
        env.update(settings)
        plan['env'].update(settings)
        plan['measurementWindow'] = dict(startSimulation=540, endSimulation=1740,
            warmupSeconds=10, countedSeconds=40, combatStartSimulation=240,
            performanceAccepted=campaign == 'clean_paced')
        plan['battleScene'] = dict(map='Seths Ravine Remake 1.3.1', mapSHA256=MAP_SHA256,
            gadgetSHA256=GADGET_SHA256, officialCommand=COMMAND,
            benchmarkSeed=7654321, gameSeed=154658555,
            startScriptSHA256=sha(data / 'script.txt'), overlaySHA256=sha(staged_overlay),
            stagedMap=str(staged_map), initialPositions={'team0': [2867, 3072], 'team1': [5325, 3072]},
            historicalUnitCountGate=False, noExtraCommanderGive=True)
        plan.update(materialStorageMode=mode, materialIdentityMode='OFF', modelTypeFacts='ON', sharedPairKeyView='OFF',
                    sepiaWidget='OFF', cusPrototype='OFF',
                    diagnosticTrace=campaign == 'diagnostic_paced',
                    metalValidation=campaign == 'diagnostic_paced',
                    imageCapture=campaign == 'diagnostic_paced',
                    performanceAccepted=campaign == 'clean_paced')
        save(out / 'LAUNCH.json', plan)
        plan_rows.append(dict(arm=arm, mode=mode, data=str(data), out=str(out),
                              controllerSHA256=plan['stagedControllerSHA256'],
                              childFlags={key: env.get(key) for key in base.CHILD_KEYS}))
    save(stage_dir / 'PR7-PLAN.json', dict(status='READY_NO_GAMES', campaign=campaign,
        product=base.pinned_product(), order=plan_rows,
        scene='Seths Ravine Remake 1.3.1 official local26 tank benchmark, f240 command, f540..1740 40s count, AA0 VSync0 1512x950',
        diagnostics='OFF/ON Metal Validation 1; bounded complete kinds43..50 storage groups; image paused sim1740 before official stop',
        batchSchema=STORAGE_SCHEMA,
        clean='OFF ON ON OFF; validation/trace/image off; only after diagnostic review',
        power='AC powermode0 required by existing preflight; runtime LimitIdleFps0 and offscreenfalse required',
        allValidRunsRetained=True, priorSamplesPooled=False))
    print('READY ' + campaign + '; no game launched', flush=True)


def trace_summary(info, mode):
    return storage.storage_trace(info, 540, 546, mode)


def preserve_diagnostic(out):
    """Persist raw functional evidence even when the FPS parser rejects a run."""
    info = (out / 'infolog.txt').read_text() if (out / 'infolog.txt').exists() else ''
    engine_log = (out / 'engine.log').read_text() if (out / 'engine.log').exists() else ''
    mode = 'ON' if base.common.read(out / 'LAUNCH.json')['env']['BAR_NATIVE_MATERIAL_STORAGE_POOL'] == '1' else 'OFF'
    trace = trace_summary(info, mode)
    validation = bool(re.search(r'Metal (?:API )?Validation (?:is )?Enabled', engine_log, re.I))
    errors = [line for line in engine_log.splitlines() if re.search(
        r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)',
        line, re.I)]
    save(out / 'MATERIAL-STORAGE-TRACE.json', trace)
    save(out / 'METAL-VALIDATION.json', dict(enabled=validation, errors=errors))
    save(out / 'DIAGNOSTIC-RAW.json', dict(imagePresent=(out / 'scene.png').is_file(),
        image=str(out / 'scene.png'), frameReceipt=bool(re.search(r'\[H1ValidationFrame\] sim=1740 ', info)),
        countedStart=bool(re.search(r'\[MatchedAnchor\] boundary=start .*simulation=540', info)),
        countedEnd=bool(re.search(r'\[MatchedAnchor\] boundary=end .*simulation=1740', info)),
        trace=trace, validationEnabled=validation, metalErrors=errors))
    return info, trace, validation, errors


def diagnostic_trace_valid(trace, mode):
    aggregate = trace.get('aggregate') or {}
    return (trace['windowAndModeValid'] and trace['status'] == 'VALID_WORK_COUNTS_ONLY'
            and aggregate.get('groups', 0) > 0 and aggregate.get('gets', 0) > 0
            and aggregate.get('new_buffers', 0) + aggregate.get('reused_buffers', 0)
                == aggregate.get('encodes', -1))


def run(campaign):
    assert campaign in ORDERS
    assert STORAGE_SCHEMA, 'PR7 batch counter contract absent'
    if campaign == 'clean_paced':
        gate = base.common.read(HERE / 'DIAGNOSTIC-GATE.json')
        assert gate['status'] == 'PASS' and gate['productSHA256'] == base.pinned_product()['sha256'], gate
    prepared = base.restore(campaign)
    assert base.common.read(HERE / campaign / 'PR7-PLAN.json')['status'] == 'READY_NO_GAMES'
    assert base.common.read(base.SHARED_AWAKE)['status'] == 'RELEASED'
    (HERE / campaign / 'AWAKE-BEFORE.json').write_bytes(base.SHARED_AWAKE.read_bytes())
    caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '2400', '-w', str(os.getpid())])
    awake = dict(status='ACTIVE', campaign=campaign, pid=caffeine.pid,
                 startUTC=base.utc(), maximumSeconds=2400, securitySettingsChanged=False)
    save(base.SHARED_AWAKE, awake)
    save(HERE / campaign / 'AWAKE.json', awake)
    rows = []
    try:
        for index, (arm, mode) in enumerate(ORDERS[campaign]):
            data, out, env, plan = prepared[arm]
            base.active.update(prepared=prepared[arm], mapped=False, campaign=campaign)
            stop = threading.Event()
            issues = []
            menu = threading.Thread(target=base.reference_runner.witness_menu, args=(out, stop, issues))
            image_worker = (threading.Thread(target=base.image['capture'], args=(data, out, stop, issues))
                            if campaign == 'diagnostic_paced' else None)
            menu.start()
            if image_worker:
                image_worker.start()
            try:
                print('OPEN ' + arm, flush=True)
                row = None
                launch_error = None
                try:
                    try:
                        row = base.harness.launch(HERE / campaign / 'preparation', arm,
                                                  prepared[arm], len(ORDERS[campaign])-index)
                    except BaseException:
                        launch_error = traceback.format_exc()
                finally:
                    stop.set()
                    menu.join()
                    if image_worker:
                        image_worker.join()
                    save(out / 'OBSERVERS.json', dict(menuErrors=issues,
                        imagePresent=(out / 'scene.png').is_file(),
                        image=str(out / 'scene.png') if (out / 'scene.png').is_file() else None))
                    memory_receipt = memory.flush(out)
                    if campaign == 'diagnostic_paced':
                        preserve_diagnostic(out)
                if launch_error:
                    save(out / 'POSTRUN-PARSER-OR-LAUNCH-ERROR.json', dict(error=launch_error))
                    cleanup = base.common.read(out / 'CLEANUP.json') if (out / 'CLEANUP.json').exists() else {}
                    if campaign != 'diagnostic_paced' or not cleanup.get('normalExit'):
                        raise RuntimeError('Owned engine did not prove normal exit; ' + launch_error)
                    row = dict(run=arm, FPS=None, frameStats=None, seconds=None, draws=None,
                               diagnosticFPSParse='FAILED_BUT_RAW_PRESERVED')
                assert base.active['mapped'], 'Actual child flags were not observed'
                info = (out / 'infolog.txt').read_text()
                power = re.findall(r'\[FPSPowerStatus\] (.+)', info)
                assert len(power) == 1 and all(value in power[0] for value in
                    ('frame=480', 'shortName=BC213', 'LimitIdleFps=0',
                     'option_limitoffscreenfps=false', 'VSync=0', 'VSyncGame=0'))
                assert re.search(r'\[NativeMRT164\].*encoded=1', info)
                if campaign == 'clean_paced':
                    assert row['draws'] > 0
                    pace = base.pacing_evidence(out, plan, info)
                else:
                    try:
                        pace = base.pacing_evidence(out, plan, info)
                    except BaseException:
                        pace = dict(status='OBSERVED_GATE_ERROR', error=traceback.format_exc())
                        save(out / 'PACING-OBSERVED-ERROR.json', pace)
                release = base.release_evidence(out, plan, info, 'PREPARE')
                screen = base.screen_state_evidence(out, info, True, False)
                trace = base.common.read(out / 'MATERIAL-STORAGE-TRACE.json') if campaign == 'diagnostic_paced' else None
                if trace is not None:
                    raw = base.common.read(out / 'DIAGNOSTIC-RAW.json')
                    assert raw['countedStart'] and raw['countedEnd'], 'Native count window absent'
                    assert (out / 'scene.png').is_file(), 'Diagnostic image absent'
                    assert re.search(r'\[H1ValidationFrame\] sim=1740 ', info), 'Image frame receipt absent'
                    assert raw['validationEnabled'], 'Metal Validation enable receipt absent'
                    assert not raw['metalErrors'], 'Metal Validation reported an error or fault'
                    assert diagnostic_trace_valid(trace, mode), 'Storage counter group/window/validity failed'
                else:
                    assert '[NativeCPUTrace]' not in info, 'Trace present in clean arm'
                    assert memory_receipt['roiSampleCount'] > 0, 'No sparse ROI process-memory sample'
                row.update(backend=mode, materialStorageMode=mode, materialIdentityMode='OFF', modelTypeFacts='ON',
                           sharedPairKeyView='OFF', powerWitness=power[0],
                           pacingWitness=pace, releaseWitness=release, screenWitness=screen,
                           materialStorageTrace=trace, processMemory=memory_receipt,
                           performanceAccepted=campaign == 'clean_paced',
                           visualImage=str(out / 'scene.png') if trace is not None else None)
                if campaign == 'clean_paced' and rows:
                    assert row['battleInitialState'] == rows[0]['battleInitialState'], 'Initial unit state changed across arms'
                    assert row['battleCameraOpen'] == rows[0]['battleCameraOpen'], 'Camera changed across arms'
                    assert (row['officialCreated'], row['officialDestroyed']) == (
                        rows[0]['officialCreated'], rows[0]['officialDestroyed']), 'Official battle workload changed across arms'
                save(out / 'RESULT.json', row)
                rows.append(row)
                save(HERE / campaign / 'RUNS.json', rows)
                if campaign == 'clean_paced':
                    print(f"DONE {arm}: {row['FPS']:.3f} FPS, 1% low {row['frameStats']['slow1PercentFPS']:.3f}", flush=True)
                else:
                    print(f'DONE {arm}: diagnostic image and trace saved, FPS excluded', flush=True)
            except BaseException:
                save(out / 'FAILURE.json', dict(arm=arm, error=traceback.format_exc(), utc=base.utc()))
                save(HERE / campaign / 'COMPLETION.json', dict(status='STOPPED_ON_FAILURE',
                    completed=len(rows), arm=arm, utc=base.utc()))
                raise
            finally:
                (data / 'LuaUI/Widgets/zzz_backend_common_status.lua').unlink(missing_ok=True)
        if campaign == 'diagnostic_paced':
            status = 'DIAGNOSTIC_REVIEW_REQUIRED'
            completion = dict(status=status, runs=len(rows), performanceAccepted=False,
                images=[r['visualImage'] for r in rows],
                errors={r['run']: base.common.read(HERE/campaign/'preparation/runs'/r['run']/'METAL-VALIDATION.json')['errors']
                        for r in rows}, utc=base.utc())
        else:
            means = {mode: statistics.mean(r['FPS'] for r in rows if r['backend'] == mode)
                     for mode in ('OFF', 'ON')}
            completion = dict(status='COMPLETE', runs=len(rows), means=means,
                onVsOffPercent=100*(means['ON']/means['OFF']-1),
                priorSamplesPooled=False, validSlowRunsRetained=True, utc=base.utc())
        save(HERE / campaign / 'COMPLETION.json', completion)
        print(json.dumps(completion, indent=2), flush=True)
    finally:
        try:
            base.cleanup(campaign, prepared)
            for data, _, _, plan in prepared.values():
                for target in (data / 'games/bar-official-combat-pr6.sdz',
                               data.parents[1] / 'assets/maps/seths_ravine_remake_1.3.1.sd7'):
                    if target.exists():
                        assert sha(target) == plan['checksums'][str(target)]
                        target.unlink()
        finally:
            if caffeine.poll() is None:
                caffeine.terminate()
                caffeine.wait(timeout=5)
            awake.update(status='RELEASED', endUTC=base.utc(), ownCaffeinateAlive=False)
            save(base.SHARED_AWAKE, awake)
            save(HERE / campaign / 'AWAKE.json', awake)
            save(HERE / campaign / 'AWAKE-RELEASED.json', awake)
            save(HERE / campaign / 'OWNER-CLOSED.json', dict(status='CLOSED', utc=base.utc(),
                awakeReleased=True, completed=len(rows)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('campaign', choices=tuple(ORDERS))
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--prepare-only', action='store_true')
    mode.add_argument('--window-open', action='store_true')
    args = parser.parse_args()
    stage(args.campaign) if args.prepare_only else run(args.campaign)
