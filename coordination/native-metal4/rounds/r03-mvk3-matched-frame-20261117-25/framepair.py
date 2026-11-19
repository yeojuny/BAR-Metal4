"""Round25: reuse the frozen runner; one staged frame capture per pinned arm."""
import argparse
import difflib
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import traceback

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
C = HERE.parents[1]
SOURCE = C / 'evidence/r03-r02-regression-20261117-01/measurement/run.py'
spec = importlib.util.spec_from_file_location('r25_frozen_runner', SOURCE)
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)
base.RUN_ROOT = HERE
base.ORDER['clean_paced'] = [('R03', 'candidate'), ('MVK3', 'MVK3')]
base.base.PACED = ('clean_paced',)
base.base.CHILD_KEYS += ('MTL_CAPTURE_ENABLED',)
# No additional image-readback draw: the captured frame is the visual evidence.
base.base.image['configure'] = lambda controller: None
read, save, sha = base.common.read, base.save, base.sha
REQUEST = read(HERE / 'REQUEST.json')


def products():
    r03 = read(SOURCE.parent / 'PRODUCT.json')
    mvk = read(C / REQUEST['MVK3_reference'])['product']
    for name, product in [('R03', r03), ('MVK3', mvk)]:
        assert sha(product['binary']) == product['sha256'] == REQUEST['binary_pins'][name]
    return {'candidate': r03, 'MVK3': mvk}


base.products = products

SNAPSHOT = r'''
local function framePairSnapshot(which)
 local f=assert(io.open('LuaUI/Config/framepair-'..which..'.txt','w'))
 local a,b,paused=Spring.GetGameSpeed()
 f:write(string.format('tick=%d\ndraw=%d\npaused=%s\nspeed=%.9g,%.9g\ncamera=%s\n',Spring.GetGameFrame(),draw(),tostring(paused),a,b,camera()))
 local selected=Spring.GetSelectedUnits();table.sort(selected)
 f:write('selected='..table.concat(selected,',')..'\n')
 local visible=Spring.GetVisibleUnits();table.sort(visible)
 f:write('visible='..table.concat(visible,',')..'\n')
 local units=Spring.GetAllUnits();table.sort(units)
 for _,id in ipairs(units)do
  local x,y,z=Spring.GetUnitPosition(id)
  local dx,dy,dz=Spring.GetUnitDirection(id)
  local vx,vy,vz=Spring.GetUnitVelocity(id)
  local hp,maxhp,paralyze,capture,build=Spring.GetUnitHealth(id)
  f:write(string.format('unit=%d,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n',id,Spring.GetUnitDefID(id) or -1,Spring.GetUnitTeam(id) or -1,x or 0,y or 0,z or 0,dx or 0,dy or 0,dz or 0,vx or 0,vy or 0,vz or 0,hp or 0,maxhp or 0,paralyze or 0,capture or 0,build or 0))
 end
 local features=Spring.GetAllFeatures();table.sort(features)
 for _,id in ipairs(features)do
  local x,y,z=Spring.GetFeaturePosition(id)
  f:write(string.format('feature=%d,%d,%.9g,%.9g,%.9g\n',id,Spring.GetFeatureDefID(id) or -1,x or 0,y or 0,z or 0))
 end
 f:close()
 Spring.Echo('[FramePair] snapshot='..which..' simulation='..Spring.GetGameFrame()..' units='..#units..' features='..#features)
end
'''

