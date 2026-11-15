"""One owned BC213 Time Profiler capture of the signed Q native engine."""

import datetime
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
EVIDENCE = HERE.parents[2]
Q = EVIDENCE / 'shared-program-v2-20261115-01'
P_RUNNER = EVIDENCE / 'screen-state-elision-20261115-01/measurement/run_ac0_prelaunch.py'
CAMPAIGN = 'cus_terrain_profile_p2'
ARM = 'P2_Q_CUS0'

spec = importlib.util.spec_from_file_location('profile_base', P_RUNNER)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
base.HERE = HERE
base.PRODUCT = Q / 'measurement/PRODUCT.json'
base.RELEASE_CONTRACT = Q / 'measurement/RELEASE-CONTRACT.json'
base.PACING = Q / 'measurement/pacing'
base.ORDERS[CAMPAIGN] = [(ARM, 'ELISION1')]
base.PACED = (CAMPAIGN,)
base.VALIDATION = ()
base.SCREEN = ()
base.CLEAN = ()
base.CHILD_KEYS += ('BAR_METAL4_SHARED_CUS_PILOT', 'BAR_METAL4_SEPIA_PATH')


def save(path, value):
    base.common.save(path, value)


def stage():
    prepared = (base.restore(CAMPAIGN) if (HERE / CAMPAIGN / 'PREPARED.json').exists()
                else base.prepare(CAMPAIGN))
    data, out, env, plan = prepared[ARM]
    widget_config = (data / 'LuaUI/Config/BC213.lua').read_text()
    assert 'Sepia Tone' not in widget_config, 'Sepia widget must stay out of the private enabled order'
    assert not (data / 'LuaUI/Widgets/zzz_v2_functional_probe.lua').exists(), 'Sepia functional fixture unexpectedly staged'
    assert re.search(r'\["Limit idle FPS"\]\s*=\s*0\b', widget_config)
    controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
    old = controller.read_text()
    assert old.count('local externalProfiles=false') == 1
    assert old.count('f==600 then budgetMark("time-profiler")') == 1
    assert "FAILED_WARMUP_BEFORE_SIM300" in old
    controller.write_text(old.replace('local externalProfiles=false', 'local externalProfiles=true', 1))
    plan['checksums'][str(controller)] = base.common.sha(controller)
    env['BAR_METAL4_SHARED_CUS_PILOT'] = plan['env']['BAR_METAL4_SHARED_CUS_PILOT'] = '0'
    env.pop('BAR_METAL4_SEPIA_PATH', None)
    plan['env'].pop('BAR_METAL4_SEPIA_PATH', None)
    plan['stagedControllerSHA256'] = base.common.sha(controller)
    plan['profileCapture'] = dict(template='Time Profiler', triggerSimulation=600,
                                  requestedSeconds=15, target='owned native engine PID',
                                  diagnosticOnly=True, metalSystemTrace=False)
    plan['sepiaWidget'] = 'OFF by private BC213 config'
    save(out / 'LAUNCH.json', plan)
    save(HERE / CAMPAIGN / 'PROFILE-PLAN.json', dict(
        status='READY_NO_GAME', arm=ARM, product=base.pinned_product(),
        controllerSHA256=base.common.sha(controller), originalControllerSHA256=base.common.sha(base.REF/'control.lua'),
        controllerChange='externalProfiles=false to true only; marker at simulation 600',
        sepiaWidget='OFF', cusPilot='0', power='AC powermode 0 per existing preflight',
        runtimeLimiter='LimitIdleFps=0 and option_limitoffscreenfps=false required',
        flags={k: env.get(k) for k in base.CHILD_KEYS},
        nativeWindow='BC213 simulation 300..1500; 40 counted seconds; common pacing',
        capture='xcrun xctrace Time Profiler attach owned PID for 15s after sim600 marker',
        performanceAccepted=False))
    print('READY: one private Q profile; no game launched', flush=True)


def capture(data, out, stop, receipt):
    marker = data / 'LuaUI/Config/budget192.time-profiler'
    while not marker.exists():
        if stop.wait(0.10):
            receipt.update(status='MARKER_NOT_REACHED', triggerSimulation=600)
            return
    receipt['markerMTimeNS'] = marker.stat().st_mtime_ns
    owned = base.common.read(out / 'OWNED.json')
    pid = int(owned['pid'])
    trace = out / 'time-profiler.trace'
    command = ['/usr/bin/xcrun', 'xctrace', 'record', '--template', 'Time Profiler',
               '--attach', str(pid), '--time-limit', '15s', '--output', str(trace), '--no-prompt']
    receipt.update(command=command, ownedPID=pid, startAfterMarkerSeconds=(time.time_ns()-receipt['markerMTimeNS'])/1e9)
    with (out / 'time-profiler.log').open('xb') as stream:
        child = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        try:
            status = child.wait(timeout=180)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGTERM)
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
            status = 'timeout'
    receipt.update(status='CAPTURED' if status == 0 and trace.is_dir() else 'CAPTURE_FAILED',
                   xctraceExit=status, tracePresent=trace.is_dir(), trace=str(trace))


