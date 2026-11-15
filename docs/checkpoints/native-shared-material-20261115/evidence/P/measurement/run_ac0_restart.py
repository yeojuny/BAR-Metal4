"""Isolated screen state elision validation/image, then clean same-engine ABBA."""

import argparse
import datetime
import importlib.util
import inspect
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import traceback
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parents[1]
ROOT = EVIDENCE.parent
REF = EVIDENCE / 'worldinfo245-runtime01'
IMAGE = EVIDENCE / 'h1-pool220-runtime01/native_image.py'
PRODUCT = HERE / 'PRODUCT.json'
RELEASE_CONTRACT = HERE / 'RELEASE-CONTRACT.json'
PACING = HERE / 'pacing'
SHARED_AWAKE = EVIDENCE / 'gpu-next219-01/AWAKE.json'
ORDERS = {
    'validation_paced_power0_restart': [('V_POWER0_RESTART_ELISION1_IMAGE', 'ELISION1')],
    'clean_paced': [('A1_ELISION0', 'ELISION0'), ('B1_ELISION1', 'ELISION1'),
                    ('B2_ELISION1', 'ELISION1'), ('A2_ELISION0', 'ELISION0')],
}
PACED = ('validation_paced_power0_restart', 'clean_paced')
VALIDATION = ('validation_paced_power0_restart',)
SCREEN = ('validation_paced_power0_restart',)
CLEAN = ('clean_paced',)
CHILD_KEYS = ('BAR_NATIVE_PREPARE_READY_PILOT', 'BAR_NATIVE_BEGIN_READY_PILOT',
              'BAR_NATIVE_INTERVENTION_RENDER_WORKER', 'BAR_NATIVE_FRAME_OVERLAP',
              'BAR_NATIVE_CAPTURE_OVERLAP', 'BAR_NATIVE_PREPARE_OVERLAP',
              'BAR_NATIVE_CAPTURE_READY_BOUNDARY', 'BAR_NATIVE_BEGIN_PREPARE_OVERLAP',
              'BAR_NATIVE_FINAL_COLOR_STAGE', 'BAR_NATIVE_SCREEN_STATE_ELISION',
              'BAR_NATIVE_SCREEN_TABLE_H5', 'BAR_NATIVE_SCREEN_TABLE_H5_COUNTERS',
              'BAR_NATIVE_FRAME_TRACE',
              'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP', 'MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION')
STRIP_PREFIXES = ('BAR_', 'MTL_', 'METAL_', 'RECOIL_', 'VK_', 'MESA_', 'MVK_',
                  'SPRING_', 'GALLIUM_', 'ZINK_', 'EGL_', 'DYLD_', 'XDG_', 'FONTCONFIG_')


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def app():
    return Path(common.read(PRODUCT)['package'])


frozen = load(REF / 'stage0.py', 'handoff_frozen')
common = frozen.load(frozen.PRIOR / 'compare_three_continue.py', 'handoff_common')
base = common.load(frozen.EVIDENCE / 'display-settings219/performance-218-219-01/compare.py', 'handoff_original')
harness = base.harness
harness.parse = frozen.parser(common, sys.modules['aligned'], clean=True)
exec(compile(inspect.getsource(common.backend_evidence).replace('[480,2250]', '[480,1500]'),
             str(__file__), 'exec'), common.__dict__)
life = common.load(REF / 'lifecycle.py', 'handoff_lifecycle')
life.install(harness)
reference_runner = load(REF / 'run.py', 'handoff_witness')
policy = frozen.load(frozen.STEADY / 'fps_power_policy.py', 'handoff_policy')
image = {'__file__': str(IMAGE), '__name__': 'handoff_image'}
exec(compile(IMAGE.read_text().replace('2250', '1500'), str(IMAGE), 'exec'), image)
active = {'prepared': None, 'mapped': False, 'campaign': None}


