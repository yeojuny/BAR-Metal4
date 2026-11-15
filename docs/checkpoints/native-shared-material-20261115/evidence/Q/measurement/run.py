"""Two private Sepia functional games; no FPS comparison or production change."""

import argparse
import datetime
import hashlib
import importlib.util
import json
import os
import re
import shutil
import subprocess
import sys
import traceback
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
Q = HERE.parent
EVIDENCE = Q.parent
BASE_RUNNER = EVIDENCE/'screen-state-elision-20261115-01/measurement/run_ac0_prelaunch.py'
REF = EVIDENCE/'worldinfo245-runtime01'
PACING = HERE/'pacing'
PRODUCT = HERE/'PRODUCT.json'
SHARED_AWAKE = EVIDENCE/'gpu-next219-01/AWAKE.json'
ASSEMBLER = HERE/'assemble_private_fixture.py'
ORDERS = {'functional': [('F1_OLD_SEPIA','old'), ('F2_SHARED_SEPIA','shared')]}


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


base = load(BASE_RUNNER, 'v2_reference_runner')
common = base.common
harness = base.harness
policy = base.policy
fixture = load(ASSEMBLER, 'v2_fixture_assembler')
base.CHILD_KEYS += ('BAR_METAL4_SEPIA_PATH', 'BAR_METAL4_SHARED_CUS_PILOT',
                    'BAR_NATIVE_SCREEN_STATE_ELISION', 'BAR_NATIVE_FINAL_COLOR_STAGE',
                    'BAR_NATIVE_SCREEN_TABLE_H5', 'BAR_NATIVE_SCREEN_TABLE_H5_COUNTERS')


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def pinned_product():
    pin = common.read(PRODUCT)
    binary = Path(pin['binary'])
    native = Path(pin['native'])
    package = Path(pin['package'])
    assert binary == native/'bin/spring-metal4-scene'
    assert re.fullmatch(r'[0-9a-f]{64}', pin['sha256']) and sha(binary) == pin['sha256']
    subprocess.run(['/usr/bin/codesign','--verify','--strict',str(binary)],check=True)
    subprocess.run(['/usr/bin/codesign','--verify','--deep','--strict',str(package)],check=True)
    for name in ('gfx_sepia_tone.lua','sepia_pilot_program.lua'):
        runtime = native/'data/LuaUI/NativeWidgets'/name
        source = Q/'source/engine/data/LuaUI/NativeWidgets'/name
        assert sha(runtime) == sha(source), ('Native widget data drift', name)
    loader = (Q/'source/engine/rts/Lua/NativeWidgetAdapters.h').read_text()
    wrapper_digest = hashlib.sha512((native/'data/LuaUI/NativeWidgets/gfx_sepia_tone.lua').read_bytes()).hexdigest()
    assert wrapper_digest in loader, 'Strict native-widget loader hash differs from staged wrapper'
    generated = Q/'source/engine/rts/Rendering/Metal4/NativeSepiaGeneratedProgram.json'
    layout = common.read(generated)
    assert layout['uniformBytes'] == 160
    expected = {'viewPos':48,'exposure':64,'bias':80,'gamma':96,
                'colorR':112,'colorG':128,'colorB':144}
    assert {field['name']:field['offset'] for field in layout['fields'] if field['name'] in expected} == expected
    return dict(variant='private-v2-sepia-functional',binary=str(binary),native=str(native),
                package=str(package),sha256=pin['sha256'],generatedLayoutSHA256=sha(generated),
                nativeWrapperSHA256=sha(native/'data/LuaUI/NativeWidgets/gfx_sepia_tone.lua'),
                generatedModuleSHA256=sha(native/'data/LuaUI/NativeWidgets/sepia_pilot_program.lua'))


def wrapper_environment(package):
    wrapper = package/'Contents/MacOS/BAR-Metal4'
    text = wrapper.read_text()
    values = {key:value.strip().strip("'\"") for key,value in re.findall(
        r'^export ((?:BAR_NATIVE_|BAR_METAL4_EXPERIMENTAL_SINGLEPLAYER|MTL_)[A-Z0-9_]*)=(.*)$',text,re.M)}
    assert all(values[k]=='1' for k in ('BAR_NATIVE_INTERVENTION_RENDER_WORKER',
        'BAR_NATIVE_CAPTURE_READY_BOUNDARY','BAR_NATIVE_BEGIN_PREPARE_OVERLAP',
        'BAR_NATIVE_FRAME_OVERLAP','BAR_NATIVE_CAPTURE_OVERLAP','BAR_NATIVE_PREPARE_OVERLAP',
        'BAR_NATIVE_FINAL_COLOR_STAGE'))
    values['BAR_NATIVE_BEGIN_READY_PILOT']='0'
    values['BAR_NATIVE_PREPARE_READY_PILOT']='1'
    return wrapper,values


