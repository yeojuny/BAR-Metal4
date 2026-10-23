#!/usr/bin/env python3
"""Translate the exact original test30770 Lightning Cannon / Environmental Lightning expressions and contracts."""
import argparse,hashlib,json,re
from pathlib import Path

def fnv(text):
    h=14695981039346656037
    for b in text.encode(): h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cannon',type=Path,required=True);p.add_argument('--environment',type=Path,required=True)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args()
    sources=[a.cannon.read_text(),a.environment.read_text()]; expected=['0c2d8771755082845d926492ba34a891437b3d232b572953d3775d5d51d1d36f', '0bab082a24c5e464b8871d156a8a95b3ea8f2475bf55273716b4149a889de90c']
    for s,h in zip(sources,expected): assert hashlib.sha256(s.encode()).hexdigest()==h
    tables=[dict(re.findall(r'local\s+(\w+(?:Vs|Fs)Src)\s*=\s*\[\[(.*?)\]\]',s,re.S)) for s in sources]
    tables=[{k:v.removeprefix('\n') for k,v in t.items()}for t in tables]
    specs=[('CannonBolt','LightningCannonBoltGL4',0,'boltVsSrc','boltFsSrc',''),('CannonGlow','LightningCannonGlowGL4',0,'glowVsSrc','glowFsSrc',''),('CannonImpact','LightningCannonImpactGL4',0,'impactVsSrc','impactFsSrc','bitmaps/projectiletextures/flare2.tga'),('EnvironmentBolt','EnvLightningBoltGL4',1,'boltVsSrc','boltFsSrc',''),('EnvironmentGlow','EnvLightningGlowGL4',1,'glowVsSrc','glowFsSrc','')]
    params=sorted(set(re.findall(r'^\s*([A-Z][A-Z0-9_]+)\s*=', '\n'.join(sources),re.M)))
    comment='/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */\n#pragma once\n'
    meta=comment+'// Generated from exact original test30770 source; no general shader capability.\n#include "NativeLightningEffectData.h"\n#include <string_view>\nnamespace rendering {\n'
    meta+='inline constexpr std::array<std::string_view,'+str(len(params))+'> NativeLightningParameterNames={'+','.join('"'+x+'"'for x in params)+'};\n'
    meta+='struct NativeLightningProgramSpec {std::string_view name;std::uint64_t vertexHash,fragmentHash;std::string_view texture;};\ninline constexpr std::array<NativeLightningProgramSpec,5> NativeLightningPrograms={{\n'
    shader='#include <metal_stdlib>\nusing namespace metal;\ninline float mod(float x,float y){return x-y*floor(x/y); }\nstruct LightningUniform {float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float parameters['+str(len(params))+'];uint4 options;float4 alpha;};\n'
    shader+='bool LightningAlpha(float a,float r,uint c){switch(c){case 0:return false;case 1:return a<r;case 2:return a==r;case 3:return a<=r;case 4:return a>r;case 5:return a!=r;case 6:return a>=r;default:return true;}}\n'
    bindings=[]; required=[]; samplerNames=[]
    for number,(kind,name,which,vkey,fkey,texture) in enumerate(specs):
        vs,fs=tables[which][vkey],tables[which][fkey]
        samplerNames.append(re.findall(r'uniform\s+sampler2D\s+(\w+)\s*;',fs))
        code=re.sub(r'//[^\n]*|/\*.*?\*/','',vs+'\n'+fs,flags=re.S)
        required.append([bool(re.search(r'\b'+name+r'\b',code)) for name in params])
        meta+='{"'+name+'",0x'+format(fnv(vs),'x')+'ULL,0x'+format(fnv(fs),'x')+'ULL,"'+texture+'"},\n'
        for stage,raw in [('Vertex',vs),('Fragment',fs)]:
            source=re.sub(r'^\s*#.*$','',raw,flags=re.M)
            inputs=re.findall(r'layout\s*\(location\s*=\s*\d+\)\s*in\s+(\w+)\s+(\w+)\s*;',source)
            source=re.sub(r'layout\s*\(location\s*=\s*\d+\)\s*in\s+\w+\s+\w+\s*;','',source)
            interface=re.search(r'(?:in|out)\s+DataVS\s*\{(.*?)\};',source,re.S)
            assert interface,(kind,stage)
            fields=re.findall(r'(\w+)\s+(\w+)\s*;',re.sub(r'//[^\n]*','',interface[1]))
            source=source[:interface.start()]+source[interface.end():]
            samplers=re.findall(r'uniform\s+sampler2D\s+(\w+)\s*;',source)
            source=re.sub(r'uniform\s+sampler2D\s+\w+\s*;','',source)
            source=re.sub(r'out\s+vec4\s+fragColor\s*;','',source)
            for glsl,msl in [('vec2','float2'),('vec3','float3'),('vec4','float4'),('mat3','float3x3'),('mat4','float4x4')]: source=re.sub(r'\b'+glsl+r'\b',msl,source)
            source=re.sub(r'\bdiscard\s*;', 'discard_fragment();',source)
            for image in samplers:source=re.sub(r'\btexture\(\s*'+image+r'\s*,','sample('+image+',',source)
            assert len(re.findall(r'\bvoid\s+main\s*\(\s*(?:void\s*)?\)',source))==1,(kind,stage)
            source=re.sub(r'\bvoid\s+main\s*\(\s*(?:void\s*)?\)','void run()',source)
            struct=kind+stage+'Program'
            if stage=='Vertex':
                shader+='struct '+kind+'Varying {float4 position [[position]];'+''.join(t.replace('vec','float')+' '+n+';'for t,n in fields)+'};\n'
            members='float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;'+''.join('float '+n+';'for n in params)
            members+=''.join(t.replace('vec','float')+' '+n+';'for t,n in inputs+fields)
            members+=('float4 gl_Position;'if stage=='Vertex'else'float4 fragColor;'+''.join('texture2d<float> '+n+';'for n in samplers))
            helper='float4 sample(texture2d<float> t,float2 uv){constexpr sampler s(coord::normalized,address::repeat,filter::linear,mip_filter::linear);return t.sample(s,uv);}' if samplers else ''
            shader+='struct '+struct+'{'+members+helper+'\n'+source+'\n};\n'
            initialize='p.cameraViewProj=u.cameraViewProj;p.cameraViewInv=u.cameraViewInv;p.timeInfo=u.timeInfo;'+''.join('p.'+n+'=u.parameters['+str(i)+'];'for i,n in enumerate(params))
            if stage=='Vertex':
                stream={'position_xy_uv':'mesh[id]','startPosAndWidth':'instances[instance*6]','endPosAndLife':'instances[instance*6+1]','endPosAndTip':'instances[instance*6+1]','coreColor':'instances[instance*6+2]','edgeColor':'instances[instance*6+3]','boltParams':'instances[instance*6+4]','extraParams':'instances[instance*6+5]'}
                shader+='vertex '+kind+'Varying '+kind+'VertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant LightningUniform& u [[buffer(2)]]){'+struct+' p{};'+initialize+''.join('p.'+n+'='+stream[n]+';'for _,n in inputs)+'p.run();'+kind+'Varying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;'+''.join('out.'+n+'=p.'+n+';'for _,n in fields)+'return out;}\n'
            else:
                shader+='fragment float4 '+kind+'FragmentMain('+kind+'Varying input [[stage_in]],constant LightningUniform& u [[buffer(2)]]'+(',texture2d<float> image [[texture(0)]]'if samplers else'')+'){'+struct+' p{};'+initialize+''.join('p.'+n+'=input.'+n+';'for _,n in fields)+''.join('p.'+n+'=image;'for n in samplers)+'p.run();if(u.options.y&&!LightningAlpha(p.fragColor.a,u.alpha.x,u.options.z))discard_fragment();return p.fragColor;}\n'
        bindings.append({'program':name,'vertexFNV64':format(fnv(vs),'016x'),'fragmentFNV64':format(fnv(fs),'016x'),'vertexSHA256':hashlib.sha256(vs.encode()).hexdigest(),'fragmentSHA256':hashlib.sha256(fs.encode()).hexdigest()})
    meta+='}};\ninline constexpr std::array<std::string_view,5> NativeLightningSamplerNames={'+','.join('"'+(row[0]if row else'')+'"'for row in samplerNames)+'};\ninline constexpr std::array<std::array<bool,'+str(len(params))+'>,5> NativeLightningRequiredParameters={{\n'+''.join('{{'+','.join('true' if b else 'false' for b in row)+'}},\n' for row in required)+'}};\n}\n'
    backend=a.source/'rts/Rendering/Backend';(backend/'NativeLightningEffectPrograms.h').write_text(meta)
    (a.source/'rts/Rendering/Metal4/NativeLightningShaders.h').write_text(comment+'// Generated expression-preserving GLSL interface/type adaptation.\nnamespace rendering::metal4::lightning {\ninline constexpr const char* SHADER=R"MSL('+shader+')MSL";\n}\n')
    a.work.mkdir(parents=True,exist_ok=True);(a.work/'shader-translation.json').write_text(json.dumps({'inputs':expected,'parameters':params,'programs':bindings},indent=2)+'\n');(a.work/'effects.metal').write_text(shader)
    print(len(params),'parameters,5 original programs')
if __name__=='__main__':main()