def pinned_product():
    pin = common.read(PRODUCT)
    package = app()
    binary = Path(pin['binary'])
    native = Path(pin['native'])
    assert pin['package'] == str(package)
    assert binary == native / 'bin/spring-metal4-scene'
    assert binary != package / 'Contents/Resources/BAR.app/Contents/Resources/native-engines/development/bin/spring-metal4-scene'
    assert re.fullmatch(r'[0-9a-f]{64}', pin['sha256'])
    assert common.sha(binary) == pin['sha256'], 'Candidate changed after pin'
    subprocess.run(['/usr/bin/codesign', '--verify', '--strict', str(binary)], check=True)
    subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(package)], check=True)
    return dict(variant='screen-state-elision-standalone-test', binary=str(binary),
                native=str(native), package=str(package), sha256=pin['sha256'])


def wrapper_environment():
    wrapper = app() / 'Contents/MacOS/BAR-Metal4'
    text = wrapper.read_text()
    values = {key: value.strip().strip("'\"") for key, value in re.findall(
        r'^export ((?:BAR_NATIVE_|BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER|MTL_)[A-Z0-9_]*)=(.*)$',
        text, re.M)}
    assert values['BAR_NATIVE_INTERVENTION_RENDER_WORKER'] == '1'
    assert values['BAR_NATIVE_FRAME_OVERLAP'] == '1'
    assert values['BAR_NATIVE_CAPTURE_OVERLAP'] == '1'
    assert values['BAR_NATIVE_PREPARE_OVERLAP'] == '1'
    assert not any(k.startswith('BAR_NATIVE_INTERVENTION_') and
                   k != 'BAR_NATIVE_INTERVENTION_RENDER_WORKER' for k in values)
    assert values.get('BAR_NATIVE_CAPTURE_READY_BOUNDARY') in (None, '1'), 'Fallback mode disabled in candidate wrapper'
    assert values['BAR_NATIVE_FINAL_COLOR_STAGE'] == '1'
    assert 'BAR_NATIVE_SCREEN_STATE_ELISION' not in values
    assert 'BAR_NATIVE_SCREEN_TABLE_H5' not in values
    assert 'BAR_NATIVE_SCREEN_TABLE_H5_COUNTERS' not in values
    values['BAR_NATIVE_BEGIN_READY_PILOT'] = '0'
    values['BAR_NATIVE_PREPARE_READY_PILOT'] = '0'
    values['BAR_NATIVE_BEGIN_PREPARE_OVERLAP'] = '1'
    return wrapper, values


