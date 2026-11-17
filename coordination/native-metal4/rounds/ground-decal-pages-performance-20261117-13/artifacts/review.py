from pathlib import Path
import json,re,statistics,hashlib,subprocess,csv,shutil
E=Path(__file__).resolve().parent;P=E.parents[1];C=P/'measurement/runs/gdp_clean01/clean_paced'
rows=json.loads((C/'RUNS.json').read_text());assert len(rows)==6
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(n,x):(E/n).write_text(json.dumps(x,indent=2)+'\n')
world=[];witness=[];images=[];pids=[];individual=[]
for r in rows:
 arm=r['run'];Q=C/'preparation/runs'/arm;info=(Q/'infolog.txt').read_text();engine=(Q/'engine.log').read_text();plan=json.loads((Q/'LAUNCH.json').read_text());cleanup=json.loads((Q/'CLEANUP.json').read_text());ultra=r['ultra']
 assert cleanup['normalExit'] and cleanup['ownedGroupGone'];pids.append(cleanup['pid'])
 assert 'Metal API Validation Enabled' not in engine
 errors=[l for l in engine.splitlines() if re.search(r'failed assertion|MTLDebug.*(?:error|assert)|(?:hazard|residency).*(?:error|fault)|(?:error|fault).*(?:hazard|residency)',l,re.I)];assert not errors
 geom=re.findall(r'SDL logical window ([^\n]+)',info);final=re.findall(r'\[NativeFinalColorStage\] ([^\n]+)',info)
 assert geom and all(s=='1512x982, safe view 1512x950 at 0,0, mouseTopOffset=32; native render 1512x982' for s in geom)
 assert final and all(s=='active=1 samples=1 present_order_original=1 width=1512 height=982' for s in final)
 assert '[NativeGroundDecalIndexG01]' not in engine
 assert json.loads((Q/'GRASS-BOUNDARY.json').read_text())['status']=='POSITIVE_SPLIT'
 for key in ['MTL_DEBUG_LAYER','MTL_SHADER_VALIDATION','BAR_NATIVE_GRASS_BOUNDARY_S01_PROBE','BAR_NATIVE_GROUND_DECAL_INDEX_G01_PROBE','BAR_NATIVE_FRAME_TRACE']:assert plan['env'][key]=='0'
 sample=[{k:s[k] for k in ['start_simulation','end_simulation','speed_a','speed_b','paused','units','camera']} for s in r['samples']];assert len(sample)==40 and sample[0]['start_simulation']=='540' and sample[-1]['end_simulation']=='1740';world.append(sample)
 assert ultra['officialCreated']==600 and ultra['officialDestroyed']==433 and ultra['actualMainSamples']==1
 initial=re.findall(r'\[BattleInitialState\] ([^\n]+)',info);visible=re.findall(r'\[BattleVisibility\] ([^\n]+)',info);anchors=re.findall(r'\[MatchedAnchor\] ([^\n]+)',info);image_frame=re.findall(r'\[H1ValidationFrame\] ([^\n]+)',info)
 assert len(initial)==1 and any('frame=540 ' in s and 'visible_units=168' in s for s in visible) and any('sim=1740 ' in s for s in image_frame)
 script=(Q/'actual-start-script.txt').read_text();assert 'fixedrngseed=154658555;' in script
 witness.append({'arm':arm,'initial':initial,'visibility':visible,'anchors':anchors,'image_frame':image_frame,'camera':ultra['camera'],'geometry':geom,'NativeFinalColorStage':final,'fixedrngseed':154658555,'benchmark_seed_basis':'Unchanged pinned controller and official benchmark inputs; no new gameplay observer.','created':600,'destroyed':433,'cleanup':cleanup})
 image=Q/'scene.png';images.append({'arm':arm,'path':str(image),'sha256':sha(image),'bytes':image.stat().st_size,'actual_pixels_viewed':True,'uploaded':False})
 f=[float(s['interval_fps']) for s in r['samples']]
 individual.append({'arm':arm,'backend':ultra['backend'],'mean_FPS':r['FPS'],'one_percent_low':r['frameStats']['slow1PercentFPS'],'counted_seconds':r['seconds'],'bins':40,'bin_min_FPS':min(f),'bin_max_FPS':max(f),'first10_bin_FPS_mean':statistics.mean(f[:10]),'last10_bin_FPS_mean':statistics.mean(f[-10:]),'valid_and_retained':True})
assert all(w==world[0] for w in world)
for key in ['initial','camera','visibility']:
 assert all(w[key]==witness[0][key] for w in witness),key
summary={}
for backend in ['baseline','candidate']:
 rs=[r for r in individual if r['backend']==backend];fps=[r['mean_FPS'] for r in rs];low=[r['one_percent_low'] for r in rs]
 summary[backend]={'n':3,'arithmetic_mean_FPS':statistics.mean(fps),'sample_stdev_FPS':statistics.stdev(fps),'min_FPS':min(fps),'max_FPS':max(fps),'arithmetic_mean_one_percent_low':statistics.mean(low),'individual_FPS':fps,'individual_one_percent_low':low}
