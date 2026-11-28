'Frozen R03 versus GroundDecal pages Ultra AA0 validation; explicit stage/run only.'

import argparse
import datetime
import hashlib
import importlib.util
import inspect
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
RUN_ROOT = HERE
E = HERE.parent
EVIDENCE = E.parent
P = EVIDENCE / 'material-storage-pr7-20261115-01'
U = P / 'ultra-compare'
C = P.parents[1]
BATTLE = P / 'measurement/battle'
PACING = EVIDENCE / 'shared-program-v2-20261114-01/measurement/pacing'
if not PACING.is_dir():
    PACING = EVIDENCE / 'shared-program-v2-20261115-01/measurement/pacing'
BASE_MODULE = BATTLE / 'run.py'
COMPARE_OLD = EVIDENCE / 'pure220-vs-mvk01/compare_three.py'
DESIGN_OLD = EVIDENCE / 'pure220-vs-mvk01/DESIGN-THREE.json'
MAP_SOURCE = Path('/Users/yeojun/Library/Application Support/BAR-Run91/assets/maps/seths_ravine_remake_1.3.1.sd7')
FONT_SOURCE = P / 'runtime/native/data/fonts/FreeSansBold.otf'
FONT_SHA256 = '8b1fb134c1ae1c4c8da1952e3c2535f7242899da21d556e9fd1d6fdf31af3242'
ORDER = {
    'validation_paced': [('V_R03_BASELINE', 'baseline'), ('V_GDP_CANDIDATE', 'candidate')],
}


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


battle = load(BASE_MODULE, 'pr7_battle_ultra_source')
base = battle.base
old_runner = load(COMPARE_OLD, 'preserved_mvk3_ultra_env')
common = base.common
sha = battle.sha
save = battle.save
base.PACED = tuple(ORDER)
base.CLEAN = ('clean_paced',)
base.CHILD_KEYS += ('BAR_NATIVE_MATERIAL_STORAGE_POOL', 'BAR_NATIVE_MODEL_TYPE_FACTS',
                    'BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP', 'BAR_NATIVE_SHARED_PAIR_KEY_VIEW',
                    'BAR_METAL4_SHARED_CUS_PILOT', 'BAR_METAL4_SEPIA_PATH',
                    'BAR_NATIVE_GRASS_BOUNDARY_S01', 'BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE',
                    'BAR_NATIVE_GROUND_DECAL_INDEX_G01', 'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE')

# Keep the official battle parser and its original 1512x950 viewport check.
parser_source = inspect.getsource(battle.battle_parse)
assert parser_source.count("'view=1512,950,0,0'") == 1
old_script_gate = "assert sha(data / 'script.txt') == sha(HERE / 'start.txt')"
assert parser_source.count(old_script_gate) == 1
parser_source = parser_source.replace(old_script_gate,
    "assert (data / 'script.txt').read_text() == (HERE / 'start.txt').read_text().replace('spectator=0;', 'spectator=1;', 1)", 1)
exec(compile(parser_source, str(BASE_MODULE), 'exec'), battle.__dict__)
base.harness.parse = battle.battle_parse
original_preflight = base.preflight
expected_libraries = common.read(EVIDENCE / 'pure220-vs-mvk01/runtime-three/preparation/runs/M1_MVK3/BACKEND-LOADED.json')['expectedRuntimeLibraries']
loaded_once = set()


def preflight_with_mvk3_stack(remaining):
    result = original_preflight(remaining)
    prepared = base.active.get('prepared')
    if not prepared:
        return result
    _, out, _, plan = prepared
    if plan['ultra']['mode'] != 'MVK3' or str(out) in loaded_once or not (out / 'OWNED.json').exists():
        return result
    log = out / 'engine.log'
    if not log.exists():
        return result
    text = log.read_text()
    if not re.search(r'GL renderer\s*:\s*zink Vulkan .*MOLTENVK', text):
        return result
    assert '[MatchedAnchor] boundary=start' not in text, 'Loaded-library witness missed pre-count window'
    pid = common.read(out / 'OWNED.json')['pid']
    mapped_raw = subprocess.run(['/usr/sbin/lsof', '-a', '-p', str(pid), '-d', 'txt', '-Fn'],
                                capture_output=True, text=True, timeout=5, check=True).stdout
    mapped = [line[1:] for line in mapped_raw.splitlines() if line.startswith('n')]
    assert all(path in mapped for path in expected_libraries), 'Actual MVK3 library mapping changed'
    assert plan['product']['binary'] in mapped, 'Actual MVK3 binary mapping changed'
    save(out / 'BACKEND-LOADED.json', dict(status='PASS_PRECOUNT_MVK3_STACK', pid=pid,
         mappedRuntimeLibraries=[path for path in mapped if path in expected_libraries],
         exactLibrariesSHA256=expected_libraries, binary=plan['product']['binary'],
         duringCountedWindow=False, queries=1))
    loaded_once.add(str(out))
    return result


