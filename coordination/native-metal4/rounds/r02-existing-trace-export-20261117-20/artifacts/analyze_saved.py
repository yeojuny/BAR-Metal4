from pathlib import Path
import json,re,hashlib,shutil,collections,plistlib
P=Path(__file__).resolve().parent
OLD=P.parent/'r02-g01-regression-20261116-01/measurement/capture_r02_01'
A=OLD/'analysis'
def save(name,obj):
 q=P/name;q.parent.mkdir(parents=True,exist_ok=True);q.write_text(json.dumps(obj,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names=['command-tree','command-buffer','api-calls','resources','render-pipelines','encoder-costs','shader-costs','encoder-intervals','shader-stage-intervals']
raw={};inventory=[]
for n in names:
 q=A/(n+'.jsonl');target=P/'raw/saved-export'/q.name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(q,target)
 objects=[json.loads(l) for l in q.read_text().splitlines() if l.strip()]
 raw[n]=max(objects,key=lambda x:len(x.get('children',[])))
 inventory.append({'path':str(q),'copy':str(target),'bytes':q.stat().st_size,'sha256':sha(q),'JSON_objects':len(objects),'full_children':len(raw[n].get('children',[]))})
save('SAVED-EXPORT-INDEX.json',inventory)
shutil.copy2(A/'REPLAY-RESULT.json',P/'raw/PRIOR-REPLAY-RESULT.json')
def vals(row):return [v.get('value') if isinstance(v,dict) else v for v in row['values']]
def label(row):return str(vals(row)[0]).strip('"')
def api_text(row):return str(vals(row)[-1])
def number(row):return int(row['name'][3:])
api=raw['api-calls']['children'];nodes=raw['command-buffer']['children'];segments=[];active=None
for row in api:
 t=api_text(row)
 if 'renderCommandEncoderWithDescriptor:' in t or 'MTL4CommandBuffer computeCommandEncoder:' in t:
  active={'begin_api':number(row),'type':'render' if 'renderCommandEncoder' in t else 'compute','rows':[]};segments.append(active)
 if active:active['rows'].append(row)
 if 'endEncoding]' in t and active:active['end_api']=number(row);active=None
assert len(segments)==len(nodes)==52
selected_labels={f'MTL4RenderCommandEncoder {n}' for n in list(range(9,13))+list(range(33,43))}
perf_by_label={label(r):r for r in raw['encoder-costs']['children']}
interval_by_label={label(r):r for r in raw['encoder-intervals']['children']}
maps=[];api_owner={};pipeline_owners=collections.defaultdict(list);draws=[];barriers=[];dss_bindings=[]
stage_bits={'Vertex':1,'Fragment':2,'Dispatch':1<<27,'Blit':1<<28}
sdk=Path('/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/Metal.framework/Headers/MTLCommandEncoder.h')
save('SDK-STAGE-MASKS.json',{'source':str(sdk),'source_sha256':sha(sdk),'verified_definitions':stage_bits,'scope':'Only these four flags decoded; unknown bits remain explicit.'})
def decode(s):
 if not s.isdigit():return {'raw':s,'names':[s],'verified_numeric_conversion':False}
 n=int(s);known=0
 for v in stage_bits.values():known|=v
 return {'raw':s,'names':[k for k,v in stage_bits.items() if n&v],'unknown_bits':n&~known}
for ordinal,(seg,node) in enumerate(zip(segments,nodes)):
 lab=label(node);assert node['name'].startswith('re' if seg['type']=='render' else 'ce')
 count=sum(bool(re.search(r' draw(?:Indexed)?Primitives:',api_text(r))) for r in seg['rows'])
 expected=re.search(r'(\d+) draws?',str(vals(node)[1]));expected=int(expected[1]) if expected else 0
 if seg['type']=='render':assert count==expected,(lab,count,expected)
 perf=perf_by_label.get(lab);intr=interval_by_label.get(lab)
 record={'command_ordinal':ordinal,'command_node':node['name'],'label':lab,'type':seg['type'],'performance_node':perf['name'] if perf else None,'timeline_node':intr['name'] if intr else None,'API_begin':seg['begin_api'],'API_end':seg['end_api'],'render_draw_count':count,'command_tree_render_draw_count':expected if seg['type']=='render' else None,'selected':lab in selected_labels,'association':'Command/performance/timeline linked by exact saved label; API segment associated by creation order, encoder type and all42 render draw-count checks. No per-call receiver handle exposed.'}
 maps.append(record);pso=None;dss=None
 for r in seg['rows']:
  api_owner[number(r)]=record;t=api_text(r)
  m=re.search(r'setRenderPipelineState:@(rps\d+)',t)
  if m:pso=m[1]
  m=re.search(r'setDepthStencilState:@(dss\d+)',t)
  if m:
   dss=m[1];dss_bindings.append({'api':r['name'],'command_node':node['name'],'label':lab,'state':dss,'raw':t})
  if 'barrier' in t:
   masks={k:decode(v) for k,v in re.findall(r'(afterQueueStages|beforeStages|afterStages|beforeQueueStages|afterEncoderStages|beforeEncoderStages):([^\s\]]+)',t)}
   barriers.append({'api':r['name'],'command_node':node['name'],'label':lab,'raw':t,'stage_masks':masks})
  if re.search(r' draw(?:Indexed)?Primitives:',t):
   draws.append({'api':r['name'],'command_node':node['name'],'label':lab,'pipeline':pso,'depth_stencil_state':dss,'raw':t})
   if pso and lab not in pipeline_owners[pso]:pipeline_owners[pso].append(lab)
 if lab in selected_labels:
  save('encoder-api/'+lab.replace(' ','_')+'.json',{'mapping':record,'raw_API_rows':seg['rows'],'attachments_load_store_resolve':'UNAVAILABLE: descriptor arguments omitted by saved API listing.','actual_resource_read_write_and_last_writer':'UNAVAILABLE beyond explicit copies/draw index buffers; argument table receiver unresolved and numeric texture IDs not mapped to trace objects.'})
save('ENCODER-ID-MAP.json',{'all':maps,'render_draw_count_checks':42,'render_draw_count_mismatches':0,'performance_IDs_not_command_IDs':True})
save('BARRIERS.json',barriers)
save('DRAW-PIPELINE-DSS.json',draws)
save('DEPTH-STENCIL-12-15.json',{'descriptor_state':'UNAVAILABLE','reason':'Resource root only lists19 objects; no depth-stencil children/info export. Saved API has only bindings, no creation descriptor. No new inspection session with unproved zero-GPU-execution semantics.','bindings':[r for r in dss_bindings if r['state'] in ['dss12','dss15']],'draws_using_states':[r for r in draws if r['depth_stencil_state'] in ['dss12','dss15']],'do_not_infer_compare_write_mask_ops_from_reference_value':True})
copies=[];views=[];resource_calls=[]
for r in api:
 t=api_text(r);owner=api_owner.get(number(r));context={'api':r['name'],'command_node':owner['command_node'] if owner else None,'label':owner['label'] if owner else None,'raw':t}
 m=re.search(r'copyFromTexture:@(tex\d+).*?toTexture:@(tex\d+)',t)
 if m:copies.append(dict(context,source=m[1],destination=m[2],access='command-semantic source read/destination write; no inferred attachments or physical access counter'))
 m=re.search(r'\[@(tex\d+) newTextureView',t)
 if m:
  result=next((v for v in vals(r)[:-1] if isinstance(v,str) and re.fullmatch(r'@tex\d+',v)),None)
  views.append(dict(context,parent=m[1],view=result.lstrip('@') if result else None))
 if any(k in t for k in ['setTexture:','copyFromTexture:','newTextureView','setDepthStencilState:','barrierAfter']):
  resource_calls.append(dict(context,interpretation='Binding/call only. setTexture numeric value is not automatically @tex ordinal; residency/aliasable query is not access or alias proof.'))
save('TEXTURE-COPY-LINEAGE.json',{'explicit_texture_copies':copies,'captured_texture_views':views,'tex99_to_tex2':[r for r in copies if r['source']=='tex99' and r['destination']=='tex2'],'tex99_or_tex2_API_mentions':[dict(api=r['name'],raw=api_text(r)) for r in api if re.search(r'@tex(?:99|2)\b',api_text(r))],'tex99_upstream_full_last_writer':'UNAVAILABLE: render attachment descriptors not saved.','tex2_last_explicit_copy_writer':'api10982, final compute command node ce51; full aliased resource history unavailable.','presentation_association':'Commit/signalDrawable/present follow final copy; exact drawable texture identity not exposed.'})
save('COMMAND-RESOURCE-TIMELINE.json',resource_calls)
stage=[]
for r in raw['shader-stage-intervals']['children']:
 values=vals(r);m=re.search(r'@(rps\d+)',str(values[1]));pso=m[1] if m else None;owners=pipeline_owners.get(pso,[])
 if not any(v in selected_labels for v in owners):continue
 stage.append({'saved_stage_node':r['name'],'shader_name':values[0],'pipeline':pso,'possible_encoder_labels':owners,'attribution_ambiguous_multiple_encoders':len(owners)!=1,'saved_start_raw':values[2],'saved_end_raw':values[3],'saved_trailing_field_uninterpreted':values[4] if len(values)>4 else None,'raw_record':r})
save('RECORDED-STAGE-RANGES.json',{'source':'Previously saved profile session120 default/overlapping, no embed. This turn performs no profiling.','records':stage,'unit_status':'Saved JSON says type=duration but omits explicit unit/column schema. Preserve unscaled raw values; prior notes interpreted ns, not independently established by a supported schema this turn.','interval_semantics':'One bounds pair and trailing field per shader record; individual constituent active intervals are not exported. Do not treat bounds intersection as measured continuous overlap or sum bounds.','individual_active_intervals':'UNAVAILABLE','last_field_semantics':'UNAVAILABLE in saved export; not treated as draw count.'})
save('RECORDED-ENCODER-RANGES.json',{'source':'Prior profile only, not live runtime','records':[dict(mapping=m,saved_timeline=interval_by_label.get(m['label']),saved_cost=perf_by_label.get(m['label'])) for m in maps if m['selected']],'unit_status':'Raw duration values preserved; cost string suffixes ms explicit, individual metric column meanings not retained.','zero_start_full_extent_warning':'Label39 has zero draws and full0..24919083 bounds; do not interpret as actual full-frame GPU occupation.'})
trace=Path(json.loads((P/'TRACE-PIN-BEFORE.json').read_text())['trace']);save('CAPTURE-METADATA.json',plistlib.loads((trace/'metadata').read_bytes()))
save('AVAILABILITY.json',{'encoder_ID_map':'EXPORTED52, selected14; exact label join and42 draw checks','selected_API_slices':'EXPORTED14','stage_bounds':'EXPORTED prior saved records only, not runtime','individual_active_stage_intervals':'UNAVAILABLE; prior no-embed profile session closed, only bounds saved','render_attachments_load_store_resolve':'UNAVAILABLE; saved creation arguments omitted','complete_resource_read_write_last_writer':'UNAVAILABLE; copy/index command semantics and raw bindings only','dss12_dss15_full_state':'UNAVAILABLE; binding and draw use exported','tex99_to_tex2_copy':'EXPORTED api10982 ce51; upstream attachment writer unknown','shader_instruction_register_counters':'UNAVAILABLE in saved exports; no new profiling','new_inspection_sessions':0,'session_decision':'gpudebug help exposes replay device/session but no documented no-GPU/read-only load option; prior open status showed replayer loading. No -t open issued under zero execution rule. Does not claim opening necessarily submits GPU; guarantee unavailable.'})
print(json.dumps({'mapped_encoder_nodes':len(maps),'selected':sum(m['selected'] for m in maps),'draw_checks':42,'stage_range_records':len(stage),'explicit_texture_copies':len(copies),'views':len(views),'sessions_opened':0,'replay_profile_game_builds':0}))
