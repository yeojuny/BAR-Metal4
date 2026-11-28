"""Open the one owned R02 trace, profile once, export summaries, terminate it."""

import argparse
import datetime
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import time

HERE = Path(__file__).resolve().parent
ARM = HERE / 'clean_paced/preparation/runs/P_R02_CAPTURE'
TRACE = ARM / 'native-frame.gputrace'
ANALYSIS = HERE / 'analysis'
GPUDEBUG = '/usr/bin/gpudebug'
EXPECTED_SHA = '21b2e9df8f331b9c4f8bb04f5abaad44978e39e97d9171c5380a6df38658e5da'
EXPORTS = {
    'command-tree': '/commands',
    'command-buffer': '/commands/cb0',
    'api-calls': '/api_calls',
    'resources': '/resources',
    'render-pipelines': '/resources/render_pipelines',
    'encoder-costs': '/performance/encoders',
    'shader-costs': '/performance/shaders',
    'encoder-intervals': '/performance/timeline/encoders',
    'shader-stage-intervals': '/performance/timeline/shaders',
}


def read(path):
    return json.loads(Path(path).read_text())


def save(path, data):
    Path(path).write_text(json.dumps(data, indent=2) + '\n')


def call(command, output, timeout):
    with Path(output).open('xb') as stream:
        child = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                 start_new_session=True)
        try:
            code = child.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            os.killpg(child.pid, signal.SIGTERM)
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
            code = f'TIMEOUT_{timeout}S'
    return code


def require_capture():
    assert TRACE.is_dir() and any(TRACE.iterdir()), 'Saved GPU trace absent'
    plan = read(ARM / 'LAUNCH.json')
    assert plan['product']['sha256'] == EXPECTED_SHA
    assert plan['env']['MTL_CAPTURE_ENABLED'] == '1'
    assert plan['env']['BAR_NATIVE_GRASS_BOUNDARY_S01'] == '1'
    assert plan['env']['BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE'] == '1'
    assert plan['env']['MTL_DEBUG_LAYER'] == plan['env']['MTL_SHADER_VALIDATION'] == '0'
    assert read(ARM / 'GPU-CAPTURE.json')['status'] == 'CAPTURED'
    assert read(ARM / 'CLEANUP.json')['normalExit'] is True
    assert read(HERE / 'OWNER-CLOSED.json')['awakeReleased'] is True
    assert read(HERE / 'AWAKE-RELEASED.json')['status'] == 'RELEASED'
    assert read(HERE / 'RESULT.json')['status'] == 'CAPTURED'
    assert shutil.disk_usage(ANALYSIS.parent).free >= 5_000_000_000, 'Insufficient free space for replay'


def list_sessions(output):
    code = call([GPUDEBUG, '--list-sessions'], output, 15)
    return code, Path(output).read_text(errors='replace')