def functional_parse(out, data, arm, stage):
    """Record raw timing after normal exit; functional gates run after artifact copy."""
    log = (out/'infolog.txt').read_text(errors='replace') if (out/'infolog.txt').exists() else ''
    total = re.findall(r'\[NativeShortRun\] total draws=(\d+) seconds=([0-9.]+)',log)
    return dict(run=arm,draws=int(total[0][0]) if len(total)==1 else None,
                seconds=float(total[0][1]) if len(total)==1 else None,
                matchedFPSSampleCountObserved=len(re.findall(r'\[MatchedFPS\]',log)),
                performanceAccepted=False,rawTimingOnly=True)


harness.parse = functional_parse


def prepare():
    campaign='functional';stage=HERE/campaign/'preparation'
    assert not stage.exists(), 'Fresh private profiles only; preserve prior stage'
    product=pinned_product()
    wrapper,shipped=wrapper_environment(Path(product['package']))
    pace=common.read(PACING/'PACING.json')
    for name in ('warmup_pacing.lua','zzz_native_warmup_pacing.lua'):
        assert sha(PACING/name)==pace['hashes'][name]
    assert pace['sharedControllerEdits']==0 and pace['pauseRequestSimulation']==180
    assert pace['ackDeadlineSimulation']==240 and pace['resumeMinimumWallSeconds']==10
    stage.mkdir(parents=True)
    common.save(stage.parent/'DESIGN.json',dict(status='FUNCTIONAL_ONLY',product=product,
        order=ORDERS[campaign],wrapperSHA256=sha(wrapper),originalControllerSHA256=sha(REF/'control.lua'),
        assemblerSHA256=sha(ASSEMBLER),probeSHA256=sha(HERE/'v2_functional_widget.lua'),
        pacing=pace,scene='BC213 sim300..1500 same camera and pause sim1500',
        performanceAccepted=False,cusPilot='0 both arms',sourceAndReleaseUnchanged=True))
    prepared=[]
    for arm,mode in ORDERS[campaign]:
        data,out,env,plan=harness.stage_arm(stage,arm,product,base.frozen.ASSETS)
        for amendment in policy.apply(data,'BC213'):
            plan['checksums'][amendment['path']]=amendment['afterSHA256']
        home=data.parents[1]/'home';home.mkdir()
        env['HOME']=plan['env']['HOME']=str(home)
        for source,target in ((REF/'control.lua',data/'LuaUI/Widgets/zzz_native_short_run.lua'),
                              (REF/'bar-commander-control-213c1.sdz',data/'games/bar-commander-control-213c1.sdz'),
                              (REF/'backend-status.lua',data/'LuaUI/Widgets/zzz_backend_common_status.lua')):
            shutil.copyfile(source,target);plan['checksums'][str(target)]=sha(target)
        for name,relative in (('warmup_pacing.lua','LuaUI/Widgets/Include/native_warmup_pacing.lua'),
                              ('zzz_native_warmup_pacing.lua','LuaUI/Widgets/zzz_native_warmup_pacing.lua')):
            target=data/relative;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(PACING/name,target);assert sha(target)==pace['hashes'][name]
            plan['checksums'][str(target)]=sha(target)
        cfg=data/'LuaUI/Config/BC213.lua';text=cfg.read_text()
        assert text.count('order = { ["Limit idle FPS"] = 0 }')==1
        cfg.write_text(text.replace('order = { ["Limit idle FPS"] = 0 }',
            'order = { ["Limit idle FPS"] = 0, ["Native Precount Warmup Pacing"] = 1 }',1))
        plan['checksums'][str(cfg)]=sha(cfg)
        base_module=Path(product['native'])/'data/LuaUI/NativeWidgets/sepia_pilot_program.lua'
        subprocess.run([sys.executable,'-B',str(ASSEMBLER),'--mode',mode,'--data',str(data),
                        '--generated-base',str(base_module)],check=True,capture_output=True,text=True)
        for relative in ('LuaUI/Widgets/zzz_native_short_run.lua','LuaUI/Widgets/zzz_v2_functional_probe.lua',
                         'LuaUI/Config/BC213.lua','LuaUI/Config/v2-functional-fixture.json','springsettings.cfg'):
            target=data/relative;plan['checksums'][str(target)]=sha(target)
        generated=data/'LuaUI/NativeWidgets/sepia_pilot_program.lua'
        if generated.exists():plan['checksums'][str(generated)]=sha(generated)
        for key in list(env):
            if key.startswith('BAR_NATIVE_'):
                env.pop(key,None);plan['env'].pop(key,None)
        env.update(shipped);plan['env'].update(shipped)
        overrides=dict(BAR_NATIVE_BEGIN_READY_PILOT='0',BAR_NATIVE_PREPARE_READY_PILOT='1',
            BAR_NATIVE_FINAL_COLOR_STAGE='1',BAR_NATIVE_SCREEN_STATE_ELISION='1',
            BAR_NATIVE_SCREEN_TABLE_H5='0',BAR_NATIVE_SCREEN_TABLE_H5_COUNTERS='0',
            BAR_NATIVE_FRAME_TRACE='0',BAR_NATIVE_FRAME_TRACE_EVERY_LOOP='0',
            BAR_METAL4_SEPIA_PATH=mode,BAR_METAL4_SHARED_CUS_PILOT='0',
            MTL_DEBUG_LAYER=str(int(mode=='shared')),MTL_SHADER_VALIDATION='0')
        env.update(overrides);plan['env'].update(overrides)
        if mode=='shared':env['MTL_LOG_TO_STDERR']=plan['env']['MTL_LOG_TO_STDERR']='1'
        plan.update(pacing=dict(contract=pace,widgetEnabledByOrder=True),
                    measurementWindow=dict(startSimulation=300,endSimulation=1500,
                                           warmupSeconds=10,countedSeconds=40,performanceAccepted=False),
                    functionalFixture=common.read(data/'LuaUI/Config/v2-functional-fixture.json'))
        common.save(out/'LAUNCH.json',plan)
        prepared.append(dict(arm=arm,mode=mode,data=str(data),out=str(out)))
    common.save(stage.parent/'PREPARED.json',dict(status='READY_NO_GAMES',utc=utc(),
        productSHA256=product['sha256'],arms=prepared))
    print('READY; no games launched',flush=True)