def prepare(campaign):
    stage = HERE / campaign / 'preparation'
    assert not stage.exists(), 'Campaign already staged; preserve prior evidence'
    product = pinned_product()
    wrapper, shipped = wrapper_environment()
    release = common.read(RELEASE_CONTRACT)
    assert release['status'] == 'FINAL_SOURCE_WITNESS'
    assert release['candidateSHA256'] == product['sha256']
    pacing = common.read(PACING / 'PACING.json') if campaign in PACED else None
    if pacing:
        for name in ('warmup_pacing.lua', 'zzz_native_warmup_pacing.lua'):
            assert common.sha(PACING / name) == pacing['hashes'][name]
        assert pacing['sharedControllerEdits'] == 0
        assert pacing['pauseRequestSimulation'] == 180 and pacing['ackDeadlineSimulation'] == 240
        assert pacing['resumeMinimumWallSeconds'] == 10
    stage.mkdir(parents=True)
    order = ORDERS[campaign]
    design = dict(campaign=campaign, order=order, product=product,
                  wrapperSHA256=common.sha(wrapper), wrapperEnvironment=shipped,
                  method='Frozen Direct50, paced >=9.8s wall warmup, sim300..1500 / 40 counted',
                  render=[1512, 950], preset='BC213 AA0 VSync0 VSyncGame0',
                  releasedWrapperReference='published v1.11.3; directly invoke signed standalone candidate engine',
                  captureReadyBoundaryDefault='enabled; explicit BAR_NATIVE_CAPTURE_READY_BOUNDARY=0 is fallback',
                  releaseWitnessContract=release,
                  previousSamplesPooled=False, allValidRunsRetained=True,
                  performanceAccepted=campaign in CLEAN,
                  metalValidation=campaign in VALIDATION,
                  frameTraceSampling='OFF every arm',
                  imageCapture=campaign in SCREEN,
                  pacingContract=pacing,
                  originalControllerSHA256=common.sha(REF / 'control.lua'),
                  priorPerformanceContextOnly='separate historical evidence; never pooled')
    common.save(stage.parent / 'DESIGN.json', design)
    prepared = {}
    for arm, mode in order:
        data, out, env, plan = harness.stage_arm(stage, arm, product, frozen.ASSETS)
        for amendment in policy.apply(data, 'BC213'):
            plan['checksums'][amendment['path']] = amendment['afterSHA256']
        home = data.parents[1] / 'home'
        home.mkdir()
        env['HOME'] = plan['env']['HOME'] = str(home)
        for source, target in (
            (REF / 'control.lua', data / 'LuaUI/Widgets/zzz_native_short_run.lua'),
            (REF / 'bar-commander-control-213c1.sdz', data / 'games/bar-commander-control-213c1.sdz'),
            (REF / 'backend-status.lua', data / 'LuaUI/Widgets/zzz_backend_common_status.lua'),
        ):
            shutil.copyfile(source, target)
            plan['checksums'][str(target)] = common.sha(target)
        if pacing:
            for name, rel in (
                ('warmup_pacing.lua', 'LuaUI/Widgets/Include/native_warmup_pacing.lua'),
                ('zzz_native_warmup_pacing.lua', 'LuaUI/Widgets/zzz_native_warmup_pacing.lua'),
            ):
                target = data / rel
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(PACING / name, target)
                assert common.sha(target) == pacing['hashes'][name]
                plan['checksums'][str(target)] = common.sha(target)
            widget_cfg = data / 'LuaUI/Config/BC213.lua'
            original_cfg = widget_cfg.read_text()
            assert 'order = { ["Limit idle FPS"] = 0 }' in original_cfg
            widget_cfg.write_text(original_cfg.replace(
                'order = { ["Limit idle FPS"] = 0 }',
                'order = { ["Limit idle FPS"] = 0, ["Native Precount Warmup Pacing"] = 1 }', 1))
            plan['checksums'][str(widget_cfg)] = common.sha(widget_cfg)
            plan['pacing'] = dict(contract=pacing, originalWidgetConfigSHA256=policy.digest(original_cfg),
                                  effectiveWidgetConfigSHA256=common.sha(widget_cfg),
                                  widgetEnabledByOrder=True)
        for key in list(env):
            if key.startswith('BAR_NATIVE_'):
                env.pop(key, None)
                plan['env'].pop(key, None)
        env.update(shipped)
        plan['env'].update(shipped)
        assert env.get('BAR_NATIVE_CAPTURE_READY_BOUNDARY') in (None, '1')
        env['BAR_NATIVE_BEGIN_READY_PILOT'] = plan['env']['BAR_NATIVE_BEGIN_READY_PILOT'] = '0'
        env['BAR_NATIVE_PREPARE_READY_PILOT'] = plan['env']['BAR_NATIVE_PREPARE_READY_PILOT'] = '1'
        env['BAR_NATIVE_BEGIN_PREPARE_OVERLAP'] = plan['env']['BAR_NATIVE_BEGIN_PREPARE_OVERLAP'] = '1'
        for key, value in (
            ('BAR_NATIVE_FINAL_COLOR_STAGE', '1'),
            ('BAR_NATIVE_SCREEN_STATE_ELISION', str(int(mode == 'ELISION1'))),
            ('BAR_NATIVE_SCREEN_TABLE_H5', '0'),
            ('BAR_NATIVE_SCREEN_TABLE_H5_COUNTERS', str(int(campaign in VALIDATION))),
            ('BAR_NATIVE_FRAME_TRACE', '0'),
            ('BAR_NATIVE_FRAME_TRACE_EVERY_LOOP', '0'),
            ('MTL_DEBUG_LAYER', str(int(campaign in VALIDATION))),
            ('MTL_SHADER_VALIDATION', '0'),
        ):
            env[key] = plan['env'][key] = value
        if campaign in VALIDATION + SCREEN:
            controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
            assert "assert(warm and Spring.DiffTimers(now,warm)>=9.8,'[MatchedGameplay] FAILED_WARMUP_BEFORE_SIM300')" in controller.read_text()
            if campaign in SCREEN:
                image['configure'](controller)
                plan['checksums'][str(controller)] = common.sha(controller)
                assert all(env.get(key) in (None, '', '0') for key in (
                    'BAR_NATIVE_FRAME_TRACE', 'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP',
                    'MTL_SHADER_VALIDATION'))
            if campaign in VALIDATION:
                assert env['MTL_DEBUG_LAYER'] == '1' and env['BAR_NATIVE_FRAME_TRACE'] == '0'
                env['MTL_LOG_TO_STDERR'] = plan['env']['MTL_LOG_TO_STDERR'] = '1'
        else:
            assert all(v in ('0', '', None) for k, v in env.items()
                       if k.startswith(('BAR_NATIVE_', 'MTL_', 'METAL_')) and
                       any(t in k for t in ('_COUNTERS', '_TRACE', '_FILE', 'DEBUG_LAYER',
                                           'CAPTURE_ENABLED', 'SHADER_VALIDATION')))
        plan['originalControllerSourceSHA256'] = common.sha(REF / 'control.lua')
        plan['stagedControllerSHA256'] = common.sha(data / 'LuaUI/Widgets/zzz_native_short_run.lua')
        plan['measurementWindow'] = dict(startSimulation=300, endSimulation=1500,
                                          warmupSeconds=10, countedSeconds=40,
                                          performanceAccepted=campaign in CLEAN)
        plan['screenStateElision'] = dict(requested=str(int(mode == 'ELISION1')),
                                          finalColorStage='1', h5Mode='0',
                                          h5Counters=str(int(campaign in VALIDATION)))
        common.save(out / 'LAUNCH.json', plan)
        prepared[arm] = (data, out, env, plan)
    common.save(stage.parent / 'PREPARED.json', dict(status='READY', utc=utc(),
        productSHA256=product['sha256'], arms=[dict(arm=arm, mode=mode,
        data=str(prepared[arm][0]), out=str(prepared[arm][1])) for arm, mode in order]))
    return prepared