def run():
    require_capture()
    ANALYSIS.mkdir(parents=True, exist_ok=False)
    session = None
    awake = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '600', '-w', str(os.getpid())])
    receipt = dict(status='OPEN', trace=str(TRACE), signedBinarySHA256=EXPECTED_SHA,
                   profile='default GPU state; overlapping execution; one pass; no embed',
                   profileTimeoutSeconds=180, sessionIdleTimeoutSeconds=600,
                   awakePID=awake.pid, startedUTC=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                   performanceAccepted=False, exportCommands=EXPORTS)
    save(ANALYSIS / 'REPLAY-AWAKE.json', dict(status='ACTIVE', pid=awake.pid, maximumSeconds=600))
    try:
        code, listed = list_sessions(ANALYSIS / 'sessions-before.log')
        assert code == 0 and 'No active sessions' in listed, 'GPU debugger session already active; do not touch it'
        print('OPEN one owned GPU replay session', flush=True)
        open_cmd = [GPUDEBUG, '-t', str(TRACE), '--timeout', '600', '--json', '-c', 'status']
        receipt['openExit'] = call(open_cmd, ANALYSIS / 'open-session.log', 180)
        opened = (ANALYSIS / 'open-session.log').read_text(errors='replace')
        match = re.search(r'Session (\d+) created\.', opened)
        if match:
            session = match.group(1)
        else:
            code, listed = list_sessions(ANALYSIS / 'sessions-after-open-without-id.log')
            owned = re.findall(r'^\s*(\d+)\s', listed, re.M) if code == 0 else []
            if len(owned) == 1:
                session = owned[0]  # There were no sessions before this owned open attempt.
            raise RuntimeError('Open returned no session ID; terminate only uniquely identified new session')
        receipt['ownedSession'] = session
        assert receipt['openExit'] == 0
        for attempt in range(36):
            path = ANALYSIS / f'status-{attempt:02d}.log'
            code = call([GPUDEBUG, '-s', session, '--json', '-c', 'status'], path, 15)
            status = path.read_text(errors='replace')
            if code == 0 and re.search(r'"state"\s*:\s*"ready"', status):
                receipt['replayerReadyStatus'] = str(path)
                break
            time.sleep(5)
        else:
            raise RuntimeError('Replayer did not become ready within bounded status window')
        print('OPEN one default/overlapping replay profile', flush=True)
        profile_cmd = [GPUDEBUG, '-s', session, '--json', '-c',
                       'profile run --gpu-state default --exec overlapping']
        receipt['profileExit'] = call(profile_cmd, ANALYSIS / 'replay-profile.log', 180)
        profile_text = (ANALYSIS / 'replay-profile.log').read_text(errors='replace')
        assert receipt['profileExit'] == 0 and re.search(r'"success"\s*:\s*true', profile_text), 'Single replay profile failed; no repeat'
        print('PROFILE complete; exporting command, API, resource and GPU interval summaries', flush=True)
        exports = {}
        for name, node in EXPORTS.items():
            path = ANALYSIS / (name + '.jsonl')
            cmd = [GPUDEBUG, '-s', session, '--json', '-c', 'go ' + node, '-c', 'list --all']
            code = call(cmd, path, 60)
            exports[name] = dict(status='EXPORTED' if code == 0 else 'UNKNOWN',
                                 exit=code, path=str(path), bytes=path.stat().st_size)
        receipt['exports'] = exports
        receipt['profileCompleted'] = True
        receipt['status'] = ('PROFILE_AND_EXPORTS_COMPLETE' if all(
            x['status'] == 'EXPORTED' for x in exports.values()) else 'PROFILE_COMPLETE_EXPORT_PARTIAL')
    except BaseException as exc:
        receipt['status'] = 'STOP_REVIEW_REQUIRED'
        receipt['error'] = repr(exc)
        print('STOP replay; preserved raw: ' + repr(exc), flush=True)
    finally:
        if session is not None:
            receipt['terminateExit'] = call([GPUDEBUG, '--terminate', session], ANALYSIS / 'terminate-owned.log', 20)
        code, after = list_sessions(ANALYSIS / 'sessions-after.log')
        receipt['sessionListExit'] = code
        receipt['ownedSessionAbsent'] = session is not None and not re.search(r'^\s*' + re.escape(session) + r'\s', after, re.M)
        receipt['allSessionsAbsent'] = code == 0 and 'No active sessions' in after
        if awake.poll() is None:
            awake.terminate()
            awake.wait(timeout=5)
        receipt['awakeReleased'] = awake.poll() is not None
        receipt['closedUTC'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        save(ANALYSIS / 'REPLAY-AWAKE-RELEASED.json', dict(status='RELEASED', pid=awake.pid,
                                                         ownCaffeinateAlive=False))
        save(ANALYSIS / 'REPLAY-RESULT.json', receipt)
        print('CLOSED replay ' + receipt['status'], flush=True)
    if receipt['status'] not in ('PROFILE_AND_EXPORTS_COMPLETE', 'PROFILE_COMPLETE_EXPORT_PARTIAL') or not receipt['ownedSessionAbsent']:
        raise RuntimeError('Replay did not close cleanly; see REPLAY-RESULT.json')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', help='Run one owned profile after capture closes')
    args = parser.parse_args()
    print(json.dumps(dict(status='READY_NO_GPU' if not args.run else 'START', trace=str(TRACE),
                          profile='default/overlapping once', exports=EXPORTS,
                          boundedProfileSeconds=180, dynamicSession=True), indent=2), flush=True)
    if args.run:
        run()
