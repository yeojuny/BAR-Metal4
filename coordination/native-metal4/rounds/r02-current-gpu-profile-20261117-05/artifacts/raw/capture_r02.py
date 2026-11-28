"""One signed R02 Ultra AA0 GPU frame capture; explicit --stage/--run only."""
import importlib.util
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import threading
import time
import traceback

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
SOURCE = HERE.parent / 'run.py'
spec = importlib.util.spec_from_file_location('ultra_capture_source', SOURCE)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
base.RUN_ROOT = HERE
base.ORDER['clean_paced'] = [('P_R02_CAPTURE', 'candidate')]
base.base.PACED = ('clean_paced',)
base.base.CHILD_KEYS += ('MTL_CAPTURE_ENABLED',)


def write(path, value):
    base.save(path, value)


def stage():
    prepared_path = HERE / 'clean_paced/PREPARED.json'
    if not prepared_path.exists():
        base.stage('clean_paced', 0)
    else:
        assert not (HERE / 'PLAN.json').exists(), 'Already staged; preserve it'
        # The first static staging attempt stopped before changing any staged input.
    summary = base.common.read(prepared_path)
    item = summary['order'][0]
    assert item['arm'] == 'P_R02_CAPTURE'
    assert item['binarySHA256'] == '21b2e9df8f331b9c4f8bb04f5abaad44978e39e97d9171c5380a6df38658e5da'
    data, out = Path(item['data']), Path(item['out'])
    controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
    body = controller.read_text()
    needle = ' if f==1740 then\n  endMeasurement(f)'
    assert body.count(needle) == 1
    body = body.replace(needle, ''' if f==1500 then budgetMark("capture-boundary") end
 if f==1590 then budgetMark("capture-start") end
''' + needle, 1)
    controller.write_text(body)
    digest = base.sha(controller)
    plan = base.common.read(out / 'LAUNCH.json')
    plan['checksums'][str(controller)] = digest
    plan['stagedControllerSHA256'] = digest
    plan['env']['MTL_CAPTURE_ENABLED'] = '1'
    plan['env']['BAR_NATIVE_GRASS_BOUNDARY_S01'] = '1'
    plan['env']['BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE'] = '1'
    plan['env']['MTL_DEBUG_LAYER'] = '0'
    plan['env']['MTL_SHADER_VALIDATION'] = '0'
    plan['env'].pop('MTL_LOG_TO_STDERR', None)
    plan['gpuCapture'] = dict(kind='one native Layer boundary', readySim=1500,
                              requestSim=1590, count=1, output=str(out / 'native-frame.gputrace'),
                              tool='/usr/bin/gpucapture', timeoutSeconds=180,
                              diagnosticFPSOnly=True, parallelMST=False)
    write(out / 'LAUNCH.json', plan)
    item['controllerSHA256'] = digest
    write(prepared_path, summary)
    write(HERE / 'PLAN.json', dict(status='READY_NO_GAME', binarySHA256=item['binarySHA256'],
        arm=item['arm'], campaign='clean_paced', controllerSHA256=digest,
        capture=plan['gpuCapture'], runtimeGate='actual Layer boundary required; no Device/Queue fallback',
        childEnv={key:plan['env'][key] for key in ('MTL_CAPTURE_ENABLED',
            'BAR_NATIVE_GRASS_BOUNDARY_S01', 'BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE',
            'MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION')},
        priorValidationImage=str((HERE.parent / 'runs/r02_validation01/validation_paced/preparation/runs/V_R02/scene.png').resolve()),
        previousCleanPoolsUnchanged=True))
    print('READY one native capture; games 0', flush=True)


def layer_id(output):
    rows = [line for line in output.splitlines() if re.search(r'\bLayer\b', line, re.I)]
    found = []
    for line in rows:
        match = re.match(r'^\s*(\d+)\b', line)
        if not match:
            match = re.search(r'\b(?:id|boundary)\s*[:=#]\s*(\d+)\b', line, re.I)
        if match:
            found.append(int(match.group(1)))
    return found[0] if len(found) == 1 else None


def capture(data, out, stop, receipt):
    ready = data / 'LuaUI/Config/budget192.capture-boundary'
    begin = data / 'LuaUI/Config/budget192.capture-start'
    while not ready.exists():
        if stop.wait(0.1):
            receipt['status'] = 'READY_MARKER_NOT_REACHED'
            return
    pid = int(base.common.read(out / 'OWNED.json')['pid'])
    listing = subprocess.run(['/usr/bin/gpucapture', 'boundaries', '--pid', str(pid)],
                             text=True, capture_output=True, timeout=8)
    receipt.update(pid=pid, boundaryExit=listing.returncode,
                   boundaryStdout=listing.stdout, boundaryStderr=listing.stderr)
    boundary = layer_id(listing.stdout)
    if listing.returncode != 0 or boundary is None:
        receipt['status'] = 'NO_UNIQUE_LAYER_BOUNDARY'
        return
    receipt['layerBoundaryID'] = boundary
    while not begin.exists():
        if stop.wait(0.1):
            receipt['status'] = 'START_MARKER_NOT_REACHED'
            return
    target = out / 'native-frame.gputrace'
    assert not target.exists()
    cmd = ['/usr/bin/gpucapture', 'start', '--pid', str(pid), '--boundary', str(boundary),
           '--count', '1', '--output', str(target)]
    receipt.update(command=cmd, startMarkerMTimeNS=begin.stat().st_mtime_ns,
                   startDelaySeconds=(time.time_ns()-begin.stat().st_mtime_ns)/1e9)
    with (out / 'gpucapture.log').open('xb') as stream:
        child = subprocess.Popen(cmd, stdout=stream, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        receipt['capturePID'] = child.pid
        try:
            exit_code = child.wait(timeout=180)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGTERM)
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
            exit_code = 'TIMEOUT_180S'
    receipt.update(captureExit=exit_code, tracePresent=target.exists(),
                   trace=str(target), status='CAPTURED' if exit_code == 0 and target.exists() else 'CAPTURE_FAILED')


