#!/usr/bin/env python3
"""Translate the exact original test30770 beam/plasma expressions and contracts."""
import argparse,hashlib,json,re
from pathlib import Path

def fnv(text):
    h=14695981039346656037
    for b in text.encode(): h=((h^b)*1099511628211)&((1<<64)-1)
    return h

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--beam',type=Path,required=True);p.add_argument('--plasma',type=Path,required=True)
    p.add_argument('--source',type=Path,required=True);p.add_argument('--work',type=Path,required=True);a=p.parse_args()
    sources=[a.beam.read_text(),a.plasma.read_text()]; expected=['335112547db63d44a490988ab148eea1dd863685504f2e9b26769a0a8fd63533','bfe2f41ecdd42979a146fe7ff788e74f9651930d38a4ea84652be86cd8c2ab55']
    for s,h in zip(sources,expected): assert hashlib.sha256(s.encode()).hexdigest()==h
    tables=[dict(re.findall(r'local\s+(\w+(?:Vs|Fs)Src)\s*=\s*\[\[(.*?)\]\]',s,re.S)) for s in sources]
    tables=[{k:v.removeprefix('\n') for k,v in t.items()}for t in tables]
    specs=[('Beam','BeamLaserGL4',0,'beamVsSrc','beamFsSrc','bitmaps/projectiletextures/largebeam.tga'),('BeamFlare','BeamLaserFlareGL4',0,'flareVsSrc','flareFsSrc','bitmaps/projectiletextures/flare2.tga'),('BeamGlow','BeamLaserGlowGL4',0,'glowVsSrc','glowFsSrc',''),('BeamPulse','BeamLaserPulseGL4',0,'pulseVsSrc','pulseFsSrc',''),('Plasma','PlasmaCannonGL4',1,'plasmaVsSrc','plasmaFsSrc','bitmaps/projectiletextures/plasmaball.tga'),('PlasmaCross','PlasmaCannonCrossGL4',1,'crossVsSrc','plasmaFsSrc','bitmaps/projectiletextures/plasmaball.tga'),('PlasmaCrossSection','PlasmaCannonCrossSectionGL4',1,'crossSectionVsSrc','crossSectionFsSrc',''),('PlasmaGlow','PlasmaCannonGlowGL4',1,'glowVsSrc','glowFsSrc','bitmaps/projectiletextures/glow2.tga')]
    params=sorted(set(re.findall(r'^\s*([A-Z][A-Z0-9_]+)\s*=', '\n'.join(sources),re.M)))
    comment='/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */\n#pragma once\n'
    meta=comment+'// Generated from exact original test30770 source; no general shader capability.\n#include "NativeProjectileEffectData.h"\n#include <string_view>\nnamespace rendering {\n'
    meta+='inline constexpr std::array<std::string_view,'+str(len(params))+'> NativeProjectileParameterNames={'+','.join('"'+x+'"'for x in params)+'};\n'
    meta+='struct NativeProjectileProgramSpec {std::string_view name;std::uint64_t vertexHash,fragmentHash;std::string_view texture;};\ninline constexpr std::array<NativeProjectileProgramSpec,8> NativeProjectilePrograms={{\n'
    shader='#include <metal_stdlib>\nusing namespace metal;\ninline float mod(float x,float y){return x-y*floor(x/y); }\nstruct EffectUniform {float4x4 cameraViewProj,cameraViewInv;float4 timeInfo;float parameters['+str(len(params))+'];uint4 options;};\n'
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
                stream={'position_xy_uv':'mesh[id]','startPosAndWidth':'instances[instance*5]','endPosAndLife':'instances[instance*5+1]','coreColor':'instances[instance*'+('5'if which==0 else'4')+'+2]'if which==0 else'instances[instance*4+1]','edgeColor':'instances[instance*5+3]','flareData':'instances[instance*5+4]','posAndSize':'instances[instance*4]','edgeColorAndSeed':'instances[instance*4+2]','velocityAndLife':'instances[instance*4+3]'}
                shader+='vertex '+kind+'Varying '+kind+'VertexMain(uint id [[vertex_id]],uint instance [[instance_id]],device const float4* mesh [[buffer(0)]],device const float4* instances [[buffer(1)]],constant EffectUniform& u [[buffer(2)]]){'+struct+' p{};'+initialize+''.join('p.'+n+'='+stream[n]+';'for _,n in inputs)+'p.run();'+kind+'Varying out{};out.position=p.gl_Position;if(u.options.x)out.position.z=(out.position.z+out.position.w)*.5f;'+''.join('out.'+n+'=p.'+n+';'for _,n in fields)+'return out;}\n'
            else:
                shader+='fragment float4 '+kind+'FragmentMain('+kind+'Varying input [[stage_in]],constant EffectUniform& u [[buffer(2)]]'+(',texture2d<float> image [[texture(0)]]'if samplers else'')+'){'+struct+' p{};'+initialize+''.join('p.'+n+'=input.'+n+';'for _,n in fields)+''.join('p.'+n+'=image;'for n in samplers)+'p.run();return p.fragColor;}\n'
        bindings.append({'program':name,'vertexFNV64':format(fnv(vs),'016x'),'fragmentFNV64':format(fnv(fs),'016x'),'vertexSHA256':hashlib.sha256(vs.encode()).hexdigest(),'fragmentSHA256':hashlib.sha256(fs.encode()).hexdigest()})
    meta+='}};\ninline constexpr std::array<std::string_view,8> NativeProjectileSamplerNames={'+','.join('"'+(row[0]if row else'')+'"'for row in samplerNames)+'};\ninline constexpr std::array<std::array<bool,'+str(len(params))+'>,8> NativeProjectileRequiredParameters={{\n'+''.join('{{'+','.join('true' if b else 'false' for b in row)+'}},\n' for row in required)+'}};\n}\n'
    backend=a.source/'rts/Rendering/Backend';(backend/'NativeProjectileEffectPrograms.h').write_text(meta)
    (a.source/'rts/Rendering/Metal4/NativeProjectileEffectShaders.h').write_text(comment+'// Generated expression-preserving GLSL interface/type adaptation.\nnamespace rendering::metal4::projectile {\ninline constexpr const char* SHADER=R"MSL('+shader+')MSL";\n}\n')
    a.work.mkdir(parents=True,exist_ok=True);(a.work/'shader-translation.json').write_text(json.dumps({'inputs':expected,'parameters':params,'programs':bindings},indent=2)+'\n');(a.work/'effects.metal').write_text(shader)
    print(len(params),'parameters,8 original programs')
if __name__=='__main__':main()
