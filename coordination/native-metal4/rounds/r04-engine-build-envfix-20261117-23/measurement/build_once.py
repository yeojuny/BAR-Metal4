#!/usr/bin/env python3
"""Execute the granted R03 configure/build commands once against fixed R04 inputs."""
import collections
import datetime
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time

E = Path(__file__).resolve().parents[1]
B = E.parent / 'r03-r02-regression-20261117-01'
receipt = json.loads((E / 'ATTEMPT-CLAIM.json').read_text())
preflight = json.loads((E / 'DEPENDENCY-PREFLIGHT.json').read_text())
assert preflight['status'] == 'PASS'
assert os.environ.get('PKG_CONFIG_PATH') == preflight['previous_PKG_CONFIG_PATH']
build_env = dict(os.environ)
build_env['PKG_CONFIG_PATH'] = preflight['PKG_CONFIG_PATH']
receipt['process_local_PKG_CONFIG_PATH'] = build_env['PKG_CONFIG_PATH']
assert receipt['configure_attempts'] == receipt['engine_builds'] == 0
assert not (E / 'build').exists()
with (E / 'BUILD-STARTED').open('x') as f:
    f.write(datetime.datetime.now(datetime.timezone.utc).isoformat() + '\n')
receipt.update(commands=[], owned_pids=[], active_pid=None, cleanup_complete=False,
               awake_created=False, source_commit='5d20bd842b3c881166abb58f361482a5d7bb19e7')

def stamp():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def save():
    (E / 'EXECUTION.json').write_text(json.dumps(receipt, indent=2) + '\n')

def run(argv, name, budget_key):
    receipt[budget_key] += 1
    row = dict(argv=argv, log=name, start_utc=stamp())
    receipt['commands'].append(row)
    save()
    began = time.monotonic()
    with (E / name).open('xb') as log:
        p = subprocess.Popen(argv, cwd=E, stdout=log, stderr=subprocess.STDOUT, start_new_session=True, env=build_env)
        row['pid'] = p.pid
        receipt['owned_pids'].append(p.pid)
        receipt['active_pid'] = p.pid
        save()
        try:
            while p.poll() is None:
                if time.monotonic() - began > 1800 or log.tell() > 64 * 1024 * 1024:
                    raise RuntimeError('Bounded command limit exceeded')
                time.sleep(1)
        except BaseException:
            os.killpg(p.pid, signal.SIGTERM)
            try:
                p.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
                p.wait()
            raise
        finally:
            row.update(exit_code=p.returncode, end_utc=stamp(), wall_seconds=time.monotonic()-began)
            receipt['active_pid'] = None
            save()
    print(json.dumps({'phase':name, 'exit':p.returncode, 'wall_seconds':row['wall_seconds']}), flush=True)
    if p.returncode:
        raise RuntimeError(name + ' failed; no retry')

try:
    run(['cmake', '-S', str(E/'source/engine'), '-B', str(E/'build'), '-G', 'Ninja',
         '-C', str(E/'initial-cache.cmake'), '-DCMAKE_BUILD_TYPE=RelWithDebInfo'],
        'CONFIGURE.log', 'configure_attempts')
    def commands(root):
        data = json.loads((root/'build/compile_commands.json').read_text())
        return collections.Counter(json.dumps(row, sort_keys=True).replace(str(root), '<FIXED_ROOT>') for row in data)
    baseline, candidate = commands(B), commands(E)
    missing, extra = list((baseline-candidate).elements()), list((candidate-baseline).elements())
    closure = dict(status='PASS' if not missing and not extra else 'FAIL',
                   baselineCompileCommands=sum(baseline.values()), candidateCompileCommands=sum(candidate.values()),
                   missingCommands=missing, extraCommands=extra,
                   normalization='Only the isolated root path; all fields and duplicate counts retained')
    (E/'TOOLCHAIN-CLOSURE.json').write_text(json.dumps(closure, indent=2)+'\n')
    if missing or extra:
        raise RuntimeError('Toolchain/compile-command mismatch; no build or repair')
    run(['cmake', '--build', str(E/'build'), '--target', 'spring-metal4-scene', '--', '-j', '8'],
        'BUILD.log', 'engine_builds')
    receipt['raw_engine_sha256'] = hashlib.sha256((E/'build/spring-metal4-scene').read_bytes()).hexdigest()
    receipt['status'] = 'CONFIGURE_BUILD_PASS_PENDING_STATIC_CLOSURE_AND_SIGN'
except BaseException as exc:
    receipt['status'] = 'STOPPED_NO_RETRY'
    receipt['failure'] = str(exc)
    raise
finally:
    receipt['build_controller_finished_at_utc'] = stamp()
    receipt['build_controller_complete'] = True
    save()