base.harness.preflight = preflight_with_mvk3_stack


def products():
    baseline = common.read(HERE / 'BASELINE-PRODUCT.json')
    candidate = common.read(HERE / 'PRODUCT.json')
    assert sha(baseline['binary']) == baseline['sha256']
    assert sha(candidate['binary']) == candidate['sha256']
    assert baseline['sha256'] == '107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9'
    assert candidate['sha256'] == '035d62084b6657d663a08e6422cdd696af0fafedc01a5189ba696619b15f20c4'
    return {'baseline': baseline, 'candidate': candidate}


def configure_settings(data, aa_samples):
    path = data / 'springsettings.cfg'
    text = path.read_text()
    values = dict(MSAA=1 if aa_samples == 4 else 0,
                  MSAALevel=4 if aa_samples == 4 else -1,
                  MinSampleShadingRate=0, VSync=0, VSyncGame=0,
                  Fullscreen=1, WindowBorderless=0, XResolutionWindowed=1512,
                  YResolutionWindowed=950, WindowPosX=0, WindowPosY=0,
                  NativeFullscreenRenderWidth=0, NativeFullscreenRenderHeight=0,
                  LimitIdleFps=0)
    for key, value in values.items():
        pattern = r'^' + re.escape(key) + r'\s*=.*$'
        line = f'{key} = {value}'
        text = re.sub(pattern, line, text, flags=re.M) if re.search(pattern, text, re.M) else text + '\n' + line
    path.write_text(text)
    return values


def configure_controller(controller):
    text = controller.read_text()
    spectator = " if f==30 then Spring.SendCommands('cheat 1') end"
    assert text.count(spectator) == 1
    text = text.replace(spectator,
        " if f==30 then Spring.SendCommands('cheat 1');Spring.SendCommands('specfullview 1') end", 1)
    opening = " if f==240 then\n  combatWarm=Spring.GetTimer()"
    assert text.count(opening) == 1
    text = text.replace(opening, ''' if f==240 then
  local vw,vh=Spring.GetViewGeometry()
  assert(vw==1512 and vh==950,'[UltraQuality] FAILED_VIEW_AT_COMBAT')
  local spec,fullView,fullSelect=Spring.GetSpectatingState()
  assert(spec and fullView,'[BattleVisibility] FAILED_SPECTATOR_FULLVIEW')
  local visible=(Spring.GetVisibleUnits and Spring.GetVisibleUnits()) or {}
  Spring.Echo(string.format('[BattleVisibility] frame=240 spectating=%s fullview=%s fullselect=%s visible_units=%d',tostring(spec),tostring(fullView),tostring(fullSelect),#visible))
  combatWarm=Spring.GetTimer()''', 1)
    opening = ' if f==540 then beginMeasurement(f) end'
    assert text.count(opening) == 1
    text = text.replace(opening, ''' if f==540 then
  local spec,fullView,fullSelect=Spring.GetSpectatingState()
  local visible=(Spring.GetVisibleUnits and Spring.GetVisibleUnits()) or {}
  Spring.Echo(string.format('[BattleVisibility] frame=540 spectating=%s fullview=%s fullselect=%s visible_units=%d',tostring(spec),tostring(fullView),tostring(fullSelect),#visible))
  beginMeasurement(f)
 end''', 1)
    controller.write_text(text)


