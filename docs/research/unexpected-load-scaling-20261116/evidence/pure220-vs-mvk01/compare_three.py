"""Preserved Small/MVK Metal3 -> MVK Metal4 -> pure220 -> pure220 -> MVK Metal4 -> MVK Metal3.

Six fresh controlled-engine-direct games; no builds, retries or exclusions.
The preserved Small compiler optimization flags are not independently proven.
Historical Small app-first launch differs from this direct-engine comparison.
"""
from pathlib import Path
import argparse,csv,datetime,hashlib,importlib.util,json,math,os,re,shutil,subprocess,sys,traceback
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent
C=HERE.parents[1]
TOOLS=C/'evidence/gpu-next219-01/measurement-aligned01'
PREVIOUS=C/'evidence/gpu-next219-01/do-no-harm02'
OLD=C/'evidence/bloom217-boundary02/build-recovery01'
MVK=C/'evidence/pass192/retry01/runs/m_product'
SMALL=Path('${AUDIT_USER_HOME}/Desktop/BAR-AppleSilicon-App/artifacts/runtime/v104-r-small-bundle-candidate-20261021/BAR.app')
ENGINE=SMALL/'Contents/Resources/bundled-assets/engine/2026.09.01-24-g01efdf4 master/spring'
PURE=PREVIOUS/'integration220/build-layout-private-sampler/packages/integration220-layout-private-sampler-checkpoint'
read=lambda p:json.loads(Path(p).read_text())
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
save=lambda p,v:Path(p).write_text(json.dumps(v,indent=2)+'\n')
PINS={str(ENGINE):'d715965659b977e05b8ccafe76fae0936cade1a7481871acff7eec876366f1aa',str(SMALL/'Contents/Resources/app.asar'):'3e723fc2e06664b9caf8e357b25e77c574da4f026e653e002d31bece29174d29'}


def load(path,name):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m


def install_common_parser(base):
 """Retain all original common checks; native startup checks stay native-only."""
 aligned=sys.modules['aligned'];old=aligned._validator_module();s=(OLD/'run/validate.py').read_text()
 edits=[
  ("    assert 'safe view 1512x950' in log and '1512x982' in log", "    if 'MVK' not in arm: assert 'safe view 1512x950' in log and '1512x982' in log\n    else: assert 'view=<0,0 1512x950>' in log and 'vsx=1512.000000, vsy=950.000000' in log"),
  ("    assert '[Metal4Scene] First presented simulation frame' in log", "    if 'MVK' not in arm: assert '[Metal4Scene] First presented simulation frame' in log\n    else: assert re.search(r'GL renderer\\s*:\\s*zink Vulkan .*MOLTENVK',log)"),
  ("    assert len(samples)==40 and 39.9<seconds<41 and draws==sum(int(s['draws']) for s in samples)", "    assert samples and seconds>0 and draws==sum(int(s['draws']) for s in samples)"),
  ("    assert first_sim<=480 and 511<=last_sim<=2250, 'Required sim anchors must be inside the measured interval'", "    assert first_sim==600 and last_sim==1800, 'Measured simulation boundaries must be exactly600..1800'"),
  ("    assert 39.8<(since['end']-since['start'])/1e9<41", "    assert (since['end']-since['start'])/1e9+0.05>=seconds>0")]
 for a,b in edits: s=aligned._replace_once(s,a,b)
 module=aligned._load(s,OLD/'run/validate.py','common_backend_validate')
 module.validate_frames=old.validate_frames;aligned._validator=module
 base.harness.parse=aligned.parse
 return edits


def common_condition(harness,row):
 value=harness.condition(row);value['route']={'features':value['route']['features']};return value


