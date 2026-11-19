from pathlib import Path
import subprocess, json, hashlib, shutil, os, time, datetime
P=Path(__file__).resolve().parent
S=P/'source'
B=P.parent/'r03-r02-regression-20261117-01'
E={'round_id':'texture-ordered-trace-20261117-17','attempt_id':'texture-ordered-trace-01','commands':[],'active_pid':None,'stage':'PREFLIGHT','games':0,'GPU_helpers':0,'benchmarks':0,'captures':0,'retries':0}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def save(): (P/'EXECUTION.json').write_text(json.dumps(E,indent=2)+'\n')
def git(*a): return subprocess.check_output(['git','-C',str(S),*a])
def run(label,args,env=None):
 E['stage']=label; t=time.monotonic()
 with (P/(label+'.log')).open('w') as f:
  q=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT,env=env)
  row={'stage':label,'argv':args,'pid':q.pid,'startedUTC':datetime.datetime.now(datetime.timezone.utc).isoformat()};E['commands'].append(row);E['active_pid']=q.pid;save()
  rc=q.wait()
 row.update(exit_code=rc,wall_seconds=time.monotonic()-t);E['active_pid']=None;save();print(label,'exit',rc,'seconds',round(row['wall_seconds'],2),flush=True)
 if rc: raise RuntimeError(label+' failed; no retries')
try:
 assert git('rev-parse','HEAD').decode().strip()=='92b8b718a8cc2fe374de9a2889c8e289fc438474'
 assert git('show','-s','--format=%P','HEAD').decode().strip()=='3713df592f99b6f8c3edc91ad99803e073daa230'
 manifest=S/'docs/research/texture-trace-20261117/MANIFEST.json'
 assert sha(manifest)=='262cefbd639de63e833a7847b868a99de0aa2045e74b9d46691535e68bfe759f'
 for f in json.loads(manifest.read_bytes())['files']: assert sha(S/f['path'])==f['candidate_sha256']
 # Raw committed bytes are authoritative: repository attributes mark nine
 # unchanged CRLF vendor files dirty after a fresh checkout.
 entries=git('ls-tree','-rz','HEAD','engine').split(b'\0'); hashes={}; mismatch=[]
 for entry in entries:
  if not entry:continue
  meta,name=entry.split(b'\t',1);mode,typ,blob=meta.split();p=S/os.fsdecode(name)
  if typ!=b'blob':continue
  data=os.fsencode(os.readlink(p)) if mode==b'120000' else p.read_bytes()
  actual=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
  if actual!=blob.decode():mismatch.append(os.fsdecode(name))
  hashes[os.fsdecode(name)]=hashlib.sha256(data).hexdigest()
 assert not mismatch,mismatch
 (P/'SOURCE-HASHES.json').write_text(json.dumps(hashes,sort_keys=True,indent=2))
 (P/'SOURCE-VERIFICATION.json').write_text(json.dumps({'head':git('rev-parse','HEAD').decode().strip(),'parent':git('show','-s','--format=%P','HEAD').decode().strip(),'raw_committed_files':len(hashes),'raw_mismatches':mismatch,'git_status_raw':git('status','--porcelain').decode(),'status_note':'Nine CRLF vendor files are raw-byte-identical to committed blobs; attribute normalization alone reports dirty.'},indent=2))
 for name in ['initial-cache.cmake','UPSTREAM-CONTRIBUTIONS.json']:shutil.copy2(B/name,P/name)
 shutil.copy2(manifest,P/'MANIFEST.json')
 assert sha(P/'initial-cache.cmake')=='4ce54ccfdf0a1f8f0883c1f60c10564e9b198936b819675c3aad486cdda72115'
 tool={'compiler':subprocess.check_output(['xcrun','clang++','--version'],text=True),'SDK':subprocess.check_output(['xcrun','--sdk','macosx','--show-sdk-version'],text=True).strip()}
 assert tool['SDK']=='27.2' and 'clang version 21.0.0' in tool['compiler']
 (P/'TOOLCHAIN.json').write_text(json.dumps(tool,indent=2))
 run('CPU-CONTRACT',['python3','-B',str(S/'docs/research/texture-trace-20261117/tests/run.py')])
 E['CPU_binary_SHA256s']={n:sha(S/('docs/research/texture-trace-20261117/tests/'+n)) for n in ['live','nonlive']};save()
 env=os.environ.copy();env['PKG_CONFIG_PATH']='/opt/homebrew/opt/openal-soft/lib/pkgconfig'
 run('CONFIGURE',['cmake','-S',str(S/'engine'),'-B',str(P/'build'),'-G','Ninja','-C',str(P/'initial-cache.cmake'),'-DCMAKE_BUILD_TYPE=RelWithDebInfo'],env)
 compile_rows=json.loads((P/'build/compile_commands.json').read_text()); live_rows=[r for r in compile_rows if r['file'].endswith('Metal4WorldRenderer.mm')];assert len(live_rows)==1 and '-DRECOIL_METAL4_LIVE_SCENE' in live_rows[0]['command'];(P/'LIVE-SCENE-COMPILE.json').write_text(json.dumps(live_rows,indent=2)+'\n')
 run('BUILD',['cmake','--build',str(P/'build'),'--target','spring-metal4-scene','--','-j','8'],env)
 binaries=[p for p in (P/'build').rglob('spring-metal4-scene') if p.is_file()]
 assert len(binaries)==1,binaries
 E['raw_engine_path']=str(binaries[0]);E['raw_engine_sha256']=sha(binaries[0])
 after={name:sha(S/name) for name in hashes};assert after==hashes
 E['source_unchanged_after_build']=True;E['stage']='COMPILE_PASS';save()
except Exception as exc:
 E['stage']='FAILED_STOPPED';E['error']=repr(exc);save();print('STOP',repr(exc),flush=True);raise
finally:
 E['controller_finishedUTC']=datetime.datetime.now(datetime.timezone.utc).isoformat();E['active_pid']=None;save()