def stage(campaign, aa_samples=0):
    assert campaign in ORDER
    assert aa_samples == 0
    assert not (RUN_ROOT / campaign / 'preparation').exists(), 'Prior staged output preserved'
    spec = json.loads((U / 'SPEC.json').read_text())
    prods = products()
    assert sha(MAP_SOURCE) == spec['sharedScene']['mapSHA256']
    assert sha(FONT_SOURCE) == FONT_SHA256
    assert sha(BATTLE / 'start.txt') == 'e52e88c99e24e5b7a02b55c5b7ea8ea261fa1d75ea54e59007433956f26d8022'
    pace = common.read(PACING / 'PACING.json')
    stage_root = RUN_ROOT / campaign / 'preparation'
    rows = []
    for arm, mode in ORDER[campaign]:
        product = prods[mode]
        data, out, env, plan = base.harness.stage_arm(stage_root, arm, product, base.frozen.ASSETS)
        for amended in base.policy.apply(data, 'BC213'):
            plan['checksums'][amended['path']] = amended['afterSHA256']
        font = data / 'fonts/FreeSansBold.otf'
        font.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(FONT_SOURCE, font)
        assert sha(font) == FONT_SHA256
        plan['checksums'][str(font)] = FONT_SHA256
        root_font = data / 'FreeSansBold.otf'
        shutil.copyfile(FONT_SOURCE, root_font)
        assert sha(root_font) == FONT_SHA256
        plan['checksums'][str(root_font)] = FONT_SHA256
        home = data.parents[1] / 'home'
        home.mkdir(exist_ok=True)
        for source, target in ((BATTLE / 'battle_control.lua', data / 'LuaUI/Widgets/zzz_native_short_run.lua'),
                               (base.REF / 'backend-status.lua', data / 'LuaUI/Widgets/zzz_backend_common_status.lua'),
                               (U / 'ultra_quality.lua', data / 'LuaUI/Widgets/zzz_ultra_quality.lua')):
            shutil.copyfile(source, target)
            plan['checksums'][str(target)] = sha(target)
        configure_controller(data / 'LuaUI/Widgets/zzz_native_short_run.lua')
        if campaign in ('clean_paced', 'validation_paced'):
            base.image['configure'](data / 'LuaUI/Widgets/zzz_native_short_run.lua')
        controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
        plan['checksums'][str(controller)] = sha(controller)
        start_text = (BATTLE / 'start.txt').read_text()
        assert start_text.count('spectator=0;') == 1
        (data / 'script.txt').write_text(start_text.replace('spectator=0;', 'spectator=1;', 1))
        plan['checksums'][str(data / 'script.txt')] = sha(data / 'script.txt')
        prior_overlay = data / 'games/bar-commander-control-213c1.sdz'
        prior_overlay.unlink()
        plan['checksums'].pop(str(prior_overlay))
        overlay = data / 'games/bar-official-combat-pr6.sdz'
        shutil.copyfile(BATTLE / 'bar-official-combat-pr6.sdz', overlay)
        plan['checksums'][str(overlay)] = sha(overlay)
        map_target = data.parents[1] / 'assets/maps/seths_ravine_remake_1.3.1.sd7'
        map_target.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(['/bin/cp', '-c', str(MAP_SOURCE), str(map_target)], check=True)
        plan['checksums'][str(map_target)] = sha(map_target)
        for name, rel in (('warmup_pacing.lua', 'LuaUI/Widgets/Include/native_warmup_pacing.lua'),
                          ('zzz_native_warmup_pacing.lua', 'LuaUI/Widgets/zzz_native_warmup_pacing.lua')):
            target = data / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(PACING / name, target)
            assert sha(target) == pace['hashes'][name]
            plan['checksums'][str(target)] = sha(target)
        widget_cfg = data / 'LuaUI/Config/BC213.lua'
        original = widget_cfg.read_text()
        assert 'order = { ["Limit idle FPS"] = 0 }' in original
        widget_cfg.write_text(original.replace('order = { ["Limit idle FPS"] = 0 }',
             'order = { ["Limit idle FPS"] = 0, ["Native Precount Warmup Pacing"] = 1, ["Ultra Effective Probe"] = 1 }', 1))
        plan['checksums'][str(widget_cfg)] = sha(widget_cfg)
        plan['pacing'] = dict(contract=pace, originalWidgetConfigSHA256=base.policy.digest(original),
                              effectiveWidgetConfigSHA256=sha(widget_cfg), widgetEnabledByOrder=True)
        settings = configure_settings(data, aa_samples)
        plan['checksums'][str(data / 'springsettings.cfg')] = sha(data / 'springsettings.cfg')
        if mode == 'MVK3':
            env = old_runner.historical_env(data, data.parents[1] / 'assets', home, 'MVK3')
            assert str(data.parents[1] / 'assets') in env['SPRING_DATADIR']
            assert '/engine-fonts:' in env['SPRING_DATADIR'], 'Historical font search route absent'
            env['XDG_CACHE_HOME'] = str(data.parents[1] / 'cache')
            env['BAR_MACOS_TARGET_DISPLAY'] = '0,0,1512,982'
        else:
            wrapper, shipped = base.wrapper_environment()
            env = {k: v for k, v in os.environ.items() if not k.startswith(base.STRIP_PREFIXES)}
            env.update(plan['env'])
            env.update(shipped)
            env.update(BAR_NATIVE_PREPARE_READY_PILOT='1', BAR_NATIVE_BEGIN_READY_PILOT='0',
                       BAR_NATIVE_BEGIN_PREPARE_OVERLAP='1', BAR_NATIVE_FINAL_COLOR_STAGE='1',
                       BAR_NATIVE_SCREEN_STATE_ELISION='1', BAR_NATIVE_MODEL_TYPE_FACTS='1',
                       BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP='0', BAR_NATIVE_MATERIAL_STORAGE_POOL='0',
                       BAR_NATIVE_MATERIAL_STORAGE_DIAGNOSTICS='0', BAR_NATIVE_SHARED_PAIR_KEY_VIEW='0',
                       BAR_METAL4_SHARED_CUS_PILOT='0', BAR_NATIVE_GRASS_BOUNDARY_S01='1',
                       BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE='0',
                       BAR_NATIVE_GROUND_DECAL_INDEX_G01='1',
                       BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE=str(int(campaign == 'validation_paced')),
                       BAR_NATIVE_FRAME_TRACE='0',
                       BAR_NATIVE_FRAME_TRACE_EVERY_LOOP='0', MTL_DEBUG_LAYER=str(int(campaign == 'validation_paced')),
                       MTL_SHADER_VALIDATION='0')
            if campaign == 'validation_paced':
                env['MTL_LOG_TO_STDERR'] = '1'
            env.pop('BAR_METAL4_SEPIA_PATH', None)
            env['HOME'] = str(home)
            env['XDG_CACHE_HOME'] = str(data.parents[1] / 'cache')
            env['BAR_MACOS_TARGET_DISPLAY'] = '0,0,1512,982'
        Path(env['XDG_CACHE_HOME']).mkdir(parents=True, exist_ok=True)
        plan['env'] = {k: v for k, v in env.items() if k.startswith(base.STRIP_PREFIXES) or
                       k in ('HOME', 'XDG_CACHE_HOME', 'SPRING_DATADIR', 'MTL_LOG_TO_STDERR')}
        plan['env'].update(BAR_STATE_PATH=str(data.parents[1] / 'state'),
                           BAR_ASSETS_PATH=str(data.parents[1] / 'assets'))
        env.update(plan['env'])
        plan['product'] = product
        plan['argv'][0] = product['binary']
        plan['stagedControllerSHA256'] = sha(controller)
        plan['ultra'] = dict(mode=mode, settings=settings, qualityRun=campaign == 'validation_paced',
                             expectedView=[1512, 950], expectedDrawable=[1512, 982], actualSamples=aa_samples, storagePool=0,
                             privateFontSHA256=FONT_SHA256,
                             gameArchive=spec['sharedScene']['currentGameArchive'],
                             gameArchiveSHA256=spec['sharedScene']['currentGameArchiveSHA256'])
        archive = data.parents[1] / 'assets/packages' / spec['sharedScene']['currentGameArchive']
        assert sha(archive) == spec['sharedScene']['currentGameArchiveSHA256']
        save(out / 'LAUNCH.json', plan)
        rows.append(dict(arm=arm, backend=mode, data=str(data), out=str(out),
                         binarySHA256=product['sha256'], controllerSHA256=sha(controller),
                         ultraProbeSHA256=sha(data / 'LuaUI/Widgets/zzz_ultra_quality.lua'),
                         configSHA256=sha(data / 'springsettings.cfg'),
                         privateFontSHA256=sha(font),
                         gameArchiveSHA256=sha(archive), mapSHA256=sha(map_target)))
    assert len({r['controllerSHA256'] for r in rows}) == 1
    assert len({r['ultraProbeSHA256'] for r in rows}) == 1
    assert len({r['configSHA256'] for r in rows}) == 1
    assert len({r['gameArchiveSHA256'] for r in rows}) == 1
    save(RUN_ROOT / campaign / 'PREPARED.json', dict(status='READY_NO_GAME', order=rows,
        campaign=campaign, sourceController=str(BATTLE / 'battle_control.lua'),
        sourceProbe=str(U / 'ultra_quality.lua'),
        pacingHashes=pace['hashes'], comparisonDecision='Frozen R03 versus GroundDecal pages validation only; S01/G01 both1; images after count',
        actualUltraRuntime='pending'))
    print('READY ' + campaign + ': static staged, games 0', flush=True)