relative=100*(summary['candidate']['arithmetic_mean_FPS']/summary['baseline']['arithmetic_mean_FPS']-1)
controller=json.loads((E/'EXECUTION.json').read_text());owner=json.loads((C/'OWNER-CLOSED.json').read_text());awake=json.loads((C/'AWAKE-RELEASED.json').read_text());assert controller['exit_code']==0 and owner['completed']==6 and owner['awakeReleased'] and not awake['ownCaffeinateAlive'];pids.extend([controller['runner_pid'],awake['pid']])
for pid in pids:assert not subprocess.run(['ps','-p',str(pid),'-o','pid='],capture_output=True,text=True).stdout.strip(),pid
frozen=json.loads((P/'FROZEN-BEFORE-RUN.json').read_text());assert sha(P/'measurement/run.py')==frozen['runner_sha256']
for n in ['measurement/BASELINE-PRODUCT.json','measurement/PRODUCT.json']:
 product=json.loads((P/n).read_text());assert sha(Path(product['binary']))==product['sha256']
save('WORLD-CHECK.json',{'status':'ALL_40_ROWS_MATCH_ALL_SIX_RUNS','compared_keys':['start_simulation','end_simulation','speed_a','speed_b','paused','units','camera'],'rows':{r['run']:w for r,w in zip(rows,world)},'fixed_tick_witnesses':witness,'bin_boundary_jitter_in_this_cohort':False,'no_unobserved_tick_states_invented':True})
save('IMAGE-REVIEW.json',{'images':images,'observed':'All six actual RGB images viewed. No obvious candidate-only missing/corrupt static terrain/buildings/ground decal pattern; occlusion appearance similar. Animated smoke, explosions, illumination, green range arcs and diagnostic text vary. No candidate-only orange circle observed.','limits':'Not exact pixel equality or depth/stencil attachment comparison; animation causes not individually traced.'})
save('INDIVIDUAL-RUNS.json',individual)
result={'status':'COMPLETE_CLEAN_COMPARISON_CAUSAL_GAIN_UNCERTAIN','round_id':'ground-decal-pages-performance-20261117-13','attempt_id':'ground-decal-pages-clean-01','source_commit':'ef4d8af835604d40b0f501b6da9b0ccd7f18a04a','source_manifest_sha256':'f23bbd44392590029b4e4155bbb9abc7775e79bbdda51dcecb0376bfbdf47f92','runner_sha256':frozen['runner_sha256'],'baseline_signed_sha256':'107958872e5d962e4d27803a0fb1af0e842b18bd6982bd9b74e115d956890ab9','candidate_signed_sha256':'035d62084b6657d663a08e6422cdd696af0fafedc01a5189ba696619b15f20c4','metric':'CPU-observed DrawScreen frames / wall duration; not GPU or displayed FPS','order':[r['run'] for r in rows],'individual':individual,'summary':summary,'candidate_vs_baseline_percent':relative,'all_valid_slow_runs_retained':True,'historical_or_validation_samples_pooled':False,'interpretation':'Observed three-run mean favors candidate +4.03596%, driven by low first baseline56.84147. Later baseline63.98720..64.10737 overlaps candidate64.10962..64.15 closely. Cause of A1 slowdown unknown. This cohort does not isolate a reproducible +4% causal patch gain; no row/run removed and no retry.','world_rows_per_run':40,'all_40_rows_match_all_six':True,'fixed_tick_initial_camera_seed_visibility_and_end_state_match':True,'images_viewed':6,'normal_exits':6,'games':6,'staging_attempts':1,'builds':0,'validation_games':0,'helpers':0,'GPU_captures':0,'retries':0,'source_or_controller_or_execution_logic_changes':0,'cleanup':{'owned_pids_checked_absent':pids,'groups_gone':True,'image_workers_joined':True,'awake_released':True,'evidence_writers_closed_after_packet':True},'unmeasured':['Actual copy bytes','Physical allocation reduction','RAM','GPU FPS/time','Exhaustive resource lifetime/failure/quarantine/revision/context paths'],'provenance':'Existing engine GPL, Beherith MIT and tomjn/benbreen contribution records preserved. No new contribution adopted.'}
save('RESULT.json',result)
shutil.copy2(P/'UPSTREAM-CONTRIBUTIONS.json',E/'UPSTREAM-CONTRIBUTIONS.json')
with (E/'SUMMARY.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(individual[0]));w.writeheader();w.writerows(individual)
print(json.dumps({'status':result['status'],'means':summary,'relative_percent':relative,'world_match':True,'normal_exits':6,'PIDs_absent':pids}))