HOLD = r'''
 if framePairRequested then
  local _,_,paused=Spring.GetGameSpeed()
  if paused and game~=1590 then
   framePairSnapshot('invalid-tick')
   Spring.Echo('[FramePair] FAILED_PAUSE_TICK actual='..game)
   budgetMark('capture-stop');Spring.Quit();widgetHandler:RemoveWidget(self);return
  end
  if paused and not framePairReady then
   if not framePairHoldDraw then framePairHoldDraw=draw() end
   if draw()>=framePairHoldDraw+8 then
    assert(cameraStillFixed(),'[FramePair] FAILED_CAMERA')
    framePairSnapshot('before')
    budgetMark('capture-start');framePairReady=true
   end
  end
  local finished=io.open('LuaUI/Config/framepair-finished','r')
  if finished then
   finished:close();framePairSnapshot('after')
   budgetMark('capture-done');Spring.Quit();widgetHandler:RemoveWidget(self);return
  end
  if Spring.DiffTimers(now,opened)>=200 then
   Spring.Echo('[FramePair] FAILED_CAPTURE_DEADLINE')
   Spring.Quit();widgetHandler:RemoveWidget(self)
  end
  return
 end
'''


def amend_controller(path):
    before = path.read_text()
    marker = '--208 read-only route witness.'
    assert before.count(marker) == 1
    body = before.replace(marker, SNAPSHOT + '\n' + marker, 1)
    body = 'local framePairRequested,framePairReady,framePairHoldDraw\n' + body
    needle = ' if f==1740 then endMeasurement(f) end'
    assert body.count(needle) == 1
    body = body.replace(needle, ''' if f==1500 then budgetMark("capture-boundary") end
 if f==1590 then
  frameStopped=true;framePairRequested=true;budgetMark("end")
  Spring.SendCommands('pause 1')
 end''', 1)
    needle = ' local now=Spring.GetTimer();local game=Spring.GetGameFrame()'
    assert body.count(needle) == 1
    body = body.replace(needle, needle + '\n' + HOLD, 1)
    path.write_text(body)
    return before, body


def stage():
    assert not (HERE / 'FREEZE.json').exists()
    base.stage('clean_paced', 0)
    prepared_path = HERE / 'clean_paced/PREPARED.json'
    prepared = read(prepared_path)
    frozen = []
    for item in prepared['order']:
        data, out = Path(item['data']), Path(item['out'])
        controller = data / 'LuaUI/Widgets/zzz_native_short_run.lua'
        before, body = amend_controller(controller)
        if item['arm'] == 'R03':
            (HERE / 'controller-before.lua').write_text(before)
            (HERE / 'controller.lua').write_text(body)
            (HERE / 'controller.diff').write_text(''.join(difflib.unified_diff(before.splitlines(True), body.splitlines(True), fromfile='original-controller', tofile='framepair-controller')))
        else:
            assert body == (HERE / 'controller.lua').read_text()
        plan = read(out / 'LAUNCH.json')
        plan['checksums'][str(controller)] = sha(controller)
        plan['stagedControllerSHA256'] = sha(controller)
        plan['env'].update(MTL_CAPTURE_ENABLED='1', MTL_DEBUG_LAYER='0', MTL_SHADER_VALIDATION='0')
        plan['env'].pop('MTL_LOG_TO_STDERR', None)
        plan['gpuCapture'] = dict(tool='/usr/bin/gpucapture', boundary='Layer', count=1, targetSim=1590, timeoutSeconds=180,
                                  trace=str(out / 'frame.gputrace'), exactTickFromCapturedBoundBufferRequired=True)
        plan['diagnosticOnly'] = True
        save(out / 'LAUNCH.json', plan)
        item['controllerSHA256'] = sha(controller)
        save(HERE / (item['arm'] + '-LAUNCH-FROZEN.json'), plan)
        save(HERE / (item['arm'] + '-ENV-FROZEN.json'), plan['env'])
        frozen.append(dict(arm=item['arm'], launch=str(out/'LAUNCH.json'), launchSHA256=sha(out/'LAUNCH.json'),
                           binary=plan['product'], controllerSHA256=sha(controller), inputs=plan['checksums']))
    save(prepared_path, prepared)
    libraries = dict(base.expected_libraries)
    r03 = products()['candidate']
    for p in sorted((Path(r03['native'])/'lib').glob('*.dylib')):
        libraries[str(p)] = sha(p)
    assert all(sha(p) == h for p,h in libraries.items())
    shutil.copyfile(C/'UPSTREAM-CONTRIBUTIONS.json', HERE/'UPSTREAM-CONTRIBUTIONS.json')
    save(HERE / 'FREEZE.json', dict(status='READY_NO_GAME', round_id=REQUEST['round_id'], attempt_id=REQUEST['attempt_id'],
        arms=frozen, libraries=libraries, runnerSHA256=sha(__file__), inheritedRunnerSHA256=sha(SOURCE),
        controllerSHA256=sha(HERE/'controller.lua'), captureOrder=['R03','MVK3'], engineSourceEdits=0,
        profileSemantics=REQUEST['profile_semantics'], bar='PAUSED'))
    print('FROZEN both arms; games 0', flush=True)


