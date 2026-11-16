"""One S01 Metal protocol helper, then one candidate validation game; explicit --run only."""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import time


HERE = Path(__file__).resolve().parent
S01 = HERE.parent
HELPER = S01 / 'validation/gpu_boundary_parity'
SOURCE = S01 / 'validation/gpu_boundary_parity.mm'
VALIDATION = HERE / 'runs/s01_valid01/validation_paced'
ARM = VALIDATION / 'preparation/runs/V_S01'
OUTPUT = S01 / 'validation/runs/s01_first_gate01'
EXPECTED_SOURCE = '4cc76cd75c42bf530e2ce819080ed17d02e88da648f51f4ca1b0995caccc5633'
EXPECTED_HELPER = '5d77046a6573851988a376ea6aa357b4d89108536577beb4652a1cb37c2d8195'
EXPECTED_CANDIDATE = '814ab5de159cd3c8ea93e1b897f24583e7d7537eacffdb7052395b51880855f6'
PASS_LINE = ('PASS cases=128 color_depth_stencil_byte_identical=128 '
             'visible_color_depth_stencil_writes=128 Metal4_checkpoint_and_split=1 '
             'actual_fragment_producer=1 grass_only_producer_value_witness=128 '
             'snapshot_copy_witness=128 synthetic_protocol_not_full_game=1')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read(path):
    return json.loads(Path(path).read_text())


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def metal_receipt_status(metal):
    return metal.get('status', 'NOT_EVALUATED')


def check():
    assert sha(SOURCE) == EXPECTED_SOURCE
    assert sha(HELPER) == EXPECTED_HELPER
    product = read(HERE / 'PRODUCT.json')
    assert product['sha256'] == EXPECTED_CANDIDATE
    assert sha(product['binary']) == EXPECTED_CANDIDATE
    assert read(VALIDATION / 'PREPARED.json')['status'] == 'READY_NO_GAME'
    plan = read(ARM / 'LAUNCH.json')
    assert plan['product']['sha256'] == EXPECTED_CANDIDATE
    assert plan['env']['BAR_NATIVE_GRASS_BOUNDARY_S01'] == '1'
    assert plan['env']['MTL_DEBUG_LAYER'] == '1'
    assert plan['env']['MTL_SHADER_VALIDATION'] == '0'
    assert plan['env']['BAR_NATIVE_FRAME_TRACE'] == '0'
    assert plan['ultra']['actualSamples'] == 0
    assert plan['ultra']['expectedView'] == [1512, 950]
    assert plan['ultra']['settings']['LimitIdleFps'] == 0
    assert not (ARM / 'OWNED.json').exists()
    assert not OUTPUT.exists()
    return dict(status='READY_NO_GPU_OR_GAME', helperSourceSHA256=EXPECTED_SOURCE,
                helperBinarySHA256=EXPECTED_HELPER, candidateSHA256=EXPECTED_CANDIDATE,
                validationArm=str(ARM), helperOutput=str(OUTPUT),
                helperEnvironment={'MTL_DEBUG_LAYER': '1', 'MTL_SHADER_VALIDATION': '1',
                                   'MTL_LOG_TO_STDERR': '1'},
                validationEnvironment={key: plan['env'].get(key) for key in
                                       ('BAR_NATIVE_GRASS_BOUNDARY_S01', 'MTL_DEBUG_LAYER',
                                        'MTL_SHADER_VALIDATION', 'BAR_NATIVE_FRAME_TRACE')})


def run_helper():
    OUTPUT.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ)
    env.update(MTL_DEBUG_LAYER='1', MTL_SHADER_VALIDATION='1', MTL_LOG_TO_STDERR='1')
    awake = subprocess.Popen(['/usr/bin/caffeinate', '-disu', '-t', '300', '-w', str(os.getpid())])
    save(OUTPUT / 'HELPER-AWAKE.json', dict(status='ACTIVE', pid=awake.pid,
                                           maximumSeconds=300))
    start = time.monotonic()
    child = None
    outcome = None
    try:
        print('OPEN GPU helper 128 cases', flush=True)
        with (OUTPUT / 'helper.stdout.log').open('xb') as stdout, (OUTPUT / 'helper.stderr.log').open('xb') as stderr:
            child = subprocess.Popen([str(HELPER)], env=env, stdout=stdout, stderr=stderr,
                                     start_new_session=True)
            try:
                code = child.wait(timeout=180)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGTERM)
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
                code = 'TIMEOUT_180S'
        output = (OUTPUT / 'helper.stdout.log').read_text(errors='replace')
        errors = (OUTPUT / 'helper.stderr.log').read_text(errors='replace')
        positive = output.count(PASS_LINE) == 1 and output.strip() == PASS_LINE
        metal_errors = [line for line in errors.splitlines() if re.search(
            r'MTLDebug.*(?:error|assert)|Metal.*(?:error|fault|assert)|failed assertion|(?:hazard|residency).*(?:error|fault)',
            line, re.I)]
        outcome = dict(status='PASS' if code == 0 and positive and not metal_errors else 'FAIL',
                       exitCode=code, passLineObserved=positive, metalErrorLines=metal_errors,
                       elapsedSeconds=time.monotonic() - start, helperPID=child.pid,
                       sourceSHA256=sha(SOURCE), helperSHA256=sha(HELPER),
                       environment={key: env[key] for key in
                                    ('MTL_DEBUG_LAYER', 'MTL_SHADER_VALIDATION', 'MTL_LOG_TO_STDERR')},
                       stdout=str(OUTPUT / 'helper.stdout.log'), stderr=str(OUTPUT / 'helper.stderr.log'))
        save(OUTPUT / 'HELPER-RESULT.json', outcome)
        print('GPU helper ' + outcome['status'], flush=True)
        return outcome
    finally:
        if child and child.poll() is None:
            os.killpg(child.pid, signal.SIGTERM)
            child.wait(timeout=3)
        if awake.poll() is None:
            awake.terminate()
            awake.wait(timeout=5)
        save(OUTPUT / 'HELPER-AWAKE.json', dict(status='RELEASED', pid=awake.pid,
                                               ownCaffeinateAlive=False,
                                               helperStatus=outcome['status'] if outcome else 'INCOMPLETE'))


