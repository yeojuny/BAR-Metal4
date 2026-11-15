"""One S diagnostic pair, then a separately authorized same-binary clean ABBA."""

import argparse
import importlib.util
import inspect
import json
import os
from pathlib import Path
import re
import statistics
import subprocess
import sys
import threading
import traceback

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parents[2]
BASE_RUNNER = EVIDENCE / 'screen-state-elision-20261115-01/measurement/run.py'
Q_PACING = EVIDENCE / 'shared-program-v2-20261115-01/measurement/pacing'
ORDERS = {
    'diagnostic_power1_paced': [('D0_POWER1_OFF', 'OFF'), ('D1_POWER1_ON', 'ON')],
}

spec = importlib.util.spec_from_file_location('s_profile_base', BASE_RUNNER)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
base.HERE = HERE
base.PRODUCT = HERE.parent / 'PRODUCT.json'
base.RELEASE_CONTRACT = HERE.parent / 'RELEASE-CONTRACT.json'
base.PACING = Q_PACING
base.ORDERS.update({key: [(arm, 'ELISION1') for arm, _ in value] for key, value in ORDERS.items()})
base.PACED = tuple(ORDERS)
base.VALIDATION = ()
base.SCREEN = ()
base.CLEAN = ()
base.CHILD_KEYS += ('BAR_NATIVE_MODEL_TYPE_FACTS', 'BAR_METAL4_SHARED_CUS_PILOT',
                    'BAR_METAL4_SEPIA_PATH', 'BAR_NATIVE_FRAME_TRACE_START_SIM',
                    'BAR_NATIVE_FRAME_TRACE_END_SIM')

# Diagnostic-only amendment: retain the proven P preflight and observe its AC
# mode instead of treating a user power-setting change as a functional failure.
preflight_source = inspect.getsource(base.preflight).splitlines()
power_gate = [i for i, line in enumerate(preflight_source)
              if line.strip().startswith('assert "\'AC Power\'" in battery.splitlines()[0]')]
assert len(power_gate) == 1
preflight_source[power_gate[0]] = (
    "    ac_mode_match = re.search(r'^\\s*powermode\\s+(\\d+)\\s*$', settings, re.M)")
policy_line = [i for i, line in enumerate(preflight_source)
               if "powerPolicy='AC powermode 1 required" in line]
assert len(policy_line) == 1
preflight_source[policy_line[0]] = (
    "                  powerPolicy='Diagnostic observation only; power mode is not an acceptance gate', "
    "observedACPowerMode=int(ac_mode_match.group(1)) if ac_mode_match else None, "
    "observedPowerSource=battery.splitlines()[0],")
exec(compile('\n'.join(preflight_source) + '\n', str(BASE_RUNNER), 'exec'), base.__dict__)
base.harness.preflight = base.preflight


def save(path, value):
    base.common.save(path, value)


def stage(campaign):
    assert campaign in ORDERS
    prepared = base.prepare(campaign)
    stage_dir = HERE / campaign
    plan_rows = []
    for arm, mode in ORDERS[campaign]:
        data, out, env, plan = prepared[arm]
        cfg = (data / 'LuaUI/Config/BC213.lua').read_text()
        assert 'Sepia Tone' not in cfg and not (data / 'LuaUI/Widgets/zzz_v2_functional_probe.lua').exists()
        assert re.search(r'\["Limit idle FPS"\]\s*=\s*0\b', cfg)
        settings = {
            'BAR_NATIVE_MODEL_TYPE_FACTS': str(int(mode == 'ON')),
            'BAR_METAL4_SHARED_CUS_PILOT': '0',
            'BAR_NATIVE_FRAME_TRACE': str(int(campaign == 'diagnostic_power1_paced')),
            'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP': str(int(campaign == 'diagnostic_power1_paced')),
            'MTL_DEBUG_LAYER': str(int(campaign == 'diagnostic_power1_paced')),
            'MTL_SHADER_VALIDATION': '0',
        }
        if campaign == 'diagnostic_power1_paced':
            settings.update(BAR_NATIVE_FRAME_TRACE_START_SIM='300',
                            BAR_NATIVE_FRAME_TRACE_END_SIM='306', MTL_LOG_TO_STDERR='1')
            controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
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
        plan.update(modelTypeFactsMode=mode, sepiaWidget='OFF', cusPrototype='OFF',
                    diagnosticTrace=campaign == 'diagnostic_power1_paced',
                    metalValidation=campaign == 'diagnostic_power1_paced',
                    imageCapture=campaign == 'diagnostic_power1_paced',
                    performanceAccepted=campaign == 'clean_paced')
        save(out / 'LAUNCH.json', plan)
        plan_rows.append(dict(arm=arm, mode=mode, data=str(data), out=str(out),
                              controllerSHA256=plan['stagedControllerSHA256'],
                              childFlags={key: env.get(key) for key in base.CHILD_KEYS}))
    save(stage_dir / 'S-PLAN.json', dict(status='READY_NO_GAMES', campaign=campaign,
        product=base.pinned_product(), order=plan_rows,
        scene='BC213, sim300..1500, AA0, VSync0, 1512x950, same paced warmup',
        diagnostics='both Metal Validation 1; trace every loop sim300..306; image paused sim1500',
        clean='Not offered in this power1 diagnostic runner; clean remains AC powermode0 in the separate runner',
        power='AC powermode1 required by proven P validation preflight on both diagnostics; runtime LimitIdleFps0 and offscreenfalse required',
        allValidRunsRetained=True, priorSamplesPooled=False))
    print('READY ' + campaign + '; no game launched', flush=True)