def restore():
    stage=HERE/'functional/preparation'
    receipt=common.read(stage.parent/'PREPARED.json')
    assert receipt['status']=='READY_NO_GAMES'
    product=pinned_product()
    assert product['sha256']==receipt['productSHA256']
    assert sha(Path(product['package'])/'Contents/MacOS/BAR-Metal4') == \
           common.read(stage.parent/'DESIGN.json')['wrapperSHA256']
    prepared={}
    for entry in receipt['arms']:
        out=Path(entry['out']);plan=common.read(out/'LAUNCH.json')
        env={k:v for k,v in os.environ.items() if not k.startswith(base.STRIP_PREFIXES)}
        env.update(plan['env'])
        prepared[entry['arm']]=(Path(entry['data']),out,env,plan)
    return prepared


def fields(line):
    return dict(re.findall(r'(\w+)=([^\s]+)',line))


def preserve_artifacts(out, data, mode):
    info_path=out/'infolog.txt' if (out/'infolog.txt').exists() else data/'infolog.txt'
    info=info_path.read_text(errors='replace') if info_path.exists() else ''
    images=[]
    for name in (['old-identity'] if mode=='old' else ['shared-identity','shared-variant']):
        src=data/('LuaUI/Config/v2-'+name+'.png')
        if src.exists():
            dst=out/src.name
            if not dst.exists():shutil.copyfile(src,dst)
            images.append(dict(kind=name,path=str(dst),bytes=dst.stat().st_size,sha256=sha(dst)))
    raw=dict(mode=mode,images=images,
             functionalLines=[line for line in info.splitlines() if '[V2Functional]' in line],
             generatedUniformLines=[line for line in info.splitlines() if '[V2GeneratedUniforms]' in line],
             warmupLines=[line for line in info.splitlines() if '[V2FunctionalWarmup]' in line or '[WarmupPacing]' in line],
             scope='Copied before functional/timing classification, including on failure')
    common.save(out/'RAW-FUNCTIONAL-ARTIFACTS.json',raw)
    return raw


