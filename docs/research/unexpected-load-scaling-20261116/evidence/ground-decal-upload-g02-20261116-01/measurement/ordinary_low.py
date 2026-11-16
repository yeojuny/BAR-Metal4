"""One G02-on All That Simmers owner-summary diagnostic; FPS excluded."""
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
ROOT = HERE / 'low_diagnostic01'
ARM = 'O1_G02'
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
                    'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE',
                    'BAR_NATIVE_GROUND_DECAL_UPLOAD_G02', 'MTL_CAPTURE_ENABLED')


def stage():
    assert not ROOT.exists(), 'Prior ordinary evidence is immutable'
    ROOT.mkdir(parents=True)
    shutil.copyfile(HERE / 'PRODUCT.json', ROOT / 'PRODUCT.json')
    candidate = base.common.read(ROOT / 'PRODUCT.json')
    assert candidate['sha256'] == base.common.read(HERE.parent / 'CANDIDATE.json')['signedSHA256']
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
        'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE': '1',
        'BAR_NATIVE_GROUND_DECAL_UPLOAD_G02': '1',
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
    print('READY G02 low_diagnostic01: All That Simmers, sim300..1500, candidate '+candidate['sha256'], flush=True)


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
        row, launch_error = None, None
        try:
            try:
                row=base.harness.launch(ROOT/'ordinary_paced/preparation',ARM,prepared[ARM],1)
            except BaseException:
                launch_error=traceback.format_exc()
        finally:
            stop.set();menu.join();image.join()
        cleanup=base.common.read(out/'CLEANUP.json') if (out/'CLEANUP.json').exists() else {}
        info=(out/'infolog.txt').read_text() if (out/'infolog.txt').exists() else ''
        engine=(out/'engine.log').read_text() if (out/'engine.log').exists() else ''
        if launch_error:
            base.common.save(out/'POSTRUN-ERROR.json',dict(error=launch_error,
                interpretation='Preserved parser/controller exception; evaluate actual normal exit and owner summary independently'))
        pattern=(r'\[NativeGroundDecalUploadG02Summary\] owner_created=1 prepare=(\d+) '
                 r'nonempty=(\d+) eligible=(\d+) packed=(\d+) separate=(\d+) '
                 r'nonindexed=(\d+) buffer_creates=(\d+) uniform_buffer_creates=(\d+) '
                 r'index_buffer_creates=(\d+) combined_buffer_creates=(\d+) '
                 r'clear_resources=(\d+) scope=owner_lifetime_prepared_routes')
        summary_rows=[list(map(int,match)) for match in re.findall(pattern,engine)]
        keys=('prepare','nonempty','eligible','packed','separate','nonindexed',
              'buffer_creates','uniform_buffer_creates','index_buffer_creates',
              'combined_buffer_creates','clear_resources')
        totals={key:sum(values[i] for values in summary_rows) for i,key in enumerate(keys)}
        classification=('MISSING_SUMMARY' if not summary_rows else
                        'PREPARE_ZERO' if totals['prepare']==0 else
                        'NONEMPTY_ZERO' if totals['nonempty']==0 else
                        'ELIGIBLE_ZERO' if totals['eligible']==0 else
                        'ELIGIBLE_PACKED_OR_FALLBACK')
        base.common.save(out/'OWNER-SUMMARY.json',dict(status=classification,
            ownerCreatedRows=len(summary_rows), ownerLifetimeRows=summary_rows,
            totals=totals, source='raw engine.log; physical allocations, not encoded draws',
            missingIsFailureNotZero=not bool(summary_rows)))
        assert cleanup.get('normalExit') and cleanup.get('ownedGroupGone') and base.active['mapped']
        assert summary_rows, 'G02 owner terminal summary missing; preserve raw and stop without retry'
        power=re.findall(r'\[FPSPowerStatus\] (.+)',info)
        assert len(power)==1 and all(s in power[0] for s in
            ('frame=480','LimitIdleFps=0','VSync=0','VSyncGame=0','option_limitoffscreenfps=false'))
        assert re.search(r'\[NativeMRT164\].*encoded=1',info)
        assert re.search(r'\[NativePass165\].*retired=1',info)
        assert re.search(r'\[NativePass166\].*retired=1',info)
        assert re.search(r'\[NativeFinalColorStage\] active=1 samples=1',info)
        assert (out/'scene.png').is_file() and (out/'scene.png').stat().st_size>1000
        child=base.common.read(out/'CHILD-MODE.json')
        assert child['status']=='PASS_ACTUAL_CHILD_ENVIRONMENT'
        assert child['actual']['BAR_NATIVE_GROUND_DECAL_UPLOAD_G02']=='1'
        assert child['actual']['BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE']=='1'
        try:
            pace=base.pacing_evidence(out,plan,info)
        except BaseException:
            pace=dict(status='POSTPROCESS_REVIEW_REQUIRED',error=traceback.format_exc())
            base.common.save(out/'PACING-POSTPROCESS-ERROR.json',pace)
        release=base.release_evidence(out,plan,info,'PREPARE')
        screen=base.screen_state_evidence(out,info,True,False)
        receipt=dict(status='COMPLETE_LOW_OWNER_DIAGNOSTIC',
            performanceAccepted=False, diagnosticFPS='EXCLUDED',
            fpsParserStatus='PASS_BUT_EXCLUDED' if row else 'POSTPROCESS_ERROR_EXCLUDED',
            ordinaryFixture=plan['ordinaryFixture'], powerWitness=power[0],
            pacingWitness=pace, releaseWitness=release, screenStateWitness=screen,
            ownerSummaryClassification=classification, ownerSummaryTotals=totals,
            ownerSummaryRows=len(summary_rows), image=str(out/'scene.png'),
            candidateSHA256=base.pinned_product()['sha256'], normalExit=True,
            oldG01OrdinaryFPSNotPooled=True)
        base.common.save(out/'RESULT.json',receipt)
        base.common.save(ROOT/'RESULT.json',receipt)
        print('DONE '+ARM+': owner summary '+classification+'; diagnostic FPS excluded',flush=True)
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