def layer_id(output):
    found = []
    for line in output.splitlines():
        if re.search(r'\bLayer\b', line, re.I):
            m = re.match(r'^\s*(\d+)\b', line) or re.search(r'\b(?:id|boundary)\s*[:=#]\s*(\d+)\b', line, re.I)
            if m: found.append(int(m[1]))
    return found[0] if len(found) == 1 else None


def capture(data, out, stop, receipt):
    try:
        cfg = data/'LuaUI/Config'
        for name in ['capture-boundary', 'capture-start']:
            marker = cfg/('budget192.'+name)
            while not marker.exists():
                if stop.wait(0.1) or (cfg/'budget192.capture-stop').exists():
                    receipt['status']='MARKER_NOT_REACHED_'+name;return
            if name == 'capture-boundary':
                pid = read(out/'OWNED.json')['pid']
                listed = subprocess.run(['/usr/bin/gpucapture','boundaries','--pid',str(pid)],capture_output=True,text=True,timeout=8)
                receipt.update(pid=pid,boundaryExit=listed.returncode,boundaryStdout=listed.stdout,boundaryStderr=listed.stderr)
                boundary=layer_id(listed.stdout)
                assert listed.returncode==0 and boundary is not None, 'NO_UNIQUE_LAYER_BOUNDARY'
                receipt['layerBoundaryID']=boundary
        marker_state=(cfg/'framepair-before.txt').read_text()
        assert 'tick=1590\n' in marker_state and 'paused=true\n' in marker_state
        target=out/'frame.gputrace';assert not target.exists()
        cmd=['/usr/bin/gpucapture','start','--pid',str(pid),'--boundary',str(boundary),'--count','1','--output',str(target)]
        receipt.update(command=cmd,captureCommandInvocations=1,markerIsNotActualCaptureTickProof=True)
        save(out/'GPU-CAPTURE-STARTED.json',receipt)
        with (out/'gpucapture.log').open('xb') as stream:
            child=subprocess.Popen(cmd,stdout=stream,stderr=subprocess.STDOUT,start_new_session=True)
            receipt['capturePID']=child.pid
            try: code=child.wait(timeout=180)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid,signal.SIGTERM)
                try:child.wait(timeout=3)
                except subprocess.TimeoutExpired:os.killpg(child.pid,signal.SIGKILL);child.wait()
                code='TIMEOUT_180S'
        receipt.update(captureExit=code,trace=str(target),tracePresent=target.is_dir(),
                       status='CAPTURED' if code==0 and target.is_dir() else 'CAPTURE_FAILED')
    except BaseException:
        receipt.update(status='CAPTURE_EXCEPTION',error=traceback.format_exc())
    finally:
        save(out/'GPU-CAPTURE.json',receipt)
        (data/'LuaUI/Config/framepair-finished').write_text(receipt.get('status','UNKNOWN'))


def diagnostic_parse(out,data,arm,stage_name):
    return dict(run=arm,diagnosticOnly=True,FPSAccepted=False,simulationTickProof='PENDING_CAPTURE_BUFFER_INSPECTION')