def restore(campaign, completed_arms=()):
    receipt = common.read(RUN_ROOT / campaign / 'PREPARED.json')
    assert receipt['status'] == 'READY_NO_GAME'
    prepared = {}
    for item in receipt['order']:
        if item['arm'] in completed_arms:
            continue
        out = Path(item['out'])
        plan = common.read(out / 'LAUNCH.json')
        assert sha(plan['product']['binary']) == item['binarySHA256']
        assert all(sha(path) == expected for path, expected in plan['checksums'].items())
        assert not (out / 'OWNED.json').exists(), 'Prior run is immutable'
        env = {k: v for k, v in os.environ.items() if not k.startswith(base.STRIP_PREFIXES)}
        env.update(plan['env'])
        prepared[item['arm']] = (Path(item['data']), out, env, plan)
    return prepared


def evidence(out, mode, quality, aa_samples):
    info = (out / 'infolog.txt').read_text()
    log = (out / 'engine.log').read_text()
    assert not re.search(r'\[UltraQuality\] FAILED_|\[MatchedGameplay\] FAILED_|\[BattleBenchmark\] FAILED_', info)
    requested = re.findall(r'\[UltraPreset\] requested_index=4 [^\n]+', info)
    observed = re.findall(r'\[UltraPreset\] observed=1 [^\n]+', info)
    assert len(requested) == len(observed) == 1
    assert 'graphicsPreset=ultra selected_index=4' in observed[0]
    expected_config = ('MSAA=1 MSAALevel=4' if aa_samples == 4 else 'MSAA=0 MSAALevel=0')
    assert 'view=1512,950' in observed[0] and expected_config + ' MinSampleShadingRate=0.000000' in observed[0]
    assert 'option_limitoffscreenfps=false' in observed[0]
    power = re.findall(r'\[FPSPowerStatus\] ([^\n]+)', info)
    assert len(power) == 1 and all(x in power[0] for x in ('frame=480', 'LimitIdleFps=0',
        'option_limitoffscreenfps=false', 'VSync=0', 'VSyncGame=0'))
    camera = re.findall(r'\[BattleCameraOpen\] ([^\n]+)', info)
    initial = re.findall(r'\[BattleInitialState\] ([^\n]+)', info)
    visibility = re.findall(r'\[BattleVisibility\] frame=(240|540) ([^\n]+)', info)
    created = re.findall(r'Benchmark complete, #created = (\d+), #destroyed = (\d+)', info)
    assert len(camera) == len(initial) == len(created) == 1
    assert [v[0] for v in visibility] == ['240', '540']
    assert all('spectating=true fullview=true' in v[1] for v in visibility)
    assert 'view=1512,950,0,0' in camera[0]
    anchors = re.findall(r'\[MatchedAnchor\] boundary=(start|end) [^\n]*simulation=(\d+)', info)
    assert anchors == [('start', '540'), ('end', '1740')]
    if mode == 'MVK3':
        expected_egl = ('configured=4 requested=4 selected=4 sampleBuffers=1' if aa_samples == 4
                        else 'configured=0 requested=0 selected=0 sampleBuffers=0')
        assert re.search(r'\[MacSDL3EGL::TryChooseExactConfig\] ' + expected_egl, log)
        assert re.search(r'GL renderer\s*:\s*zink Vulkan .*MOLTENVK', log)
        assert f'gl_samples_drawscreen={aa_samples}' in observed[0], 'Scoped GL sample receipt absent'
        assert common.read(out / 'BACKEND-LOADED.json')['status'] == 'PASS_PRECOUNT_MVK3_STACK'
    else:
        sample = re.findall(r'\[NativeFinalColorStage\] active=(\d+) samples=(\d+) [^\n]*width=(\d+) height=(\d+)', info)
        expected_native = ('0', '4') if aa_samples == 4 else ('1', '1')
        assert sample and any(row[:2] == expected_native for row in sample), sample
        assert re.search(r'\[NativeMRT164\].*encoded=1', info)
    assert (out / 'scene.png').is_file() and (out / 'scene.png').stat().st_size > 1000
    assert re.search(r'\[H1ValidationFrame\] sim=1740 ', info)
    assert len(observed) == 1, 'Preset probe was still active after its pre-combat observation'
    receipt = dict(status='PASS_RUNTIME_ULTRA_AA', backend=mode, qualityRun=quality,
        actualView=[1512, 950], requestedAA=aa_samples,
        actualMainSamples=aa_samples if mode == 'MVK3' else (4 if aa_samples == 4 else 1),
        ultraRequest=requested[0], ultraWitness=observed[0],
        nativeFinalColorExtents=sample if mode == 'native' else None,
        powerWitness=power[0], camera=camera[0], initialState=initial[0],
        visibility=[v[1] for v in visibility],
        officialCreated=int(created[0][0]), officialDestroyed=int(created[0][1]),
        image=str(out / 'scene.png'),
        engineSHA256=sha(common.read(out / 'LAUNCH.json')['product']['binary']),
        scopedGLSamplesIsNotSoleMainTargetProof=True)
    save(out / 'ULTRA-EVIDENCE.json', receipt)
    return receipt


