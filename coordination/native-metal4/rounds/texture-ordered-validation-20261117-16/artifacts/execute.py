from pathlib import Path
import subprocess,json,time,datetime,sys,hashlib
E=Path(__file__).resolve().parent
P=E.parents[1]
frozen=json.loads((P/'FROZEN-BEFORE-RUN.json').read_text())
runner=P/'measurement/run.py'
assert hashlib.sha256(runner.read_bytes()).hexdigest()==frozen['runner_sha256']
args=['python3','-B',str(runner),'--suite','texture_validation01','--run','validation_paced']
record={'round_id':'texture-ordered-validation-20261117-16','attempt_id':'texture-ordered-validation-01','argv':args,'runner_sha256':frozen['runner_sha256'],'startedUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'invocations':1,'games_budget':2,'retries':0}
t=time.monotonic()
with (E/'RUN.log').open('w') as log:
 q=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
 record['runner_pid']=q.pid
 (E/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
 for line in q.stdout:
  log.write(line);log.flush();print(line,end='',flush=True)
 rc=q.wait()
record.update(exit_code=rc,wall_seconds=time.monotonic()-t,finishedUTC=datetime.datetime.now(datetime.timezone.utc).isoformat(),runner_reaped=True)
(E/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
print('CONTROLLER_EXIT',rc,flush=True)
sys.exit(rc)