def restore(campaign):
    stage = HERE / campaign / 'preparation'
    receipt = common.read(stage.parent / 'PREPARED.json')
    assert receipt['status'] == 'READY'
    product = pinned_product()
    assert product['sha256'] == receipt['productSHA256']
    assert common.sha(app() / 'Contents/MacOS/BAR-Metal4') == common.read(
        stage.parent / 'DESIGN.json')['wrapperSHA256']
    prepared = {}
    for entry in receipt['arms']:
        out = Path(entry['out'])
        plan = common.read(out / 'LAUNCH.json')
        env = {k: v for k, v in os.environ.items() if not k.startswith(STRIP_PREFIXES)}
        env.update(plan['env'])
        prepared[entry['arm']] = (Path(entry['data']), out, env, plan)
    return prepared


def preflight(remaining):
    awake = common.read(SHARED_AWAKE)
    assert awake['status'] == 'ACTIVE' and not awake['securitySettingsChanged']
    os.kill(awake['pid'], 0)
    assert '/usr/bin/caffeinate' in subprocess.check_output(
        ['/bin/ps', '-p', str(awake['pid']), '-o', 'args='], text=True)
    age = (datetime.datetime.now(datetime.timezone.utc) -
           datetime.datetime.fromisoformat(awake['startUTC'])).total_seconds()
    assert age + 330*remaining < awake['maximumSeconds']
    display = json.loads(subprocess.check_output(
        ['/usr/bin/python3', '-B', str(ROOT/'watch_display110.py'), '--once'], text=True))
    battery = subprocess.check_output(['/usr/bin/pmset', '-g', 'batt'], text=True)
    settings = subprocess.check_output(['/usr/bin/pmset', '-g', 'custom'], text=True).partition('AC Power:')[2]
    assert "'AC Power'" in battery.splitlines()[0] and re.search(r'^\s*powermode\s+0\s*$', settings, re.M)
    thermal = subprocess.check_output(['/usr/bin/pmset', '-g', 'therm'], text=True)
    assert not re.search(r'(?:thermal|performance) warning level\s*[:=]\s*[1-9]|(?:Thermal_Level|Performance_Level)\s*=\s*[1-9]', thermal, re.I)
    result = dict(display=display, battery=battery, thermal=thermal,
                  awakeOwner=str(SHARED_AWAKE),
                  offscreenFrameReduction='staged LimitIdleFps0, BC213 widget order0; runtime receipt required',
                  lockPolicy='User authorized locked/offscreen observation; visible FPS not inferred')
    if active['prepared']:
        _, out, _, _ = active['prepared']
        observed = dict(utc=utc(), remaining=remaining, result=result,
                        limit='Exact display and power state; locked/asleep accepted by user, rendering checked separately')
        first = out / 'PREFLIGHT-OBSERVED-FIRST.json'
        if not first.exists():
            common.save(first, observed)
        common.save(out / 'PREFLIGHT-OBSERVED-LAST.json', observed)
        if active['campaign'] in PACED:
            engine_log = out / 'engine.log'
            if engine_log.exists():
                with engine_log.open('rb') as stream:
                    stream.seek(max(0, engine_log.stat().st_size - 65536))
                    recent = stream.read()
                if b'FAILED_WARMUP_BEFORE_SIM300' in recent:
                    raise RuntimeError('Observed original FAILED_WARMUP_BEFORE_SIM300; stop owned child promptly')
                if active['campaign'] in PACED and b'[WarmupPacing] event=FAILED_' in recent:
                    raise RuntimeError('Observed pacing failure; stop owned child promptly')
    if active['prepared'] and not active['mapped']:
        _, out, env, _ = active['prepared']
        if (out / 'OWNED.json').exists():
            log = (out / 'engine.log').read_text()
            assert '[MatchedAnchor] boundary=start' not in log, 'Child proof missed warmup'
            pid = common.read(out / 'OWNED.json')['pid']
            raw = subprocess.check_output(['/bin/ps', 'eww', '-p', str(pid), '-o', 'command='],
                                          text=True, timeout=3)
            actual = {key: (match.group(1) if (match := re.search(
                r'(?:^|\s)' + key + r'=([^\s]+)', raw)) else None) for key in CHILD_KEYS}
            expected = {key: env.get(key) for key in CHILD_KEYS}
            assert actual == expected, (actual, expected)
            common.save(out / 'CHILD-MODE.json', dict(status='PASS_ACTUAL_CHILD_ENVIRONMENT',
                pid=pid, actual=actual, expected=expected, beforeCountedWindow=True))
            active['mapped'] = True
    return result