def functional_evidence(out, data, plan, row, mode):
    artifacts=preserve_artifacts(out,data,mode)
    info=(out/'infolog.txt').read_text(errors='replace')
    engine_log=(out/'engine.log').read_text(errors='replace')
    assert base.active['mapped'], 'Actual child environment proof absent'
    child=common.read(out/'CHILD-MODE.json')
    assert child['status']=='PASS_ACTUAL_CHILD_ENVIRONMENT'
    assert child['actual']['BAR_METAL4_SEPIA_PATH']==mode
    assert child['actual']['BAR_METAL4_SHARED_CUS_PILOT']=='0'
    assert child['actual']['BAR_NATIVE_PREPARE_READY_PILOT']=='1'
    assert child['actual']['BAR_NATIVE_FINAL_COLOR_STAGE']=='1'
    assert child['actual']['BAR_NATIVE_SCREEN_STATE_ELISION']=='1'
    assert child['actual']['BAR_NATIVE_FRAME_TRACE']=='0'
    assert '[MatchedGameplay] FAILED_' not in info and '[V2Functional] FAILED_' not in info
    assert '[Commander213Complete] sim=1500' in info
    assert re.search(r'\[NativeFinalColorStage\] active=1 samples=1 present_order_original=1 width=1512 height=982',info)
    release=[fields(x) for x in re.findall(r'\[NativeReleaseMode\] (.+)',info)]
    assert len(release)==1 and release[0]['mode']=='PREPARE'
    assert release[0]['eligible']==release[0]['queued']=='1' and 300<=int(release[0]['simulation'])<=1500
    assert re.search(r'\[NativeMRT164\].*encoded=1',info)
    assert re.search(r'\[NativePass166\] enabled=0 source_frame=300 submission=\d+ .*retired=1',info)
    assert re.search(r'\[NativePass165\] enabled=0 source_frame=300 submission=\d+ .*retired=1',info)
    backend=[fields(x) for x in re.findall(r'\[BackendCommonStatus\] (.+)',info)]
    assert [int(x['frame']) for x in backend]==[480,1500]
    assert all(x['view']=='1512,950,0,0' and x['MSAA']=='0' and x['VSync']=='0' for x in backend)
    markers=[fields(x) for x in re.findall(r'\[V2Functional\] (event=.+)',info)]
    events=[x['event'] for x in markers]
    expected=(['initialized','sepia_active','pause_ack','image','done'] if mode=='old' else
              ['initialized','sepia_active','pause_ack','image','config_update',
               'widget_disable_requested','widget_disabled','widget_enable_requested',
               'widget_relinked','image','done'])
    assert events==expected,(mode,events)
    assert all(x.get('sim')=='1500' and x.get('ok')=='true' for x in markers if x['event']=='image')
    images=[x for x in markers if x['event']=='image']
    assert len(set(x['camera'] for x in images))==1, 'Paused image camera changed'
    baseline=common.read(EVIDENCE/'bloom217-boundary02/build-recovery01/run/BASELINE.json')
    assert images[0]['camera']==baseline['expectedCamera'], 'Paused camera differs from fixed BC213 camera'
    names=(['old-identity'] if mode=='old' else ['shared-identity','shared-variant'])
    assert [x['kind'] for x in images]==names
    saved=artifacts['images']
    assert [x['kind'] for x in saved]==names
    assert all(x['bytes']>1000 and Path(x['path']).read_bytes().startswith(b'\x89PNG\r\n\x1a\n') for x in saved)
    generated=[fields(x) for x in re.findall(r'\[V2GeneratedUniforms\] (.+)',info)]
    if mode=='shared':
        assert len(generated)==2 and [x['exposure'] for x in generated]==['1.000','1.250']
        assert [x['bias'] for x in generated]==['0.000,0.000,0.000','0.100,0.200,0.300']
        assert re.search(r'Metal (?:API )?Validation (?:is )?Enabled',engine_log,re.I)
    else:
        assert not generated
    errors=[line for line in engine_log.splitlines() if re.search(
        r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)',line,re.I)]
    try:
        pace=base.pacing_evidence(out,plan,info)
    except Exception as error:
        pace=dict(status='OBSERVED_NOT_ACCEPTED',detail=str(error),performanceAccepted=False)
    timing=dict(matchedFPSSampleCount=row['matchedFPSSampleCountObserved'],
                countedSecondsObserved=row['seconds'],drawsObserved=row['draws'],
                warmupGate='OBSERVATIONAL_ONLY',pacing=pace)
    result=dict(mode=mode,images=saved,events=events,generatedUniforms=generated,
                pausedCamera=images[0]['camera'],pausedSimulation=1500,
                nativeRelease=release[0],backendStatus=backend,timingObservation=timing,
                nativeLinkUniformDeleteTrace='ABSENT_IN_FROZEN_SOURCE; source-backed traversal plus rendered response only',
                metalValidationEnabled=mode=='shared',metalValidationErrors=errors,
                cusPilotEnabled=False,cusPerformanceClaim=False,performanceAccepted=False,
                resultStatus='ERRORS_REVIEW_REQUIRED' if errors else 'FUNCTIONAL_EVIDENCE_CAPTURED')
    common.save(out/'FUNCTIONAL-WITNESS.json',result)
    if errors:raise RuntimeError('Metal Validation assertion/fault detected; raw functional artifacts preserved')
    return result


def run():
    campaign='functional';stage=HERE/campaign/'preparation';prepared=restore();rows=[]
    for index,(arm,mode) in enumerate(ORDERS[campaign]):
        data,out,env,plan=prepared[arm]
        base.active.update(prepared=prepared[arm],mapped=False,campaign=campaign)
        try:
            print('START '+arm,flush=True)
            row=harness.launch(stage,arm,prepared[arm],len(ORDERS[campaign])-index)
            witness=functional_evidence(out,data,plan,row,mode)
            row.update(mode=mode,functionalWitness=witness,performanceAccepted=False)
            common.save(out/'RESULT.json',row)
            rows.append(row)
            common.save(stage.parent/'RUNS.json',rows)
            print('DONE '+arm+': '+witness['resultStatus']+', images='+str(len(witness['images'])),flush=True)
        except BaseException:
            preserve_artifacts(out,data,mode)
            failure=dict(arm=arm,mode=mode,error=traceback.format_exc(),utc=utc())
            common.save(out/'FAILURE.json',failure)
            common.save(stage.parent/'COMPLETION.json',dict(status='STOPPED_ON_FAILURE',
                completed=len(rows),failure=failure,utc=utc()))
            raise
        finally:
            (data/'LuaUI/Widgets/zzz_backend_common_status.lua').unlink(missing_ok=True)
    statuses=[row['functionalWitness']['resultStatus'] for row in rows]
    common.save(stage.parent/'COMPLETION.json',dict(status='TWO_FUNCTIONAL_GAMES_CAPTURED',
        runs=len(rows),statuses=statuses,performanceAccepted=False,FPSCompared=False,
        images=[image for row in rows for image in row['functionalWitness']['images']],utc=utc()))
    print('FUNCTIONAL COMPLETE; no FPS comparison',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    action=parser.add_mutually_exclusive_group(required=True)
    action.add_argument('--prepare-only',action='store_true')
    action.add_argument('--window-open',action='store_true')
    args=parser.parse_args()
    if args.prepare_only:
        prepare();return
    restore()
    assert common.read(SHARED_AWAKE)['status']=='RELEASED'
    (HERE/'AWAKE-BEFORE.json').write_bytes(SHARED_AWAKE.read_bytes())
    caffeine=subprocess.Popen(['/usr/bin/caffeinate','-disu','-t','2400','-w',str(os.getpid())])
    receipt=dict(status='ACTIVE',campaign='functional',pid=caffeine.pid,startUTC=utc(),
                 maximumSeconds=2400,securitySettingsChanged=False)
    common.save(SHARED_AWAKE,receipt);common.save(HERE/'AWAKE.json',receipt)
    try:
        run()
    finally:
        if caffeine.poll() is None:
            caffeine.terminate();caffeine.wait(timeout=5)
        receipt.update(status='RELEASED',endUTC=utc(),ownCaffeinateAlive=False)
        common.save(SHARED_AWAKE,receipt);common.save(HERE/'AWAKE.json',receipt)
        common.save(HERE/'AWAKE-RELEASED.json',receipt)


if __name__=='__main__':
    main()
