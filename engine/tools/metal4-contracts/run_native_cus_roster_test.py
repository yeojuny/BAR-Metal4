#!/usr/bin/env python3
"""Compile CPU-only production CUS roster/capture/read-set code; never app/Metal build."""
import argparse, os, subprocess, pathlib, json, hashlib
p=argparse.ArgumentParser();p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--negative-control',action='store_true');a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
root=pathlib.Path(__file__).resolve().parents[2];backend=root/'rts/Rendering/Backend'
files=[backend/(name+'.cpp') for name in ['NativeCUSModelInputs','NativeCUSTextureHistory','NativeCUSPublishedData','NativeCUSUniformData','NativeCUSSourceData','NativeCUSRenderInputs']]
files+=[pathlib.Path(__file__).with_name('TestNativeCUSRoster.cpp')]
cmd=['g++','-std=c++20','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'rts'),*map(str,files),'-o',str(out/'test')]
if a.negative_control:
 overlay=out/'overlay/Rendering/Backend';overlay.mkdir(parents=True,exist_ok=True)
 header=(backend/'NativeCUSCPUReadScope.h').read_text()
 header=header.replace('reinterpret_cast<uintptr_t>(d.roster.get())','reinterpret_cast<uintptr_t>(d.modelInputs.get())')
 (overlay/'NativeCUSCPUReadScope.h').write_text(header)
 cmd[1:1]=['-I'+str(out/'overlay'),'-I'+str(backend)]
built=subprocess.run(cmd,capture_output=True,text=True);(out/'compile.log').write_text(built.stdout+built.stderr)
if built.returncode:print(built.stderr);raise SystemExit(built.returncode)
run=subprocess.run([str(out/'test')],capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'});(out/'run.log').write_text(run.stdout+run.stderr)
# Ensure every committing builder operation invalidates the cached proof, including
# each AddOne before a later item in Add(table) can throw. No Lua VM claim.
s=(root/'rts/Lua/LuaNativeCUSSource.cpp').read_text();start=s.index('struct VertexArray');s=s[start:s.index('std::map<std::string,std::vector<float>> UniformTable',start)]
for method,end in [('void AddOne','void Add('),('void ClearSubmission','void RemoveFromSubmission'),('void RemoveFromSubmission','void Delete'),('void Delete','void Submit')]:
 body=s[s.index(method):s.index(end)];assert 'roster.reset()' in body
assert s.index('objects.push_back(o)') < s.index('roster.reset()') < s.index('void Add(')
assert 'if(!roster)roster=NativeCUSSubmissionRoster::Seal' in s
receipt={'scope':__doc__,'negativeControl':a.negative_control,'command':cmd,'exit':run.returncode,'summary':run.stdout,'sourceSHA256':{str(f.relative_to(root)):hashlib.sha256(f.read_bytes()).hexdigest() for f in files},'builderSourceChecks':6,'notTested':['Lua VM integration','Metal Objective-C++','Mac app build','GPU gameplay/performance']}
(out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n');print(run.stdout+run.stderr);raise SystemExit(run.returncode)