def grass_boundary_evidence(out, expected_enabled):
    log = (out / 'engine.log').read_text()
    rows = re.findall(r'\[NativeGrassBoundaryS01\] source_frame=(\d+) submission=(\d+) plan=(\d+) checkpoint=(\d+) split=(\d+) grass_draws=(\d+) preserved_order=(\d+) producer_waits_unchanged=(\d+)', log)
    positive = [row for row in rows if row[2:5] == ('1', '1', '1') and row[6:] == ('1', '1')]
    receipt = dict(status='POSITIVE_SPLIT' if positive else 'NO_POSITIVE_SPLIT',
                   expectedEnabled=expected_enabled, rows=rows,
                   source='engine.log', cleanFPSNotInferred=True)
    save(out / 'GRASS-BOUNDARY.json', receipt)
    if expected_enabled:
        assert positive, 'S01 positive grass split absent; preserve raw and stop before clean'
    else:
        assert not rows, 'S01 marker appeared in baseline OFF run'
    return receipt


def ground_index_evidence(out):
    log = (out / 'engine.log').read_text()
    rows = re.findall(r'\[NativeGroundDecalIndexG01\] source_frame=(\d+) indexed=(\d+) index_count=(\d+) unique_ids=(\d+) index_bytes=(\d+) instances=(\d+) samples=(\d+) reason=(\d+) base_vertex=(\d+) base_instance=(\d+)', log)
    positive = [row for row in rows if row[1:5] == ('1', '36', '8', '72')
                and int(row[5]) > 0 and row[6:] == ('1', '0', '0', '0')]
    receipt = dict(status='POSITIVE_INDEXED' if positive else 'NO_POSITIVE_INDEXED',
                   rows=rows, positive=positive, source='engine.log', cleanFPSNotInferred=True)
    save(out / 'GROUND-INDEX.json', receipt)
    assert positive, 'G01 actual indexed draw absent; preserve raw and stop'
    return receipt