harness.preflight = preflight


def pacing_evidence(out, plan, info):
    contract = plan['pacing']['contract']
    entries = []
    for line in info.splitlines():
        match = re.search(r'\[WarmupPacing\] event=(\w+)(.*)', line)
        if match:
            fields = dict(re.findall(r'(\w+)=([^\s]+)', match.group(2)))
            entries.append(dict(event=match.group(1), fields=fields, raw=line))
    expected = ('start', 'pause_request', 'pause_ack', 'resume_request',
                'resume_ack', 'removed_before_count')
    assert tuple(e['event'] for e in entries) == expected, entries
    def field(event, key, cast=float):
        return cast(next(e['fields'][key] for e in entries if e['event'] == event))
    assert field('start', 'simulation', int) == 1
    assert field('pause_request', 'simulation', int) == contract['pauseRequestSimulation']
    ack_frame = field('pause_ack', 'simulation', int)
    assert contract['pauseRequestSimulation'] <= ack_frame < contract['ackDeadlineSimulation']
    assert field('resume_request', 'simulation', int) == ack_frame
    assert field('resume_ack', 'simulation', int) == ack_frame
    assert field('removed_before_count', 'simulation', int) < contract['ackDeadlineSimulation']
    assert field('resume_request', 'wall_seconds') >= contract['resumeMinimumWallSeconds']
    held_seconds = field('resume_request', 'held_seconds')
    held_draws = field('resume_request', 'held_draws', int)
    assert held_seconds <= 0.5 or held_draws >= 2, 'No native draw progression during a sustained pause'
    assert field('removed_before_count', 'wall_seconds') >= contract['resumeMinimumWallSeconds']
    anchors = re.findall(r'\[MatchedAnchor\] boundary=start .*?warmup_seconds=([0-9.]+)', info)
    assert len(anchors) == 1 and float(anchors[0]) >= contract['originalWallGate'], anchors
    result = dict(status='PASS', events=entries, originalWallGateSeconds=float(anchors[0]),
                  moduleSHA256=contract['hashes']['warmup_pacing.lua'],
                  widgetSHA256=contract['hashes']['zzz_native_warmup_pacing.lua'])
    common.save(out / 'PACING-WITNESS.json', result)
    return result