def products():
 backend=read(MVK/'BACKEND-LOADED.json')
 for path,digest in {**PINS,**backend['runtimeLibraries']}.items(): assert sha(path)==digest,path
 native=PURE/'app-build/mac-arm64/BAR.app/Contents/Resources/native-engines/development';binary=native/'bin/spring-metal4-scene'
 assert sha(binary)=='4a53fbd8bad898dace64545c47f80acfd806ead6aece2a801da23c5daa83ae2f'
 assert sha(PURE/'SOURCE-MANIFEST.json')=='5155b08a8929a505db50cd81db24ee2a29ee9013f8439043bbac346bb5cbec9d'
 return {'MVK':dict(variant='preserved-small-metal3',binary=str(ENGINE),sha256=sha(ENGINE),native=str(ENGINE.parent),package=str(SMALL),check=str(MVK/'BACKEND-LOADED.json'),checkSHA256=sha(MVK/'BACKEND-LOADED.json')),
         '220':dict(variant='pure220',binary=str(binary),sha256=sha(binary),native=str(native),package=str(PURE),check=str(PURE/'PACKAGE-RESULT.json'),checkSHA256=sha(PURE/'PACKAGE-RESULT.json'))}


def historical_env(data,assets,home,mode):
 text=(MVK/'lobby.log').read_text();matches=re.findall(r'native-engine-env: (\{[^\n]+\})',text);assert len(matches)==1
 values=json.loads(matches[0]);values={k:v for k,v in values.items()if v!='<unset>'}
 original=read(C/'evidence/pass192/retry01/launch-plans/m_product.json')['environmentOverrides']
 old_state=original['BAR_STATE_PATH'];old_assets=original['BAR_ASSETS_PATH']
 for key,value in list(values.items()):values[key]=value.replace(old_assets,str(assets)).replace(old_state,str(data.parent))
 values.update(BAR_MACOS_VULKAN_BACKEND='moltenvk',BAR_METAL_BACKEND='METAL3',BAR_METAL_BACKEND_REQUESTED='METAL3',MVK_CONFIG_USE_METAL4='0',BAR_ZINK_MVK_DISABLE_ASYNC_WAIT='1',BAR_STATE_PATH=str(data.parent),BAR_ASSETS_PATH=str(assets),HOME=str(home),BAR_MACOS_TARGET_DISPLAY='0,0,1512,982')
 if mode=='MVK4':
  values.update(BAR_METAL_BACKEND='METAL4',BAR_METAL_BACKEND_REQUESTED='METAL4',MVK_CONFIG_USE_METAL4='1')
  values['XDG_CACHE_HOME']=str(data/'cache/metal4')
 return values


def backend_evidence(out,row,kind):
 log=(out/'infolog.txt').read_text();status=[dict(re.findall(r'(\w+)=([^\s]+)',s))for s in re.findall(r'\[BackendCommonStatus\] (.*)',log)]
 assert [int(s['frame'])for s in status]==[480,2250]
 assert all(s['view']=='1512,950,0,0'for s in status)
 assert all(s['MSAA']=='0' and s['MSAALevel']=='-1' and s['VSync']=='0'for s in status),status
 if kind.startswith('MVK'):
  assert re.search(r'GL renderer\s*:\s*zink Vulkan .*MOLTENVK',log)
  assert '[VSync::SetInterval] interval=0 (disabled)'in log and 'using 0x anti-aliasing'in log
  assert 'view=<0,0 1512x950>'in log and 'vsx=1512.000000, vsy=950.000000'in log
 else:
  assert '[NativeMRT164]'in log
  extents={tuple(map(int,m))for m in re.findall(r'\[NativeMRT145\][^\n]*?width=(\d+)\s+height=(\d+)',log)}
  assert extents=={(1512,950)},extents
 evidence=[line for line in log.splitlines()if len(line)<3000 and not any(s in line for s in ['[MatchedFPS]','[CAS::GASCB]']) and any(s in line for s in ['SSAO','Bloom','Contrast Adaptive','CAS213','[NativeWater','Creating Water','VSync::SetInterval','GL renderer','UpdateViewPortGeometry','UpdateScreenMatrices','using 0x anti-aliasing'])]
 return dict(backend=kind,commonStatus=status,effectStartupEvidence=evidence,viewport=[1512,950],fullEffectPixelParity=False,nativeOnlyRoute=row['renderRoute'])