def trace_summary(info):
    rows = [dict(re.findall(r'(\w+)=([^\s]+)', text)) for text in
            re.findall(r'\[NativeCPUTrace\] (.+)', info)]
    by_kind = {kind: [r for r in rows if r.get('kind') == str(kind)] for kind in range(30, 36)}
    summaries = {str(kind): dict(records=len(values),
        countA=sum(int(v.get('count_a', '0')) for v in values),
        countB=sum(int(v.get('count_b', '0')) for v in values),
        sampledFrames=sorted({int(v['frame']) for v in values if 'frame' in v}),
        statuses=sorted({int(v['status']) for v in values if 'status' in v}))
        for kind, values in by_kind.items()}
    prep = [r for r in rows if r.get('kind') == '4']
    emission = re.findall(r'\[NativeCPUTraceEmission\] (.+)', info)
    buffers = re.findall(r'\[NativeCPUTraceBuffer\] (.+)', info)
    drops = sum(int(value) for line in buffers for value in re.findall(r'\bdropped=(\d+)', line))
    return dict(kinds=summaries, prepareSamples=len(prep), emission=emission,
                buffers=buffers, dropped=drops, allRecords=len(rows),
                newKindsPresent=all(summaries[str(k)]['records'] > 0 for k in range(30, 36)))


def preserve_diagnostic(out):
    """Persist raw functional evidence even when the FPS parser rejects a run."""
    info = (out / 'infolog.txt').read_text() if (out / 'infolog.txt').exists() else ''
    engine_log = (out / 'engine.log').read_text() if (out / 'engine.log').exists() else ''
    trace = trace_summary(info)
    validation = bool(re.search(r'Metal (?:API )?Validation (?:is )?Enabled', engine_log, re.I))
    errors = [line for line in engine_log.splitlines() if re.search(
        r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)',
        line, re.I)]
    save(out / 'MODEL-FACTS-TRACE.json', trace)
    save(out / 'METAL-VALIDATION.json', dict(enabled=validation, errors=errors))
    save(out / 'DIAGNOSTIC-RAW.json', dict(imagePresent=(out / 'scene.png').is_file(),
        image=str(out / 'scene.png'), frameReceipt=bool(re.search(r'\[H1ValidationFrame\] sim=1500 ', info)),
        countedStart=bool(re.search(r'\[MatchedAnchor\] boundary=start .*simulation=300', info)),
        countedEnd=bool(re.search(r'\[MatchedAnchor\] boundary=end .*simulation=1500', info)),
        trace=trace, validationEnabled=validation, metalErrors=errors))
    return info, trace, validation, errors


def diagnostic_trace_valid(trace):
    if not trace['newKindsPresent'] or trace['dropped'] != 0 or not trace['emission']:
        return False
    if not all('outside_window=1' in line for line in trace['emission']):
        return False
    if trace['kinds']['35']['countA'] or trace['kinds']['35']['countB']:
        return False
    return all(trace['kinds'][str(kind)]['sampledFrames'] and
               trace['kinds'][str(kind)]['statuses'] == [0] and
               all(300 <= frame <= 306 for frame in trace['kinds'][str(kind)]['sampledFrames'])
               for kind in range(30, 36))


def run(campaign):
    assert campaign in ORDERS
    if campaign == 'clean_paced':
        gate = base.common.read(HERE / 'DIAGNOSTIC-GATE.json')
        assert gate['status'] == 'PASS' and gate['productSHA256'] == base.pinned_product()['sha256'], gate
    prepared = base.restore(campaign)
    assert base.common.read(HERE / campaign / 'S-PLAN.json')['status'] == 'READY_NO_GAMES'
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
                            if campaign == 'diagnostic_power1_paced' else None)
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
                    if campaign == 'diagnostic_power1_paced':
                        preserve_diagnostic(out)
                if launch_error:
                    save(out / 'POSTRUN-PARSER-OR-LAUNCH-ERROR.json', dict(error=launch_error))
                    cleanup = base.common.read(out / 'CLEANUP.json') if (out / 'CLEANUP.json').exists() else {}
                    if campaign != 'diagnostic_power1_paced' or not cleanup.get('normalExit'):
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
                trace = base.common.read(out / 'MODEL-FACTS-TRACE.json') if campaign == 'diagnostic_power1_paced' else None
                if trace is not None:
                    raw = base.common.read(out / 'DIAGNOSTIC-RAW.json')
                    assert raw['countedStart'] and raw['countedEnd'], 'Native count window absent'
                    assert (out / 'scene.png').is_file(), 'Diagnostic image absent'
                    assert re.search(r'\[H1ValidationFrame\] sim=1500 ', info), 'Image frame receipt absent'
                    assert raw['validationEnabled'], 'Metal Validation enable receipt absent'
                    assert not raw['metalErrors'], 'Metal Validation reported an error or fault'
                    assert diagnostic_trace_valid(trace), 'New model-facts trace counts/window/validity failed'
                else:
                    assert '[NativeCPUTrace]' not in info, 'Trace present in clean arm'
                row.update(backend=mode, modelTypeFactsMode=mode, powerWitness=power[0],
                           pacingWitness=pace, releaseWitness=release, screenWitness=screen,
                           modelFactsTrace=trace, performanceAccepted=campaign == 'clean_paced',
                           visualImage=str(out / 'scene.png') if trace is not None else None)
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
        if campaign == 'diagnostic_power1_paced':
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