def release_evidence(out, plan, info, mode):
    contract = common.read(RELEASE_CONTRACT)
    assert contract['candidateSHA256'] == plan['product']['sha256']
    releases = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in
                re.findall(r'\[NativeReleaseMode\] (.+)', info)]
    boundaries = re.findall(r'\[NativeCaptureReadyBoundary\] ([^\n]+)', info)
    pending = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in
               re.findall(r'\[NativeBeginPrepareOverlap\] (.+)', info)]
    reclaim = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in
               re.findall(r'\[NativeBeginCPUReclaim\] (.+)', info)]
    assert mode in ('OLD_BEGIN', 'FIXED_BEGIN', 'PREPARE')
    actual_mode = 'PREPARE' if mode == 'PREPARE' else 'BEGIN'
    assert len(releases) == 1 and releases[0]['mode'] == actual_mode, releases
    release = releases[0]
    assert release['eligible'] == release['queued'] == '1'
    assert release['begin_prepare_overlap'] == str(int(mode == 'FIXED_BEGIN')), release
    simulation = int(release['simulation'])
    assert 300 <= simulation <= 1500, release
    begin_submission = int(release['begin_submission'])
    if actual_mode == 'BEGIN':
        assert begin_submission > 0, ('BEGIN release must follow actual Begin', release)
    expected = contract['boundaryMarkers'][actual_mode]
    assert len(boundaries) == 1 and boundaries[0] == expected, boundaries
    if mode == 'FIXED_BEGIN':
        assert len(pending) == 1 and pending[0]['pending_at_prepare'] == '1', pending
        assert 300 <= int(pending[0]['simulation']) <= 1500
        assert int(pending[0]['source_draw']) > 0
        assert len(reclaim) <= 1
        if reclaim:
            assert all(reclaim[0][k] == v for k, v in (
                ('cpu_joined', '1'), ('capacity_recovered', '1'),
                ('full_gpu_drain', '0'), ('source_credit_limit', '2')))
            assert 300 <= int(reclaim[0]['simulation']) <= 1500
    else:
        assert not pending and not reclaim, (pending, reclaim)
    result = dict(mode=mode, actualReleaseMode=actual_mode, queuedReleaseCount=1, release=release,
                  boundaryCount=1, boundary=boundaries[0],
                  pendingAtPrepareWitness=pending[0] if pending else None,
                  cpuCreditReclaimWitness=reclaim[0] if reclaim else None,
                  cpuCreditReclaimStatus='OBSERVED' if reclaim else 'NOT_OBSERVED',
                  effectiveModeFromActualRelease=True)
    common.save(out / 'RELEASE-WITNESS.json', result)
    return result


def screen_state_evidence(out, info, enabled, validation):
    color = re.findall(r'\[NativeFinalColorStage\] active=(\d+) samples=(\d+) present_order_original=(\d+) width=(\d+) height=(\d+)', info)
    assert color == [('1', '1', '1', '1512', '982')], ('Final color stage must remain active', color)
    records = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in
               re.findall(r'\[NativeScreenStateElision\] (.+)', info)]
    h5 = [dict(re.findall(r'(\w+)=([^\s]+)', line)) for line in
          re.findall(r'\[NativeScreenTableH5V2\] stage=encode (.+)', info)]
    if validation:
        assert records and h5, 'Bounded screen state and H5 receipts absent'
        assert all(r.get('enabled') == str(int(enabled)) and r.get('fresh_range') == '1'
                   and r.get('argument_binds_unchanged') == '1' for r in records)
        assert all(r.get('mode') == '0' and r.get('fresh_range_baseline') == '1' for r in h5)
        omitted_keys = ('omitted_cull', 'omitted_depth', 'omitted_stencil', 'omitted_viewport',
                        'omitted_scissor', 'omitted_depthclip', 'omitted_pipeline')
        omitted = {key: sum(int(r[key]) for r in records) for key in omitted_keys}
        assert sum(omitted.values()) > 0, 'Enabled candidate emitted no omitted-setter evidence'
        assert len(records) == len(h5), 'Screen state/H5 bounded receipt counts differ'
    else:
        assert not records and not h5, 'Timing run unexpectedly enabled screen counters'
        omitted = None
    result = dict(requestedElision=int(enabled), finalColorStageActive=True,
                  h5Mode=0, countersEnabled=validation, boundedRecords=len(records),
                  omittedSetters=omitted, scope='bounded value-only receipts; validation run excluded from FPS')
    common.save(out / 'SCREEN-STATE-WITNESS.json', result)
    return result