def metal_validation_raw(out, plan, cleanup):
    log_path = out / 'engine.log'
    if not log_path.exists():
        receipt = dict(status='NOT_EVALUATED', enabledByChildEnv=plan['env'].get('MTL_DEBUG_LAYER') == '1',
                       enabledByEngineLog=False, errors=None, normalExit=cleanup.get('normalExit'),
                       candidateSHA256=plan['product']['sha256'], reason='engine.log absent')
    else:
        engine_log = log_path.read_text()
        errors = [line for line in engine_log.splitlines() if re.search(
            r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)', line, re.I)]
        enabled = 'Metal API Validation Enabled' in engine_log
        receipt = dict(status=('ERROR' if errors else 'NO_MATCHED_METAL_ERRORS' if enabled else 'NOT_EVALUATED'),
                       enabledByChildEnv=plan['env'].get('MTL_DEBUG_LAYER') == '1',
                       enabledByEngineLog=enabled, errors=errors, normalExit=cleanup.get('normalExit'),
                       candidateSHA256=plan['product']['sha256'],
                       image=str(out / 'scene.png') if (out / 'scene.png').is_file() else None,
                       source='raw engine.log; same original error regex')
    save(out / 'METAL-VALIDATION.json', receipt)
    return receipt


def run(campaign, resume=False):
    assert campaign in ORDER
    completed_arms = ('A1_MVK3',) if resume else ()
    prepared = restore(campaign, completed_arms)
    suffix = '-RECOVERY' if resume else ''
    assert common.read(base.SHARED_AWAKE)['status'] == 'RELEASED'
    caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '2400', '-w', str(os.getpid())])
    awake = dict(status='ACTIVE', campaign=campaign, pid=caffeine.pid, startUTC=base.utc(),
                 maximumSeconds=2400, securitySettingsChanged=False)
    save(base.SHARED_AWAKE, awake)
    save(RUN_ROOT / campaign / f'AWAKE{suffix}.json', awake)
    rows = ([common.read(RUN_ROOT / campaign / 'preparation/runs/A1_MVK3/RECOVERY-RESULT.json')]
            if resume else [])
    try:
        for index, (arm, mode) in enumerate(ORDER[campaign]):
            if arm in completed_arms:
                continue
            data, out, env, plan = prepared[arm]
            base.active.update(prepared=prepared[arm], mapped=False, campaign=campaign)
            stop = threading.Event()
            issues = []
            image_worker = threading.Thread(target=base.image['capture'], args=(data, out, stop, issues))
            if image_worker:
                image_worker.start()
            try:
                print('OPEN ' + arm, flush=True)
                row, launch_error = None, None
                try:
                    row = base.harness.launch(RUN_ROOT / campaign / 'preparation', arm, prepared[arm],
                                              len(ORDER[campaign]) - index)
                except BaseException:
                    launch_error = traceback.format_exc()
                finally:
                    stop.set()
                    if image_worker:
                        image_worker.join()
                    save(out / 'OBSERVERS.json', dict(imagePresent=(out / 'scene.png').is_file(),
                        issues=issues, launchOrParserError=bool(launch_error)))
                cleanup = common.read(out / 'CLEANUP.json') if (out / 'CLEANUP.json').exists() else {}
                if launch_error:
                    save(out / 'POSTRUN-ERROR.json', dict(error=launch_error))
                    if campaign == 'clean_paced' or not cleanup.get('normalExit'):
                        raise RuntimeError('Owned game failed: ' + launch_error)
                assert cleanup.get('normalExit') and base.active['mapped']
                validation = metal_validation_raw(out, plan, cleanup) if campaign == 'validation_paced' else None
                proof = evidence(out, mode, campaign == 'validation_paced', plan['ultra']['actualSamples'])
                grass_boundary_evidence(out, True)
                if campaign == 'validation_paced':
                    ground_index_evidence(out)
                else:
                    assert '[NativeGroundDecalIndexG01]' not in (out / 'engine.log').read_text(), 'G01 probe marker in clean run'
                if campaign == 'clean_paced':
                    assert row and row['draws'] > 0
                    pace = base.pacing_evidence(out, plan, (out / 'infolog.txt').read_text())
                    assert pace
                    if rows:
                        assert proof['initialState'] == rows[0]['ultra']['initialState']
                        assert proof['camera'] == rows[0]['ultra']['camera']
                        assert (proof['officialCreated'], proof['officialDestroyed']) == (
                            rows[0]['ultra']['officialCreated'], rows[0]['ultra']['officialDestroyed'])
                    row['ultra'] = proof
                    save(out / 'RESULT.json', row)
                    print(f"DONE {arm}: {row['FPS']:.3f} FPS; 1% low {row['frameStats']['slow1PercentFPS']:.3f}", flush=True)
                else:
                    assert validation and validation['status'] == 'NO_MATCHED_METAL_ERRORS', 'Metal validation requires review'
                    row = dict(run=arm, ultra=proof, performanceAccepted=False,
                               metalValidation=validation,
                               diagnosticFPSParse='EXCLUDED' if launch_error else 'PASSED_NOT_POOLED')
                    save(out / 'RESULT.json', row)
                    print('DONE ' + arm + ': validation and image; diagnostic FPS excluded', flush=True)
                rows.append(row)
                save(RUN_ROOT / campaign / f'RUNS{suffix}.json', rows)
            except BaseException:
                save(out / 'FAILURE.json', dict(arm=arm, error=traceback.format_exc(), utc=base.utc()))
                save(RUN_ROOT / campaign / f'COMPLETION{suffix}.json', dict(status='STOPPED_ON_FAILURE',
                     completed=len(rows), arm=arm, utc=base.utc()))
                raise
            finally:
                for rel in ('LuaUI/Widgets/zzz_ultra_quality.lua', 'games/bar-official-combat-pr6.sdz'):
                    target = data / rel
                    if target.exists():
                        assert sha(target) == plan['checksums'][str(target)]
                        target.unlink()
        if campaign == 'validation_paced':
            completion = dict(status='TWO_ARM_VALIDATION_REVIEW_REQUIRED', runs=len(rows),
                images=[r['ultra']['image'] for r in rows],
                metalValidation=[r['metalValidation'] for r in rows], utc=base.utc())
        else:
            means = {mode: statistics.mean(r['FPS'] for r in rows if r['ultra']['backend'] == mode)
                     for mode in ('baseline', 'candidate')}
            completion = dict(status='COMPLETE', runs=6, means=means,
                candidateVsBaselinePercent=100 * (means['candidate'] / means['baseline'] - 1),
                allValidSlowRunsRetained=True, historicalSamplesPooled=False, utc=base.utc())
        save(RUN_ROOT / campaign / f'COMPLETION{suffix}.json', completion)
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
            save(RUN_ROOT / campaign / f'AWAKE-RELEASED{suffix}.json', awake)
            save(RUN_ROOT / campaign / f'OWNER-CLOSED{suffix}.json', dict(status='CLOSED',
                 awakeReleased=True, completed=len(rows), utc=base.utc()))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--suite', help='Private output name; --stage chooses a fresh one when omitted')
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--list', action='store_true')
    action.add_argument('--check-only', action='store_true')
    action.add_argument('--stage', choices=tuple(ORDER))
    action.add_argument('--run', choices=tuple(ORDER))
    args = parser.parse_args()
    global RUN_ROOT
    if args.list:
        print('validation_paced: sequential R03 baseline and GroundDecal candidate Metal API validation+image; S01/G01 both1; Ultra AA0 1512x950, official Seths combat; no clean suite')
        return
    if args.stage and not args.suite:
        args.suite = 'ground_decal_' + datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%d_%H%M%S_%f')
    if args.run and not args.suite:
        parser.error('A run requires the fresh --suite printed by --stage')
    if args.suite:
        assert re.fullmatch(r'[a-z][a-z0-9_-]{1,40}', args.suite), 'Invalid suite name'
        RUN_ROOT = HERE / 'runs' / args.suite
    if args.stage:
        stage(args.stage, 0)
        print('SUITE ' + args.suite, flush=True)
    elif args.run:
        run(args.run)
    else:
        print(json.dumps({'status':'STATIC_PINS_ONLY','products':products(),
            'sourceComparison':'Fixed GroundDecal pages candidate versus frozen R03; S01/G01 ON in both','games':0}, indent=2))


if __name__ == '__main__':
    main()
