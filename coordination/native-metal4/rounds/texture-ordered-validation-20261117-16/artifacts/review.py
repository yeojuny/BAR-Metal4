from pathlib import Path
import json,re,hashlib,subprocess
E=Path(__file__).resolve().parent
P=E.parents[1]
C=P/'measurement/runs/texture_validation01/validation_paced'
world=[];raw=[];images=[];pids=[]
for arm in ['V_TEXTURE_OFF','V_TEXTURE_ON']:
 Q=C/'preparation/runs'/arm
 info=(Q/'infolog.txt').read_text();engine=(Q/'engine.log').read_text()
 validation=json.loads((Q/'METAL-VALIDATION.json').read_text());cleanup=json.loads((Q/'CLEANUP.json').read_text());ultra=json.loads((Q/'ULTRA-EVIDENCE.json').read_text())
 rows=[]
 for line in info.splitlines():
  if '[MatchedFPS]' in line:
   d=dict(re.findall(r'(\w+)=([^\s]+)',line));rows.append({k:d[k] for k in ['start_simulation','end_simulation','speed_a','speed_b','paused','units','camera']})
 world.append(rows)
 geometry=re.findall(r'SDL logical window ([^\n]+)',info)
 final=re.findall(r'\[NativeFinalColorStage\] ([^\n]+)',info)
 errors=[l for l in (engine+'\n'+info).splitlines() if re.search(r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)',l,re.I)]
 assert validation['enabledByEngineLog'] and not errors and cleanup['normalExit'] and cleanup['ownedGroupGone']
 assert geometry and all(r=='1512x982, safe view 1512x950 at 0,0, mouseTopOffset=32; native render 1512x982' for r in geometry)
 assert final and all(r=='active=1 samples=1 present_order_original=1 width=1512 height=982' for r in final)
 assert len(rows)==40 and rows[0]['start_simulation']=='540' and rows[-1]['end_simulation']=='1740'
 assert ultra['officialCreated']==600 and ultra['officialDestroyed']==433
 g=json.loads((Q/'GROUND-INDEX.json').read_text());s=json.loads((Q/'GRASS-BOUNDARY.json').read_text());assert g['status']=='POSITIVE_INDEXED' and s['status']=='POSITIVE_SPLIT'
 raw.append({'arm':arm,'metal':validation,'geometry':geometry,'NativeFinalColorStage':final,'ultra':ultra,'S01':s['status'],'G01':g['status'],'cleanup':cleanup,'retirement_witness':[l for l in info.splitlines() if '[NativeRecord158]' in l],'seed_script_fields':re.findall(r'(?im)^\s*[^\n;]*seed[^\n;]*;',(Q/'actual-start-script.txt').read_text())})
 image=Q/'scene.png';images.append({'arm':arm,'path':str(image),'sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'bytes':image.stat().st_size,'actual_pixels_viewed':True,'uploaded':False});pids.append(cleanup['pid'])
differences=[{'row_zero_based':i,'fields':{k:[a[k],b[k]] for k in a if a[k]!=b[k]}} for i,(a,b) in enumerate(zip(*world)) if a!=b]
for key in ['initialState','camera','officialCreated','officialDestroyed']:assert raw[0]['ultra'][key]==raw[1]['ultra'][key]
controller=json.loads((E/'EXECUTION.json').read_text());owner=json.loads((C/'OWNER-CLOSED.json').read_text());awake=json.loads((C/'AWAKE-RELEASED.json').read_text());assert controller['exit_code']==0 and owner['completed']==2 and owner['awakeReleased'] and not awake['ownCaffeinateAlive']
pids.extend([awake['pid'],controller['runner_pid']])
for pid in pids:assert not subprocess.run(['ps','-p',str(pid),'-o','pid='],capture_output=True,text=True).stdout.strip(),pid
def save(n,v):(E/n).write_text(json.dumps(v,indent=2)+'\n')
save('RAW-CHECK.json',{'status':'NO_METAL_ERRORS_TWO_NORMAL_EXITS','arms':raw,'retirement_limit':'Normal exit, cleanup and prior_retired_before_submit witness observed; no per-event completion tracing or exhaustive resource lifetime proof.'})
save('WORLD-CHECK.json',{'status':('MATCHED_40_ROWS' if not differences else 'BIN_JITTER_REVIEW_REQUIRED'),'rows_per_arm':40,'exactly_matching_rows':40-len(differences),'differences':differences,'baseline_rows':world[0],'candidate_rows':world[1],'initial_camera_created_destroyed_match':True,'reason_for_bin_difference':'UNKNOWN; first-row unit counts are sampled at different simulation times, not proof of divergent gameplay. No retry.'})
save('IMAGE-REVIEW.json',{'images':images,'observed':'Both actual images viewed. No obvious new missing/corrupt static terrain, buildings or ground-decal pattern. Occlusion appearance similar. Explosions/smoke/illumination and diagnostic numbers differ. Green range/sensor arcs visible in both; no candidate-only orange ring observed.','limits':'RGB screenshots do not prove exact pixel or depth/stencil attachment equality; dynamic differences not causally isolated.'})
result={'status':'PASS_ORDINARY_OFF_ON_CONFIGS_TEXTURE_PATH_UNKNOWN','round_id':'texture-ordered-validation-20261117-16','attempt_id':'texture-ordered-validation-01','source_commit':'3713df592f99b6f8c3edc91ad99803e073daa230','source_manifest_sha256':'3b7c8a27d3a9a6d1c9ee4d248a25c021e55256c283027c829a8bd6c7b8b71de7','input_artifact_commit':'544c23c4214af99efe67c98f047e6325fe6fc9c2','runner_sha256':hashlib.sha256((P/'measurement/run.py').read_bytes()).hexdigest(),'OFF_signed_sha256':'ad32896dab1e403f7e007e1fa51d4c8532b095049d86d672ace756337a1829d3','ON_signed_sha256':'ad32896dab1e403f7e007e1fa51d4c8532b095049d86d672ace756337a1829d3','games':2,'baseline_games':1,'candidate_games':1,'Metal_API_Validation_observed_both':True,'matched_Metal_errors':0,'normal_exits':2,'actual_main_samples':1,'actual_safe_view':[1512,950],'actual_drawable':[1512,982],'world_rows_per_arm':40,'exactly_matching_world_rows':40-len(differences),'world_differences':differences,'created':600,'destroyed':433,'performance_accepted':False,'images_viewed':2,'source_or_runner_changes':0,'retries':0,'builds':0,'helpers':0,'clean_games':0,'GPU_captures':0,'cleanup':{'owned_pids_checked_absent':pids,'owned_groups_gone':True,'image_workers_joined':True,'awake_released':True,'writers_closed_after_packet':True},'limits':['First two sample bin boundaries differ; cause unknown. Preserve all rows, no retry or FPS decision.','Ordinary API Validation/normal exit is not proof of all skipped/reversed revision, failed submission, quarantine, context-change or shutdown-error paths.','No per-resource event completion tracing added.','Actual RGB screenshots viewed without obvious static regression; no exact pixel/depth/stencil equivalence claim.','No actual page-reuse/copy/alloc/RAM benefit measured.']}
child_proofs=[]
for arm,value in [('V_TEXTURE_OFF','0'),('V_TEXTURE_ON','1')]:
 Q=C/'preparation/runs'/arm
 child=json.loads((Q/'CHILD-MODE.json').read_text())
 assert child['status']=='PASS_ACTUAL_CHILD_ENVIRONMENT' and child['actual']['BAR_NATIVE_TEXTURE_ORDERED_INPUT']==value and child['expected']['BAR_NATIVE_TEXTURE_ORDERED_INPUT']==value
 child_proofs.append({'arm':arm,'pid':child['pid'],'actual_texture_flag':value,'receipt':'CHILD-MODE.json','before_counted_window':child['beforeCountedWindow']})
raw_path_evidence=[]
for arm in ['V_TEXTURE_OFF','V_TEXTURE_ON']:
 Q=C/'preparation/runs'/arm;log=(Q/'engine.log').read_text()
 raw_path_evidence.append({'arm':arm,'generic_world_borrow_lines':[l for l in log.splitlines() if '[NativeWorldBorrow168]' in l],'why_insufficient':'Existing WorldOverlayRenderer.mm:737 preparation-only overlay command validation marker; not texture producer/context/consumer or OrderAfterPreviousSubmission admission.'})
path={'ON_path_classification':'UNKNOWN','OFF_flag':'DISABLED','positive_texture_admission_witness':False,'comprehensive_proof_unused':False,'do_not_classify_NOT_EXERCISED_from_absent_marker':True,'source_before_review':json.loads((P/'PATH-EVIDENCE-BEFORE.json').read_text()),'existing_generic_logs':raw_path_evidence,'new_instrumentation':False}
save('CHILD-ENV-CHECK.json',{'status':'ACTUAL_CHILD_OFF0_ON1_CONFIRMED','arms':child_proofs})
save('PATH-EVIDENCE-AFTER.json',path)
result.update(actual_child_flag_OFF0_ON1=True,ON_path_classification='UNKNOWN',optimization_exercised_or_correctness_approval=False,positive_texture_admission_witness=False,performance_accepted=False)
save('RESULT.json',result)
print(json.dumps({'status':result['status'],'normal_exits':2,'Metal_errors':0,'exact_world_rows':result['exactly_matching_world_rows'],'differences':differences,'owned_PIDs_absent':pids,'seed_fields':[r['seed_script_fields'] for r in raw]}))
