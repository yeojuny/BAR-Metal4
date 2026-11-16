"""One G01-on All That Simmers run using the accepted frozen Direct50 fixture."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import traceback

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
EVIDENCE = HERE.parents[1]
SOURCE = EVIDENCE / 'screen-state-elision-20261115-01/measurement/run_ac0_prelaunch.py'
PACING = EVIDENCE / 'shared-program-v2-20261115-01/measurement/pacing'
ROOT = HERE / 'ordinary_once01'
ARM = 'O1_G01'
spec = importlib.util.spec_from_file_location('frozen_ordinary_runner', SOURCE)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
base.HERE = ROOT
base.PRODUCT = ROOT / 'PRODUCT.json'
base.RELEASE_CONTRACT = ROOT / 'RELEASE-CONTRACT.json'
base.PACING = PACING
base.ORDERS = {'ordinary_paced': [(ARM, 'ELISION1')]}
base.PACED = ('ordinary_paced',)
base.VALIDATION = ()
base.SCREEN = ('ordinary_paced',)
base.CLEAN = ('ordinary_paced',)
base.CHILD_KEYS += ('BAR_NATIVE_MODEL_TYPE_FACTS', 'BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP',
                    'BAR_NATIVE_MATERIAL_IDENTITY_DIAGNOSTICS', 'BAR_NATIVE_MATERIAL_STORAGE_POOL',
                    'BAR_NATIVE_MATERIAL_STORAGE_DIAGNOSTICS', 'BAR_NATIVE_SHARED_PAIR_KEY_VIEW',
                    'BAR_METAL4_SHARED_CUS_PILOT', 'BAR_NATIVE_GRASS_BOUNDARY_S01',
                    'BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE',
                    'BAR_NATIVE_GROUND_DECAL_INDEX_G01',
                    'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE', 'MTL_CAPTURE_ENABLED')


def stage():
    assert not ROOT.exists(), 'Prior ordinary evidence is immutable'
    ROOT.mkdir(parents=True)
    shutil.copyfile(HERE / 'PRODUCT.json', ROOT / 'PRODUCT.json')
    candidate = base.common.read(ROOT / 'PRODUCT.json')
    assert candidate['sha256'] == '962637cd9a0d71bb66896bedb0b807f2940097ae5cb58c2521eebd93bbb98559'
    shutil.copyfile(HERE / 'RELEASE-CONTRACT.json', ROOT / 'RELEASE-CONTRACT.json')
    contract = base.common.read(ROOT / 'RELEASE-CONTRACT.json')
    assert contract['candidateSHA256'] == candidate['sha256']
    assert {'BEGIN', 'PREPARE'} <= set(contract['boundaryMarkers'])
    prepared = base.prepare('ordinary_paced')
    data, out, env, plan = prepared[ARM]
    fixed = {
        'BAR_NATIVE_MODEL_TYPE_FACTS': '1',
        'BAR_NATIVE_MATERIAL_IDENTITY_LOOKUP': '0',
        'BAR_NATIVE_MATERIAL_IDENTITY_DIAGNOSTICS': '0',
        'BAR_NATIVE_MATERIAL_STORAGE_POOL': '0',
        'BAR_NATIVE_MATERIAL_STORAGE_DIAGNOSTICS': '0',
        'BAR_NATIVE_SHARED_PAIR_KEY_VIEW': '0',
        'BAR_METAL4_SHARED_CUS_PILOT': '0',
        'BAR_NATIVE_GRASS_BOUNDARY_S01': '1',
        'BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE': '0',
        'BAR_NATIVE_GROUND_DECAL_INDEX_G01': '1',
        'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE': '0',
        'BAR_NATIVE_FRAME_TRACE': '0',
        'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP': '0',
        'MTL_DEBUG_LAYER': '0', 'MTL_SHADER_VALIDATION': '0',
    }
    env.pop('BAR_METAL4_SEPIA_PATH', None)
    plan['env'].pop('BAR_METAL4_SEPIA_PATH', None)
    env.pop('MTL_CAPTURE_ENABLED', None)
    plan['env'].pop('MTL_CAPTURE_ENABLED', None)
    env.update(fixed)
    plan['env'].update(fixed)
    plan['ordinaryFixture'] = dict(map='All That Simmers v1.1.1', seed=154658555,
        controller=str(base.REF / 'control.lua'), countSimulation=[300, 1500],
        warmupSeconds=10, countedSeconds=40, imageAfterCount=True,
        sourceRunner=str(SOURCE), candidateSHA256=candidate['sha256'])
    base.common.save(out / 'LAUNCH.json', plan)
    base.common.save(ROOT / 'ORDINARY-PLAN.json', dict(status='READY_NO_GAME',
        candidateSHA256=candidate['sha256'], arm=ARM,
        fixedChildFlags=fixed, controllerSHA256=plan['stagedControllerSHA256'],
        pacing=plan['pacing'], map='All That Simmers v1.1.1',
        countSimulation=[300, 1500], independentSingleRun=True))
    print('READY G01 ordinary_once01: All That Simmers, sim300..1500, candidate '+candidate['sha256'], flush=True)


def run():
    assert base.common.read(ROOT / 'ORDINARY-PLAN.json')['status'] == 'READY_NO_GAME'
    prepared = base.restore('ordinary_paced')
    data, out, env, plan = prepared[ARM]
    assert base.common.read(base.SHARED_AWAKE)['status'] == 'RELEASED'
    caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '900', '-w', str(os.getpid())])
    awake = dict(status='ACTIVE', campaign='ordinary_paced', pid=caffeine.pid,
                 startUTC=base.utc(), maximumSeconds=900, securitySettingsChanged=False)
    base.common.save(base.SHARED_AWAKE, awake)
    base.common.save(ROOT / 'AWAKE.json', awake)
    base.active.update(prepared=prepared[ARM], mapped=False, campaign='ordinary_paced')
    stop=threading.Event();issues=[]
    menu=threading.Thread(target=base.reference_runner.witness_menu,args=(out,stop,issues))
    image=threading.Thread(target=base.image['capture'],args=(data,out,stop,issues))
    try:
        menu.start();image.start()
        print('OPEN '+ARM,flush=True)
        try:
            row=base.harness.launch(ROOT/'ordinary_paced/preparation',ARM,prepared[ARM],1)
        finally:
            stop.set();menu.join();image.join()
        cleanup=base.common.read(out/'CLEANUP.json')
        assert cleanup['normalExit'] and cleanup['ownedGroupGone'] and base.active['mapped']
        info=(out/'infolog.txt').read_text()
        power=re.findall(r'\[FPSPowerStatus\] (.+)',info)
        assert len(power)==1 and all(s in power[0] for s in
            ('frame=480','LimitIdleFps=0','VSync=0','VSyncGame=0','option_limitoffscreenfps=false'))
        assert re.search(r'\[NativeMRT164\].*encoded=1',info)
        assert re.search(r'\[NativePass165\].*retired=1',info)
        assert re.search(r'\[NativePass166\].*retired=1',info)
        assert re.search(r'\[NativeFinalColorStage\] active=1 samples=1',info)
        assert (out/'scene.png').is_file() and (out/'scene.png').stat().st_size>1000
        pace=base.pacing_evidence(out,plan,info)
        release=base.release_evidence(out,plan,info,'PREPARE')
        screen=base.screen_state_evidence(out,info,True,False)
        row.update(performanceAccepted=True,ordinaryFixture=plan['ordinaryFixture'],
                   powerWitness=power[0],pacingWitness=pace,releaseWitness=release,
                   screenStateWitness=screen,image=str(out/'scene.png'),
                   candidateSHA256=base.pinned_product()['sha256'])
        base.common.save(out/'RESULT.json',row)
        base.common.save(ROOT/'RESULT.json',dict(status='COMPLETE_ONE_ORDINARY_RUN',
            meanFPS=row['FPS'],slow1PercentFPS=row['frameStats']['slow1PercentFPS'],
            countedSeconds=row['seconds'],draws=row['draws'],normalExit=True,
            image=str(out/'scene.png'),candidateSHA256=row['candidateSHA256'],
            historicalComparisonsPooled=False))
        print(f"DONE {ARM}: {row['FPS']:.3f} FPS; 1% low {row['frameStats']['slow1PercentFPS']:.3f}",flush=True)
    except BaseException:
        base.common.save(out/'FAILURE.json',dict(error=traceback.format_exc(),utc=base.utc()))
        raise
    finally:
        stop.set()
        if menu.ident and menu.is_alive():menu.join()
        if image.ident and image.is_alive():image.join()
        try:base.cleanup('ordinary_paced',prepared)
        finally:
            if caffeine.poll() is None:
                caffeine.terminate();caffeine.wait(timeout=5)
            awake.update(status='RELEASED',endUTC=base.utc(),ownCaffeinateAlive=False)
            base.common.save(base.SHARED_AWAKE,awake)
            base.common.save(ROOT/'AWAKE-RELEASED.json',awake)
            base.common.save(ROOT/'OWNER-CLOSED.json',dict(status='CLOSED',awakeReleased=True,utc=base.utc()))


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--prepare-only',action='store_true')
    group.add_argument('--run',action='store_true')
    args=parser.parse_args()
    stage() if args.prepare_only else run()