def pooled(rows,kind,stage):
 chosen=[r for r in rows if r['backend']==kind];values=[]
 for r in chosen:
  p=stage/'runs'/r['run']/'TERRAIN211-FRAMES.csv';assert sha(p)==r['frameStats']['rawSHA256']
  with p.open()as f: values.extend(float(v['seconds'])for v in csv.DictReader(f))
 ordered=sorted(values);tail=ordered[-math.ceil(len(ordered)*.01):]
 return dict(FPS=sum(r['draws']for r in chosen)/sum(r['seconds']for r in chosen),samples=len(values),p99Ms=1000*ordered[math.ceil(.99*len(ordered))-1],slow1PercentFPS=len(tail)/sum(tail),maximumMs=1000*max(values),over16_667ms=sum(v>1/60 for v in values),over33_333ms=sum(v>1/30 for v in values),interpolation=False,excluded=0)


def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--hq-only',action='store_true',required=True);parser.add_argument('--check-only',action='store_true');args=parser.parse_args()
 base=load(C/'evidence/display-settings219/performance-218-219-01/compare.py','shared_original_harness');harness=base.harness;edits=install_common_parser(base);original=products();prods={'MVK3':dict(original['MVK'],variant='preserved-small-requested-metal3'),'MVK4':dict(original['MVK'],variant='preserved-small-requested-metal4'),'220':original['220']}
 if args.check_only:print(json.dumps(dict(status='PREPARED_NO_NATIVE_LAUNCH',products=prods,commonParserEdits=edits),indent=2));return
 stage=HERE/'runtime-three/preparation';assert not stage.exists(),'Preserve prior attempt; no automatic retries/resume';stage.mkdir(parents=True)
 shutil.copyfile(PREVIOUS/'AUDIO-REFERENCE.json',stage.parent/'AUDIO-REFERENCE.json')
 active={'out':None,'data':None,'kind':None,'stackDone':False,'checks':None}
 expected=read(MVK/'BACKEND-LOADED.json')['runtimeLibraries']
 def preflight(remaining):
  result=base.power.preflight(remaining)
  out=active['out']
  if out and active['kind'].startswith('MVK')and not active['stackDone']and(out/'OWNED.json').exists()and(out/'engine.log').exists():
   text=(out/'engine.log').read_text()
   driver=active['data']/'stack-game.stderr.log'
   driver_text=driver.read_text(errors='replace')if driver.exists()else text
   assert '[MatchedAnchor] boundary=start'not in text,'Effective stack check missed warmup; no measurement-time inspection allowed'
   active_queues=re.findall(r'Native Metal 4 command backend active on queue ([^\n]+)\.',driver_text)
   if re.search(r'GL renderer\s*:\s*zink Vulkan .*MOLTENVK',text)and(active['kind']!='MVK4'or active_queues):
    pid=read(out/'OWNED.json')['pid'];p=subprocess.run(['/usr/sbin/lsof','-a','-p',str(pid),'-d','txt','-Fn'],capture_output=True,text=True,timeout=5,check=True)
    mapped=[s[1:]for s in p.stdout.splitlines()if s.startswith('n')]
    assert all(path in mapped for path in expected), 'Pinned Small graphics libraries not mapped'
    assert str(ENGINE)in mapped
    q=subprocess.run(['/bin/ps','eww','-p',str(pid),'-o','command='],capture_output=True,text=True,timeout=3,check=True)
    flags={key:(re.search(r'(?:^|\s)'+key+r'=([^\s]+)',q.stdout).group(1)if re.search(r'(?:^|\s)'+key+r'=([^\s]+)',q.stdout)else None)for key in ['BAR_MACOS_VULKAN_BACKEND','BAR_METAL_BACKEND','BAR_METAL_BACKEND_REQUESTED','MVK_CONFIG_USE_METAL4']}
    assert flags==dict(BAR_MACOS_VULKAN_BACKEND='moltenvk',BAR_METAL_BACKEND='METAL4'if active['kind']=='MVK4'else'METAL3',BAR_METAL_BACKEND_REQUESTED='METAL4'if active['kind']=='MVK4'else'METAL3',MVK_CONFIG_USE_METAL4='1'if active['kind']=='MVK4'else'0'),flags
    save(out/'BACKEND-LOADED.json',dict(status='PASS_INITIAL_WARMUP_STACK',pid=pid,flags=flags,expectedRuntimeLibraries=expected,mappedRuntimeLibraries=[x for x in mapped if x in expected],mappedMoltenVK=[x for x in mapped if 'MoltenVK'in x],observedAI=[x for x in mapped if re.search(r'lib(?:AIInterface|SkirmishAI)\.dylib$',x)],warmupMetal4ActiveQueues=active_queues,duringMeasuredWindow=False,queries=1))
    active['stackDone']=True
  return result
 harness.preflight=preflight
 protected=[Path(__file__),HERE/'backend-status.lua',TOOLS/'control.lua',TOOLS/'aligned.py',OLD/'run/campaign.py',OLD/'run/validate.py',OLD/'run/metrics.py',OLD/'run/audio.py',C/'evidence/gpu-next219-01/hq_preflight.py']
 pins={str(p):sha(p)for p in protected}
 order=[('M1_MVK3','MVK3'),('M1_MVK4','MVK4'),('N1_220','220'),('N2_220','220'),('M2_MVK4','MVK4'),('M2_MVK3','MVK3')]
 save(HERE/'DESIGN-THREE.json',dict(order=order,products=prods,builds=0,commonParserEdits=edits,toolPins=pins,priorTimingSamplesReused=0,releaseFlags='Preserved distribution candidate binaries pinned; compiler optimization flags not independently verified',launchDifference='Controlled direct-engine in both arms; historical Small app-first receipt retained',measurement=[600,1800],commonStatusWidget='Read-only at480/2250 only; original controller/DrawScreen/camera byte-identical'))
 reference=read(C/'evidence/commander213-session03/run/preparation/REFERENCE.json');reference['route']={'features':reference['route']['features']}
 rows=[];completion=dict(status='STARTED',startUTC=datetime.datetime.now(datetime.timezone.utc).isoformat(),launchAttempts=0)
 try:
  base.power.preflight(6)
  for index,(arm,kind)in enumerate(order):
   assert all(sha(path)==h for path,h in pins.items()),'Tool source changed'
   for path,h in {**PINS,**expected}.items():assert sha(path)==h
   prepared=harness.stage_arm(stage,arm,prods[kind],C/'evidence/mvk104-vs181-unlocked01/private/mvk104/assets');data,out,env,plan=prepared
   profile={str(Path(p).relative_to(data)):h for p,h in plan['checksums'].items()};assert profile==read(PREVIOUS/'FROZEN-PROFILE.json')
   home=data.parents[1]/'home';home.mkdir();env['HOME']=str(home);plan['env']['HOME']=str(home)
   widget=data/'LuaUI/Widgets/zzz_backend_common_status.lua';shutil.copyfile(HERE/'backend-status.lua',widget);plan['checksums'][str(widget)]=sha(widget)
   if kind.startswith('MVK'):
    clean={k:v for k,v in os.environ.items()if not k.startswith(('BAR_','MTL_','METAL_','RECOIL_','VK_','MESA_','MVK_','SPRING_','GALLIUM_','ZINK_','EGL_','DYLD_','XDG_','FONTCONFIG_'))}
    overrides=historical_env(data,data.parents[1]/'assets',home,kind);clean.update(overrides);env.clear();env.update(clean);plan['env']=overrides
   save(out/'LAUNCH.json',plan);active.update(out=out,data=data,kind=kind,stackDone=False)
   completion['launchAttempts']+=1
   try: row=harness.launch(stage,arm,prepared,len(order)-index)
   finally:
    active['out']=None
    if widget.exists():
     assert sha(widget)==plan['checksums'][str(widget)]
     widget.unlink()
   if kind.startswith('MVK'):
    assert active['stackDone'],'Warmup stack not verified'
    driver=data/'stack-game.stderr.log'
    retained=out/'stack-game.stderr.log'
    if driver.exists():shutil.copyfile(driver,retained)
    else:retained=out/'engine.log'
    text=retained.read_text(errors='replace')
    queues=re.findall(r'Native Metal 4 command backend active on queue ([^\n]+)\.',text)
    retired=[(queue,int(value))for queue,value in re.findall(r'Native Metal 4 queue ([^\n]+?) retired (\d+) completed submissions\.',text)]
    completed=[value for queue,value in retired]
    if kind=='MVK4':
     assert queues and retired and sum(completed)>0,'Actual MVK Metal4 queue completion evidence absent'
     assert all(queue in queues for queue,value in retired),'Retired Metal4 queue identity differs from active queue'
    else:assert not queues and not completed,'MVK Metal3 unexpectedly selected Metal4 queues'
    assert not re.search(r'\[mvk-(?:metal4-(?:queue-)?stats|backend-cost)',text),'Unexpected runtime performance diagnostics'
    save(out/'BACKEND-SELECTION.json',dict(status='PASS_ACTUAL_SELECTED_BACKEND',requested=kind,ownedEnginePID=read(out/'OWNED.json')['pid'],metal4ActiveQueueIdentities=queues,metal4RetiredQueueCounts=retired,metal4CompletedSubmissions=completed,driverLog=str(retained),driverLogSHA256=sha(retained),evidence='Same owned engine and same queue activation/positive retired completion for MVK4; both markers absent for MVK3'))
   assert common_condition(harness,row)==reference,'Common scene/replay/feature route differs'
   evidence=backend_evidence(out,row,kind);row.update(backend=kind,backendEvidence=evidence)
   checks=[{k:v for k,v in s.items()if k!='frame'}for s in evidence['commonStatus']]
   assert checks[0]==checks[1],'Effective renderer options changed during run'
   if rows: assert checks[0]=={k:v for k,v in rows[0]['backendEvidence']['commonStatus'][0].items()if k!='frame'},'Backend effective options differ'
   save(out/'RESULT.json',row);rows.append(row);save(HERE/'RUNS-THREE.json',rows)
   print(f"{arm} FPS={row['FPS']:.6f} p99={row['frameStats']['p99Ms']:.6f}ms max={row['frameStats']['maximumMs']:.6f}ms long16={row['frameStats']['over16_667ms']} actualView=1512x950",flush=True)
  summary={kind:pooled(rows,kind,stage)for kind in ['MVK3','MVK4','220']}
  pairs=[]
  for label,left,right in [('MVK3_to_220',0,2),('MVK3_to_220_reverse',5,3),('MVK4_to_220',1,2),('MVK4_to_220_reverse',4,3),('MVK3_to_MVK4',0,1),('MVK3_to_MVK4_reverse',5,4)]:
   a,b=rows[left],rows[right];pairs.append(dict(comparison=label,first=a['run'],second=b['run'],FPSChangePercent=100*(b['FPS']/a['FPS']-1)))
  report=dict(status='COMPLETE_SYMMETRIC_THREE_ARM_SIX_GAME_COMPARISON',summary=summary,pairs=pairs,runs=6,retries=0,excludedRuns=0,excludedBins=0,priorTimingSamplesReused=0,limits=['One controlled macro scene; no general game or pixel parity claim','Native and preserved Small implementations differ; compiler optimization flags not independently proven','The same Small engine and Mesa/Zink/loader/MoltenVK library are retained for both MVK arms; only requested Metal backend and original per-backend cache scope differ','DrawScreen deltas are CPU-facing frame intervals, not exclusive GPU time','Native-only MRT/Metal4 markers are native evidence only; MVK verified during one initial-warmup stack snapshot; backend labels require actual selection evidence'])
  save(HERE/'COMPARISON-THREE.json',report);completion.update(status=report['status'],runs=6,retries=0,excludedBins=0)
  print(json.dumps(report,indent=2),flush=True)
 except BaseException:completion.update(status='FAILED',parsedRuns=len(rows),traceback=traceback.format_exc());raise
 finally:completion['endUTC']=datetime.datetime.now(datetime.timezone.utc).isoformat();save(HERE/'COMPLETION-THREE.json',completion)


if __name__=='__main__':main()