def run():
    prepared = base.restore(CAMPAIGN)
    data, out, env, plan = prepared[ARM]
    assert base.common.read(HERE / CAMPAIGN / 'PROFILE-PLAN.json')['status'] == 'READY_NO_GAME'
    assert base.common.read(base.SHARED_AWAKE)['status'] == 'RELEASED'
    (HERE / 'AWAKE-BEFORE.json').write_bytes(base.SHARED_AWAKE.read_bytes())
    caffeine = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '2400', '-w', str(os.getpid())])
    awake = dict(status='ACTIVE', campaign=CAMPAIGN, pid=caffeine.pid,
                 startUTC=base.utc(), maximumSeconds=2400, securitySettingsChanged=False)
    save(base.SHARED_AWAKE, awake)
    save(HERE / 'AWAKE.json', awake)
    stop = threading.Event()
    capture_receipt = {}
    base.active.update(prepared=prepared[ARM], mapped=False, campaign=CAMPAIGN)
    def capture_safe():
        try:
            capture(data, out, stop, capture_receipt)
        except BaseException:
            capture_receipt.update(status='CAPTURE_EXCEPTION', error=traceback.format_exc())
    worker = threading.Thread(target=capture_safe)
    try:
        worker.start()
        print('OPEN: ' + ARM, flush=True)
        try:
            row = base.harness.launch(HERE / CAMPAIGN / 'preparation', ARM, prepared[ARM], 1)
        finally:
            stop.set()
            worker.join()
            save(out / 'CAPTURES.json', capture_receipt)
        assert base.active['mapped'], 'Actual child environment receipt absent'
        info = (out / 'infolog.txt').read_text()
        power = re.findall(r'\[FPSPowerStatus\] (.+)', info)
        assert len(power) == 1 and all(s in power[0] for s in
            ('frame=480', 'shortName=BC213', 'LimitIdleFps=0', 'option_limitoffscreenfps=false',
             'VSync=0', 'VSyncGame=0')), power
        assert row['draws'] > 0 and re.search(r'\[NativeMRT164\].*encoded=1', info)
        pace = base.pacing_evidence(out, plan, info)
        release = base.release_evidence(out, plan, info, 'PREPARE')
        screen = base.screen_state_evidence(out, info, True, False)
        row.update(profileCapture=capture_receipt, powerWitness=power[0],
                   pacingWitness=pace, releaseWitness=release, screenWitness=screen,
                   performanceAccepted=False, diagnosticFPSOnly=True, sepiaWidget='OFF', cusPilot='0')
        save(out / 'RESULT.json', row)
        save(HERE / CAMPAIGN / 'COMPLETION.json', dict(status='CAPTURED' if capture_receipt.get('status') == 'CAPTURED' else 'DIAGNOSTIC_CAPTURE_PARTIAL',
            arm=ARM, trace=capture_receipt.get('trace'), performanceAccepted=False, utc=base.utc()))
        print('DONE: capture=' + str(capture_receipt.get('status')) + ' engine normal', flush=True)
    except BaseException:
        save(out / 'FAILURE.json', dict(error=traceback.format_exc(), capture=capture_receipt, utc=base.utc()))
        save(HERE / CAMPAIGN / 'COMPLETION.json', dict(status='STOPPED_ON_FAILURE', arm=ARM, utc=base.utc()))
        raise
    finally:
        stop.set()
        if worker.is_alive():
            worker.join()
        if not (out / 'CAPTURES.json').exists():
            save(out / 'CAPTURES.json', capture_receipt)
        try:
            base.cleanup(CAMPAIGN, prepared)
        finally:
            if caffeine.poll() is None:
                caffeine.terminate()
                caffeine.wait(timeout=5)
            awake.update(status='RELEASED', endUTC=base.utc(), ownCaffeinateAlive=False)
            save(base.SHARED_AWAKE, awake)
            save(HERE / 'AWAKE.json', awake)
            save(HERE / 'AWAKE-RELEASED.json', awake)
            save(HERE / 'OWNER-CLOSED.json', dict(status='CLOSED', utc=base.utc(),
                captureStatus=capture_receipt.get('status'), awakeReleased=True,
                ownedEngineAlive=False, xctraceAlive=False))


if __name__ == '__main__':
    assert len(sys.argv) == 2 and sys.argv[1] in ('--prepare-only', '--window-open')
    stage() if sys.argv[1] == '--prepare-only' else run()