def run_game():
    command = [sys.executable, str(HERE / 'run.py'), '--run', 'validation_paced', '--suite', 's01_valid01']
    print('OPEN S01 candidate Metal validation game', flush=True)
    with (OUTPUT / 'game-controller.stdout.log').open('xb') as stdout, (OUTPUT / 'game-controller.stderr.log').open('xb') as stderr:
        code = subprocess.run(command, stdout=stdout, stderr=stderr).returncode
    cleanup = read(ARM / 'CLEANUP.json') if (ARM / 'CLEANUP.json').exists() else {}
    child = read(ARM / 'CHILD-MODE.json') if (ARM / 'CHILD-MODE.json').exists() else {}
    metal = read(ARM / 'METAL-VALIDATION.json') if (ARM / 'METAL-VALIDATION.json').exists() else {}
    grass = read(ARM / 'GRASS-BOUNDARY.json') if (ARM / 'GRASS-BOUNDARY.json').exists() else {}
    awake = read(VALIDATION / 'AWAKE-RELEASED.json') if (VALIDATION / 'AWAKE-RELEASED.json').exists() else {}
    engine_log = (ARM / 'engine.log').read_text(errors='replace') if (ARM / 'engine.log').exists() else ''
    image = ARM / 'scene.png'
    gates = dict(controllerExitZero=code == 0, engineNormalExit=cleanup.get('normalExit') is True,
                 childModeVerified=child.get('status') == 'PASS_ACTUAL_CHILD_ENVIRONMENT',
                 childFlag1=child.get('actual', {}).get('BAR_NATIVE_GRASS_BOUNDARY_S01') == '1',
                 actualMetalValidationEnabled='Metal API Validation Enabled' in engine_log,
                 noMatchedMetalErrors=(metal_receipt_status(metal) == 'NO_MATCHED_METAL_ERRORS' if metal else None),
                 positiveGrassSplit=grass.get('status') == 'POSITIVE_SPLIT',
                 nativeRetirement=bool(re.search(r'\[NativePass16[56]\][^\n]*retired=1', engine_log)),
                 imagePresent=image.is_file() and image.stat().st_size > 1000,
                 awakeReleased=awake.get('status') == 'RELEASED')
    receipt = dict(status='READY_FOR_ROOT_IMAGE_REVIEW' if all(value is True for value in gates.values()) else 'STOP_REVIEW_REQUIRED',
                   gates=gates, metalReceiptStatus=metal_receipt_status(metal),
                   missingMetalReceiptMeans='NOT_EVALUATED; inspect raw engine.log separately, never infer actual Metal errors',
                   command=command, controllerExitCode=code,
                   engineSHA256=EXPECTED_CANDIDATE, image=str(image) if gates['imagePresent'] else None,
                   output=str(ARM), controllerStdout=str(OUTPUT / 'game-controller.stdout.log'),
                   controllerStderr=str(OUTPUT / 'game-controller.stderr.log'),
                   diagnosticFPSExcluded=True, captureAndCleanNotAuthorized=True)
    save(OUTPUT / 'GAME-RESULT.json', receipt)
    print('S01 validation ' + receipt['status'], flush=True)
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='store_true', help='Execute exactly one helper, then one candidate game if it passes')
    args = parser.parse_args()
    ready = check()
    if not args.run:
        print(json.dumps(ready, indent=2))
        return
    save(HERE / 'FIRST-GATE-OPEN.json', dict(status='OPEN', utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                                            helperSHA256=EXPECTED_HELPER, candidateSHA256=EXPECTED_CANDIDATE))
    try:
        helper = run_helper()
        if helper['status'] != 'PASS':
            save(OUTPUT / 'FIRST-GATE.json', dict(status='STOP_HELPER_FAILED', games=0,
                                                  helperResult=str(OUTPUT / 'HELPER-RESULT.json')))
            return
        game = run_game()
        save(OUTPUT / 'FIRST-GATE.json', dict(status=game['status'], games=1,
                                              helperResult=str(OUTPUT / 'HELPER-RESULT.json'),
                                              gameResult=str(OUTPUT / 'GAME-RESULT.json')))
    finally:
        save(HERE / 'FIRST-GATE-CLOSED.json', dict(status='CLOSED',
             utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
             helperResult=str(OUTPUT / 'HELPER-RESULT.json'),
             gameResult=str(OUTPUT / 'GAME-RESULT.json') if (OUTPUT / 'GAME-RESULT.json').exists() else None))


if __name__ == '__main__':
    main()