def run(campaign):
    stage = HERE / campaign / 'preparation'
    prepared = restore(campaign)
    order = ORDERS[campaign]
    rows = []
    for index, (arm, mode) in enumerate(order):
        data, out, env, plan = prepared[arm]
        active.update(prepared=prepared[arm], mapped=False, campaign=campaign)
        stop = threading.Event()
        issues = []
        menu_worker = threading.Thread(target=reference_runner.witness_menu,
                                       args=(out, stop, issues))
        menu_worker.start()
        image_worker = None
        if campaign in SCREEN:
            image_worker = threading.Thread(target=image['capture'],
                                            args=(data, out, stop, issues))
            image_worker.start()
        try:
            print('START ' + arm, flush=True)
            row = harness.launch(stage, arm, prepared[arm], len(order)-index)
            stop.set()
            menu_worker.join()
            if image_worker:
                image_worker.join()
            pid = common.read(out / 'OWNED.json')['pid']
            window = common.read(out / 'WINDOW.json')
            witness_path = out / 'MENU-WITNESS.jsonl'
            samples = [json.loads(line) for line in witness_path.read_text().splitlines()] if witness_path.exists() else []
            samples = [s for s in samples if window['start'] <= s['epoch']*1e9 <= window['end']]
            menu_observation = dict(samples=len(samples), foregroundPIDMatches=sum(
                s.get('frontPID') == pid for s in samples), gameCoversDisplay=sum(
                bool(s.get('gameCoversDisplay')) for s in samples), menuAboveGame=sum(
                bool(s.get('menuAboveGame')) for s in samples), errors=issues,
                acceptance='observed only; foreground and menu occlusion not required under locked/offscreen authorization')
            common.save(out / 'MENU-OBSERVATION.json', menu_observation)
            assert active['mapped'], 'Actual child mode proof absent'
            backend = common.backend_evidence(out, row, 'PREPARE')
            info = (out / 'infolog.txt').read_text()
            pace = pacing_evidence(out, plan, info) if campaign in PACED else None
            power = re.findall(r'\[FPSPowerStatus\] (.+)', info)
            if len(power) != 1 or not all(value in power[0] for value in (
                'frame=480', 'shortName=BC213', 'LimitIdleFps=0', 'VSync=0',
                'VSyncGame=0', 'idleInterface=absent', 'option_limitoffscreenfps=false')):
                raise AssertionError(('Offscreen FPS limiter not verified OFF at runtime', power))
            assert row['draws'] > 0 and re.search(
                r'\[NativeMRT164\].*encoded=1', info) and re.search(
                r'\[NativePass166\] enabled=0 source_frame=300 submission=\d+ .*retired=1', info) and re.search(
                r'\[NativePass165\] enabled=0 source_frame=300 submission=\d+ .*retired=1', info), 'Native scene encode/submit/retire evidence missing'
            release = release_evidence(out, plan, info, 'PREPARE')
            screen_state = screen_state_evidence(out, info, mode == 'ELISION1', campaign in VALIDATION)
            row.update(backend=mode, backendEvidence=backend,
                       powerWitness=power[0], menuObservation=menu_observation,
                       displayObservation=common.read(out/'PREFLIGHT-OBSERVED-FIRST.json')['result']['display'],
                       nativeSceneEvidence='MRT164 encoded; Pass166/165 source frame300 submission and retirement',
                       nativeReleaseWitness=release, screenStateWitness=screen_state,
                       performanceAccepted=campaign in CLEAN,
                       pacingWitness=pace)
            if campaign in VALIDATION:
                engine_log = (out / 'engine.log').read_text()
                assert re.search(r'Metal (?:API )?Validation (?:is )?Enabled', engine_log, re.I)
                row['metalValidationEnabled'] = True
                row['metalValidationErrors'] = [line for line in engine_log.splitlines() if
                    re.search(r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)', line, re.I)]
                assert '[NativeCPUTrace]' not in engine_log, 'Frame trace must remain off'
            if campaign in SCREEN:
                info = (out / 'infolog.txt').read_text()
                captures = re.findall(r'\[H1ValidationFrame\] (sim=1500 .+)', info)
                assert len(captures) == 1 and (out / 'scene.png').is_file()
                row['capture'] = captures[0]
            common.save(out / 'RESULT.json', row)
            rows.append(row)
            common.save(stage.parent / 'RUNS.json', rows)
            print(f"DONE {arm}: {row['FPS']:.3f} FPS, 1% low {row['frameStats']['slow1PercentFPS']:.3f}", flush=True)
        except BaseException:
            stop.set()
            menu_worker.join()
            if image_worker:
                image_worker.join()
            failure = dict(arm=arm, mode=mode, error=traceback.format_exc(), utc=utc())
            common.save(out / 'FAILURE.json', failure)
            common.save(stage.parent / 'COMPLETION.json', dict(status='STOPPED_ON_FAILURE',
                completed=len(rows), failure=failure, utc=utc()))
            raise
        finally:
            (data / 'LuaUI/Widgets/zzz_backend_common_status.lua').unlink(missing_ok=True)
    if campaign in VALIDATION:
        errors = {r['run']: r['metalValidationErrors'] for r in rows if r['metalValidationErrors']}
        status = 'ERRORS_REVIEW_REQUIRED' if errors else 'NO_DETECTED_VALIDATION_ERRORS'
        completion = dict(status=status, runs=len(rows), errors=errors,
                          image=str(stage/'runs'/rows[0]['run']/'scene.png'),
                          performanceAccepted=False,
                          comparisonLimits='Compare fixed static regions against prior published 1.11.3 image; dynamic water, cloud and text may differ',
                          utc=utc())
    else:
        pooled = {mode: common.pooled(rows, mode, stage) for mode in ('ELISION0', 'ELISION1')}
        completion = dict(status='COMPLETE', runs=len(rows), pooled=pooled,
                          elision1Vs0Percent=100*(pooled['ELISION1']['FPS']/pooled['ELISION0']['FPS']-1),
                          previousSamplesPooled=False, validSlowRunsRetained=True, utc=utc())
    common.save(stage.parent / 'COMPLETION.json', completion)
    print(json.dumps(completion, indent=2), flush=True)


def cleanup(campaign, prepared):
    for data, out, _, plan in prepared.values():
        for rel in ('LuaUI/Widgets/zzz_native_short_run.lua',
                    'LuaUI/Widgets/zzz_backend_common_status.lua',
                    'games/bar-commander-control-213c1.sdz',
                    'LuaUI/Widgets/Include/native_warmup_pacing.lua',
                    'LuaUI/Widgets/zzz_native_warmup_pacing.lua'):
            target = data / rel
            if target.exists():
                assert common.sha(target) == plan['checksums'][str(target)]
                target.unlink()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign', choices=tuple(ORDERS), required=True)
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare-only', action='store_true')
    action.add_argument('--window-open', action='store_true')
    args = parser.parse_args()
    if args.prepare_only:
        prepare(args.campaign)
        print('READY; no games launched', flush=True)
        return
    prepared = restore(args.campaign)
    assert common.read(SHARED_AWAKE)['status'] == 'RELEASED'
    (HERE / 'AWAKE-BEFORE.json').write_bytes(SHARED_AWAKE.read_bytes())
    caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '2400', '-w', str(os.getpid())])
    receipt = dict(status='ACTIVE', campaign=args.campaign, pid=caffeine.pid,
                   startUTC=utc(), maximumSeconds=2400, securitySettingsChanged=False)
    common.save(SHARED_AWAKE, receipt)
    common.save(HERE / 'AWAKE.json', receipt)
    try:
        run(args.campaign)
    finally:
        try:
            cleanup(args.campaign, prepared)
        finally:
            if caffeine.poll() is None:
                caffeine.terminate()
                caffeine.wait(timeout=5)
            receipt.update(status='RELEASED', endUTC=utc(), ownCaffeinateAlive=False)
            common.save(SHARED_AWAKE, receipt)
            common.save(HERE / 'AWAKE.json', receipt)
            common.save(HERE / 'AWAKE-RELEASED.json', receipt)


if __name__ == '__main__':
    main()
