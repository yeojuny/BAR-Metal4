from pathlib import Path
import json,subprocess,datetime,time,sys
P=Path(__file__).resolve().parent
request=json.loads((P/'REQUEST.json').read_text())
record={'round_id':request['round_id'],'attempt_id':request['attempt_id'],'argv':request['command'],'startedUTC':datetime.datetime.now(datetime.timezone.utc).isoformat(),'invocations':1,'games_budget':2,'retries':0}
t=time.monotonic()
with (P/'RUN.log').open('w') as log:
 q=subprocess.Popen(request['command'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,bufsize=1)
 record['runner_pid']=q.pid
 (P/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
 for line in q.stdout:
  log.write(line);log.flush();print(line,end='',flush=True)
 rc=q.wait()
record.update(exit_code=rc,wall_seconds=time.monotonic()-t,finishedUTC=datetime.datetime.now(datetime.timezone.utc).isoformat(),runner_reaped=True)
(P/'EXECUTION.json').write_text(json.dumps(record,indent=2)+'\n')
print('CONTROLLER_EXIT',rc,flush=True)
sys.exit(rc)
