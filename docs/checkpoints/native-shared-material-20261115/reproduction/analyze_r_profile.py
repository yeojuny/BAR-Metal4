#!/usr/bin/env python3
"""Offline all-thread analysis of P2's exported Time Profiler point samples."""
import json
import gzip
import os
from collections import Counter, defaultdict
from pathlib import Path
from xml.etree.ElementTree import iterparse

HERE=Path(__file__).resolve().parent
RUN=HERE.parent/'evidence/R/measurement/p2/cus_terrain_profile_p2/preparation/runs/P2_Q_CUS0'
PID=str(json.loads((RUN/'OWNED.json').read_text())['pid'])
SCOPES={
 'DrawPrepared':'Metal4WorldRenderer::DrawPrepared(',
 'PrepareCUSDeferred':'PrepareCUSDeferred(',
 'TerrainSourceGPU::Prepare':'TerrainSourceGPU::Prepare(',
 'CUSSourceGPU::Prepare':'CUSSourceGPU::Prepare(',
 'OriginalTerrainImage':'Metal4WorldRenderer::Impl::OriginalTerrainImage(',
 'OriginalModelImage':'Metal4WorldRenderer::Impl::OriginalModelImage(',
 'TerrainSampler':'TerrainSourceGPU::Impl::Sampler(',
 'TerrainUniformUpload':'TerrainUniformBatch::Upload(',
 'ModelMaterialsGet':'ModelMaterials156::Get<',
 'PrepareOpaqueModelSource':'PrepareOpaqueModelSource(',
 'PrepareOriginalShadowSource':'PrepareOriginalShadowSource(',
}

def main():
    ids={};columns={};node=None
    states=Counter();threads=Counter();total=0;symbolized=0
    scopes=defaultdict(lambda: {'count':0,'weightNs':0,'threads':Counter(),
                                'leaf':Counter(),'leafWeightNs':Counter(),'frames':Counter()})
    all_running_leaf=Counter();interesting_stacks=Counter();scope_overlaps=Counter()
    def value(e):
        key=e.get('id') or e.get('ref')
        return ids.get(key) if key else None
    for event,elem in iterparse(gzip.open(HERE.parent/'evidence/R/outputs/offline-profile-p2/time-profile.xml.gz','rb'),('start','end')):
        if event=='start':
            if elem.tag=='node':node=elem
            continue
        if elem.tag=='schema':
            columns={c.findtext('mnemonic'):i for i,c in enumerate(elem)}
            node.remove(elem)
        if elem.tag!='row':continue
        for child in reversed(list(elem.iter())):
            if not (key:=child.get('id')):continue
            if child.tag=='tagged-backtrace':ids[key]=tuple(value(f) for f in child.findall('frame'))
            elif child.tag=='frame':ids[key]=child.get('name') or child.get('addr') or '(unknown)'
            elif child.tag=='process':ids[key]=value(child.find('pid'))
            else:ids[key]=child.get('fmt') if child.tag=='thread' else child.text
        def col(name):return value(elem[columns[name]])
        if col('process')==PID:
            total+=1;state=col('thread-state') or '(unknown)';states[state]+=1
            thread=col('thread') or '(unknown)';threads[thread]+=1
            frames=tuple(x for x in (col('stack') or ()) if x)
            if frames and not frames[0].startswith(('0x','(unknown)')):symbolized+=1
            if state=='Running' and frames:
                leaf=frames[0];weight=int(col('weight') or 0)
                all_running_leaf[leaf]+=1
                matched=[label for label,needle in SCOPES.items() if any(needle in x for x in frames)]
                for i,a in enumerate(matched):
                    for b in matched[i+1:]:scope_overlaps[a+' & '+b]+=1
                for label,needle in SCOPES.items():
                    if label not in matched:continue
                    s=scopes[label];s['count']+=1;s['weightNs']+=weight
                    s['threads'][thread]+=1;s['leaf'][leaf]+=1;s['leafWeightNs'][leaf]+=weight
                    s['frames'].update(dict.fromkeys(frames,1))
                    if label=='DrawPrepared' and any(t in leaf for t in ('CUS','Terrain','Sampler','Binding','Texture','Shader','Program')):
                        interesting_stacks[(leaf,tuple(x for x in frames[1:10] if any(t in x for t in ('CUS','Terrain','Sampler','Binding','Texture','Shader','Program','DrawPrepared'))))]+=1
        node.remove(elem)
    assert total>0 and scopes['DrawPrepared']['count']>0
    def top(c,n=30):return [{'symbol':k,'samples':v} for k,v in c.most_common(n)]
    out={'source_trace':str(RUN/'time-profiler.trace'),'source_xml':str(HERE.parent/'evidence/R/outputs/offline-profile-p2/time-profile.xml.gz'),
         'owned_pid':int(PID),'owned_rows':total,'states':dict(states),
         'leaf_symbolized_rows':symbolized,'leaf_symbol_coverage':symbolized/total,
         'thread_sample_counts':dict(threads),
         'method':'All threads; Running samples only. Leaf is first frame; ancestor scopes are inclusive and overlap. Sample weight is statistical, not measured CPU duration.',
         'scope_intersection_samples':dict(scope_overlaps),
         'scopes':{label:{'inclusive_running_samples':v['count'],
                          'inclusive_weight_statistical_ms':v['weightNs']/1e6,
                          'self_leaf_samples':sum(n for leaf,n in v['leaf'].items() if SCOPES[label] in leaf),
                          'threads':dict(v['threads']),
                          'top_leaf_exclusive_sample_counts':top(v['leaf'],45),
                          'top_inclusive_ancestor_sample_counts':top(v['frames'],45)}
                   for label,v in scopes.items()},
         'all_running_top_leaf':top(all_running_leaf,40),
         'interesting_draw_prepared_stacks':[
             {'leaf':leaf,'selected_ancestors':ancestors,'samples':count}
             for (leaf,ancestors),count in interesting_stacks.most_common(40)]}
    Path(os.environ.get('ANALYSIS_OUT',str(HERE/'R-REANALYSIS.json'))).write_text(json.dumps(out,indent=2)+'\n')
    print(json.dumps({'owned_rows':total,'states':dict(states),'symbol_coverage':symbolized/total,
      'scopes':{k:{'samples':v['count'],'weightStatisticalMs':v['weightNs']/1e6,
                   'topLeaf':top(v['leaf'],12)} for k,v in scopes.items()}},indent=2))

if __name__=='__main__':main()