def run():
    assert base.common.read(HERE / 'PLAN.json')['status'] == 'READY_NO_GAME'
    prepared = base.restore('clean_paced')
    arm = 'P_R02_CAPTURE'
    data, out, env, plan = prepared[arm]
    assert env['MTL_CAPTURE_ENABLED'] == '1' and plan['env']['MTL_CAPTURE_ENABLED'] == '1'
    assert env['BAR_NATIVE_GRASS_BOUNDARY_S01'] == env['BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE'] == '1'
    assert env['MTL_DEBUG_LAYER'] == env['MTL_SHADER_VALIDATION'] == '0'
    assert base.common.read(base.base.SHARED_AWAKE)['status'] == 'RELEASED'
    sessions = subprocess.run(['/usr/bin/gpudebug', '--list-sessions'], capture_output=True,
                              text=True, timeout=15)
    write(HERE / 'GPU-SESSIONS-BEFORE.json', dict(exit=sessions.returncode,
        stdout=sessions.stdout, stderr=sessions.stderr))
    assert (sessions.returncode == 0 and 'No active sessions' in sessions.stdout), \
        'Existing GPU debugger session; do not start a game'
    caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '2400', '-w', str(os.getpid())])
    awake = dict(status='ACTIVE', campaign='clean_paced', pid=caffeine.pid,
                 startUTC=base.base.utc(), maximumSeconds=2400, securitySettingsChanged=False)
    write(base.base.SHARED_AWAKE, awake)
    write(HERE / 'AWAKE.json', awake)
    base.base.active.update(prepared=prepared[arm], mapped=False, campaign='clean_paced')
    stop = threading.Event()
    receipt = {}
    worker = threading.Thread(target=lambda: guarded_capture(data, out, stop, receipt), name='gpu-capture')
    worker.start()
    row = None
    launch_error = None
    try:
        print('OPEN one native diagnostic game', flush=True)
        try:
            try:
                row = base.base.harness.launch(HERE / 'clean_paced/preparation', arm, prepared[arm], 1)
            except BaseException:
                launch_error = traceback.format_exc()
        finally:
            stop.set()
            worker.join()
            write(out / 'GPU-CAPTURE.json', receipt)
        cleanup = base.common.read(out / 'CLEANUP.json') if (out / 'CLEANUP.json').exists() else {}
        if launch_error:
            write(out / 'POSTRUN-ERROR.json', dict(error=launch_error,
                 interpretation='Diagnostic FPS/parser excluded; actual engine exit and capture evaluated independently'))
        assert cleanup.get('normalExit'), 'Engine did not exit normally'
        assert receipt.get('status') == 'CAPTURED', receipt.get('status')
        child = base.common.read(out / 'CHILD-MODE.json')
        assert child['status'] == 'PASS_ACTUAL_CHILD_ENVIRONMENT'
        assert child['actual']['BAR_NATIVE_GRASS_BOUNDARY_S01'] == '1'
        assert child['actual']['BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE'] == '1'
        write(HERE / 'RESULT.json', dict(status='CAPTURED', arm=arm,
            capture=receipt['trace'], engineNormalExit=True, diagnosticFPS=row.get('FPS') if row else None,
            diagnosticParserError=bool(launch_error), performanceAccepted=False,
            actualChildS01Flag='1', actualChildProbeFlag='1', previousCleanPoolsUnchanged=True))
    except BaseException:
        write(HERE / 'FAILURE.json', dict(error=traceback.format_exc(), capture=receipt,
                                         performanceAccepted=False))
        raise
    finally:
        try:
            base.base.cleanup('clean_paced', prepared)
        finally:
            if caffeine.poll() is None:
                caffeine.terminate()
                caffeine.wait(timeout=5)
            awake.update(status='RELEASED', endUTC=base.base.utc(), ownCaffeinateAlive=False)
            write(base.base.SHARED_AWAKE, awake)
            write(HERE / 'AWAKE-RELEASED.json', awake)
            write(HERE / 'OWNER-CLOSED.json', dict(status='CLOSED', awakeReleased=True,
                engineNormalExit=(base.common.read(out / 'CLEANUP.json').get('normalExit')
                    if (out / 'CLEANUP.json').exists() else False),
                captureStatus=receipt.get('status'), utc=base.base.utc()))
            print('CLOSED ' + str(receipt.get('status')), flush=True)


def guarded_capture(data, out, stop, receipt):
    try:
        capture(data, out, stop, receipt)
    except BaseException:
        receipt.update(status='CAPTURE_EXCEPTION', error=traceback.format_exc())


if __name__ == '__main__':
    assert len(sys.argv) == 2 and sys.argv[1] in ('--stage', '--run')
    stage() if sys.argv[1] == '--stage' else run()
