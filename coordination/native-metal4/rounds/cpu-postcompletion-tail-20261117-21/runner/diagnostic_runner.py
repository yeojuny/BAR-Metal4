"""One frozen CPU-tail diagnostic. Default/check-only never creates a process.

Execution is forbidden in the preparation turn. A later designated HQ must first
read JINI's formal REQUEST/READY and latest STATE, verify exact round/attempt,
source, owner and budget, and close every other local measurement participant.
The explicit execution flag is NOT evidence of that authority or freshness.

Uses the historical launch argv/cwd, 210-second/32-MiB bound, owned process-group
cleanup, finite caffeinate and exercised power/display preflight. The frozen Lua
controller owns all warmup, pacing, camera, combat and normal shutdown behavior.
No validation/G01 positive-probe requirement, GPU helper, performance parser,
capture/replay, framework, service, daemon or automatic retry is installed.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
E = HERE.parent
OUT = E / 'staging/runs/D_CPU_POSTCOMPLETION_TAIL'
LAUNCH = OUT / 'LAUNCH.json'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def save(path, value, exclusive=False):
    with Path(path).open('x' if exclusive else 'w') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n')


def check_only():
    pins = json.loads((E / 'RUNNER-INPUTS.json').read_text())
    assert all(sha(p) == h for p, h in pins['files'].items()), 'Frozen runner/input drift'
    plan = json.loads(LAUNCH.read_text())
    assert sha(plan['product']['binary']) == plan['product']['sha256']
    assert all(sha(p) == h for p, h in plan['checksums'].items()), 'Profile drift'
    for item in json.loads((E / 'RUNTIME-REVERIFICATION.json').read_text())['libraries']:
        assert sha(item['path']) == item['expected'], 'Runtime library drift'
    assets = Path(plan['env']['BAR_ASSETS_PATH'])
    assert all(sha(assets / p) == h for p, h in json.loads((E / 'CONTENT-HASHES-RECOMPUTED.json').read_text()).items())
    env = plan['env']
    assert env == json.loads((OUT / 'ENV.json').read_text())
    required = {'BAR_NATIVE_FRAME_TRACE':'1', 'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP':'1',
                'BAR_NATIVE_FRAME_TRACE_START_SIM':'900', 'BAR_NATIVE_FRAME_TRACE_END_SIM':'915',
                'MTL_DEBUG_LAYER':'0', 'MTL_SHADER_VALIDATION':'0', 'MTL_CAPTURE_ENABLED':'0',
                'BAR_NATIVE_PREPARE_READY_PILOT':'1', 'BAR_NATIVE_BEGIN_READY_PILOT':'0',
                'BAR_NATIVE_INTERVENTION_RENDER_WORKER':'1', 'BAR_NATIVE_FRAME_OVERLAP':'1',
                'BAR_NATIVE_CAPTURE_OVERLAP':'1', 'BAR_NATIVE_PREPARE_OVERLAP':'1',
                'BAR_NATIVE_CAPTURE_READY_BOUNDARY':'1', 'BAR_NATIVE_BEGIN_PREPARE_OVERLAP':'1',
                'BAR_NATIVE_GRASS_BOUNDARY_S01':'1', 'BAR_NATIVE_GROUND_DECAL_INDEX_G01':'1',
                'BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE':'0', 'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE':'0'}
    assert all(env.get(k) == v for k, v in required.items()), 'Diagnostic child env drift'
    assert not any(k in env for k in plan['unset_environment'])
    control_review = json.loads((E / 'DIAGNOSTIC-CONTROLS.json').read_text())
    controls = control_review['controls']
    assert all(env[k] in ('0', '') for k in controls if not k.startswith('BAR_NATIVE_FRAME_TRACE'))
    assert all(env[k] == '' for k in control_review['disabled_path_controls'])
    assert not plan['performanceAccepted'] and plan['environment_mode'] == 'EXACT_ENV_NO_HOST_ENV_INHERITANCE'
    assert plan['argv'][0] == plan['product']['binary']
    assert not (OUT / 'ATTEMPT-CLAIM.json').exists(), 'Attempt already consumed; no retry'
    return plan


def run_once(plan):
    # This local import has only standard-library imports and function definitions.
    # Its display/power commands are called only in this separately authorized branch.
    from preflight import preflight
    data = Path(plan['argv'][plan['argv'].index('--write-dir') + 1])
    save(OUT / 'ATTEMPT-CLAIM.json', {'round_id':plan['round_id'], 'attempt_id':plan['attempt_id'],
        'utc':utc(), 'launch_sha256':sha(LAUNCH), 'retries':0}, exclusive=True)
    caffeine = child = None
    normal = False
    awake = {'status':'ACTIVE', 'startUTC':utc(), 'maximumSeconds':2400, 'securitySettingsChanged':False}
    try:
        caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '2400', '-w', str(os.getpid())])
        awake['pid'] = caffeine.pid
        save(HERE / 'AWAKE.json', awake)
        state = preflight(1)
        assert state['display']['valid'] and not state['display']['locked']
        save(OUT / 'PREFLIGHT.json', state)
        # Exactly the frozen dict; no os.environ merge, wrapper, staging or restaging.
        with (OUT / 'engine.log').open('xb') as stream:
            child = subprocess.Popen(plan['argv'], cwd=plan['cwd'], env=plan['env'],
                                     stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
            save(OUT / 'OWNED.json', {'pid':child.pid, 'pgid':child.pid, 'utc':utc(),
                'binary':plan['product']['binary'], 'games_started':1, 'engine_launches':1})
            # An OS snapshot is future evidence only; it does not exist during staging.
            observed = subprocess.check_output(['/bin/ps', 'eww', '-p', str(child.pid), '-o', 'command='], text=True)
            (OUT / 'CHILD-ENV-OBSERVED.txt').write_text(observed)
            for key in ['BAR_NATIVE_FRAME_TRACE', 'BAR_NATIVE_FRAME_TRACE_EVERY_LOOP',
                        'BAR_NATIVE_FRAME_TRACE_START_SIM', 'BAR_NATIVE_FRAME_TRACE_END_SIM',
                        'MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION', 'BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE']:
                assert key + '=' + plan['env'][key] in observed.split(), 'Actual child env mismatch'
            started = next_check = time.monotonic()
            while child.poll() is None:
                now = time.monotonic()
                assert now - started <= plan['wall_limit_seconds'], 'Owned engine wall bound exceeded'
                assert (OUT / 'engine.log').stat().st_size <= plan['log_limit_bytes'], 'Owned engine log bound exceeded'
                if now >= next_check:
                    state = preflight(1)
                    assert state['display']['valid'] and not state['display']['locked']
                    next_check = now + 5
                time.sleep(0.5)
        assert child.returncode == 0, ('Owned engine exit', child.returncode)
        normal = True
    finally:
        failure = repr(sys.exc_info()[1]) if sys.exc_info()[1] is not None else None
        if child is not None and child.poll() is None:
            os.killpg(child.pid, signal.SIGTERM)
            try:
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
        if caffeine is not None and caffeine.poll() is None:
            caffeine.terminate()
            caffeine.wait(timeout=5)
        group_gone = child is None
        if child is not None:
            try:
                os.killpg(child.pid, 0)
            except ProcessLookupError:
                group_gone = True
        awake.update(status='RELEASED', endUTC=utc(), ownCaffeinateAlive=False)
        save(HERE / 'AWAKE.json', awake)
        for rel, name in [('infolog.txt','infolog.txt'), ('script.txt','actual-start-script.txt'),
                          ('LuaUI/Config/TERRAIN211-FRAMES.csv','TERRAIN211-FRAMES.csv'),
                          ('LuaUI/Config/h1-scene.png','scene.png')]:
            if (data / rel).is_file():
                shutil.copyfile(data / rel, OUT / name)
        # Preserve the private profile and all raw output; it cannot be run again.
        save(OUT / 'CLEANUP.json', {'normalExit':normal, 'pid':child.pid if child else None,
            'ownedProcessExited':child is None or child.poll() is not None,
            'ownedGroupGone':group_gone, 'failure':failure,
            'ownCaffeinateReleased':caffeine is None or caffeine.poll() is not None,
            'originalEvidenceModified':False, 'performanceAccepted':False, 'utc':utc()})
        assert group_gone, 'Owned process group remains; preserve output and stop'
    save(OUT / 'EXECUTION-RECEIPT.json', {'status':'RAW_DIAGNOSTIC_REQUIRES_COMPLETENESS_REVIEW',
        'games':1, 'builds':0, 'captures':0, 'replays':0, 'helpers':0, 'retries':0,
        'performanceAccepted':False, 'sceneAndTraceValidity':'NOT_YET_ASSESSED'})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument('--check-only', action='store_true')
    action.add_argument('--execute-after-verified-handoff', action='store_true')
    args = parser.parse_args()
    plan = check_only()
    if args.execute_after_verified_handoff:
        run_once(plan)
    else:
        print(json.dumps({'status':'PLANNED_CHILD_ENV_VERIFIED_NO_ENGINE_EXECUTION',
            'launch_sha256':sha(LAUNCH), 'env_sha256':sha(OUT / 'ENV.json'),
            'engine_launches':0, 'actual_child_observed':False, 'performanceAccepted':False}, indent=2))


if __name__ == '__main__':
    main()