def game(arm):
    frozen=read(HERE/'FREEZE.json')
    assert sha(__file__)==frozen['runnerSHA256'] and sha(SOURCE)==frozen['inheritedRunnerSHA256']
    assert all(sha(p)==h for p,h in frozen['libraries'].items())
    row=next(r for r in frozen['arms'] if r['arm']==arm)
    assert sha(row['launch'])==row['launchSHA256']
    if arm=='MVK3':
        assert read(HERE/'R03-TICK-GATE.json')['status']=='PASS_CAPTURE_BOUND_SIM1590'
    with (HERE/(arm+'-GAME-CLAIM.json')).open('x') as f:
        json.dump(dict(arm=arm,attempt_id=REQUEST['attempt_id'],utc=base.base.utc(),retry=False),f)
    prepared=base.restore('clean_paced',tuple(a for a in ['R03','MVK3'] if a!=arm))
    data,out,env,plan=prepared[arm]
    assert read(base.base.SHARED_AWAKE)['status']=='RELEASED'
    awake_process=subprocess.Popen(['/usr/bin/caffeinate','-disu','-t','900','-w',str(os.getpid())])
    awake=dict(status='ACTIVE',campaign='clean_paced',pid=awake_process.pid,startUTC=base.base.utc(),maximumSeconds=900,securitySettingsChanged=False)
    save(base.base.SHARED_AWAKE,awake);save(out/'AWAKE.json',awake)
    stop=threading.Event();receipt={};error=None
    worker=threading.Thread(target=capture,args=(data,out,stop,receipt),name='round25-capture')
    base.base.active.update(prepared=prepared[arm],mapped=False,campaign='clean_paced')
    base.base.harness.parse=diagnostic_parse
    try:
        worker.start();print('OPEN '+arm+' one diagnostic game',flush=True)
        try: base.base.harness.launch(HERE/'clean_paced/preparation',arm,prepared[arm],1)
        except BaseException:error=traceback.format_exc()
        finally:
            stop.set();worker.join()
            for p in (data/'LuaUI/Config').glob('framepair-*'):
                if p.is_file():shutil.copyfile(p,out/p.name)
        if error:save(out/'POSTRUN-ERROR.json',dict(error=error))
        assert read(out/'CLEANUP.json')['normalExit'], 'NON_NORMAL_GAME_EXIT'
        assert receipt.get('status')=='CAPTURED',receipt.get('status')
        assert read(out/'CHILD-MODE.json')['status']=='PASS_ACTUAL_CHILD_ENVIRONMENT'
        assert not error, 'RUNNER_OR_RUNTIME_GATE_FAILED'
        save(out/'CAPTURE-RESULT.json',dict(status='CAPTURED_PENDING_BOUND_TICK_GATE',arm=arm,capture=receipt['trace'],actualCaptureTick='UNKNOWN',profileCommands=0))
        print('CAPTURED '+arm+'; actual bound tick still pending',flush=True)
    finally:
        stop.set()
        if worker.is_alive():worker.join()
        base.base.cleanup('clean_paced',prepared)
        if awake_process.poll() is None:awake_process.terminate();awake_process.wait(timeout=5)
        awake.update(status='RELEASED',endUTC=base.base.utc(),ownCaffeinateAlive=False)
        save(base.base.SHARED_AWAKE,awake);save(out/'AWAKE-RELEASED.json',awake)
        save(out/'OWNER-CLOSED.json',dict(status='CLOSED',arm=arm,captureStatus=receipt.get('status'),awakeReleased=True,workerJoined=True,utc=base.base.utc()))
        print('CLOSED game '+arm+' '+receipt.get('status','UNKNOWN'),flush=True)


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument('--stage',action='store_true');g.add_argument('--game',choices=['R03','MVK3'])
    a=p.parse_args()
    stage() if a.stage else game(a.game)
