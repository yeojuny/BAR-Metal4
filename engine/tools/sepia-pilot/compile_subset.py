#!/usr/bin/env python3
"""Offline, fail-closed GLSL 330 expression subset -> MSL + interface.
No external compiler. Admits the source-selected fullscreen vertex unchanged;
fragment supports typed locals, uniforms/defaults, constructors, arithmetic,
swizzles, dot/pow/min/max/clamp/mix and lod-zero texelFetch. No preprocessor,
loops, functions, writes, other stages or hidden source rewriting.
Original Sepia shader/widget by Beherith (GPL v2 or later).
"""
import argparse, hashlib, json, re, math
from pathlib import Path
REVISION='sepia-subset-ast-1;MTL4LibraryDescriptor-default-options-frozenJ;sample-position-v1'
TYPES={'float':('float',1),'vec2':('float',2),'vec3':('float',3),'vec4':('float',4),'int':('int',1),'ivec2':('int',2),'ivec3':('int',3),'ivec4':('int',4)}
MSL={t:base+(str(n) if n>1 else '') for t,(base,n) in TYPES.items()}
def tokens(source):
 source=re.sub(r'/\*.*?\*/|//[^\n]*','',source,flags=re.S)
 source=re.sub(r'^\s*#version 330\s*$','',source,flags=re.M)
 pattern=r'\s+|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?|[A-Za-z_]\w*|[-+*/=.,;(){}\[\]]'
 out=[];pos=0
 for m in re.finditer(pattern,source):
  if m.start()!=pos:raise ValueError('unsupported GLSL token at '+source[pos:pos+30])
  pos=m.end()
  if not m[0].isspace():out.append(m[0])
 if source[pos:].strip():raise ValueError('unsupported trailing input')
 return out
class Parser:
 def __init__(self,source):self.ts=tokens(source)+['<eof>'];self.i=0;self.env={};self.fields=[];self.output=None
 def take(self,w=None):
  t=self.ts[self.i]
  if w is not None and t!=w:raise ValueError(f'expected {w}, got {t}')
  self.i+=1;return t
 def peek(self):return self.ts[self.i]
 def ident(self):
  x=self.take()
  if not re.fullmatch(r'[A-Za-z_]\w*',x) or x.startswith(('gl_','pilot','Shared')) or x in set(TYPES)|{'target','alpha','raster','auto','bool','thread','device','constant','vertex','fragment','texture2d','input','id','source','pass','return','const','uniform','out','void','main'}:raise ValueError('reserved/invalid identifier '+x)
  return x
 def expr(self,minimum=0):
  t=self.take()
  if t in ('+','-'):
   a=self.expr(30)
   if a[-1] not in TYPES:raise ValueError('non-arithmetic unary operand')
   left=('unary',t,a,a[-1])
  elif t=='(':
   left=self.expr();self.take(')')
  elif re.fullmatch(r'(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?',t):left=('literal',t,'float' if any(c in t for c in '.eE') else 'int')
  elif re.fullmatch(r'[A-Za-z_]\w*',t):
   if self.peek()=='(':
    self.take('(');args=[]
    if self.peek()!=')':
     while True:
      args.append(self.expr())
      if self.peek()!=',':break
      self.take(',')
    self.take(')');left=('call',t,args,self.calltype(t,args))
   else:
    if t not in self.env:raise ValueError('unknown identifier '+t)
    left=('name',t,self.env[t])
  else:raise ValueError('unsupported expression '+t)
  while True:
   if self.peek()=='.':
    self.take('.');s=self.take();base,n=TYPES[left[-1]]
    if not s or not (set(s)<=set('xyzw') or set(s)<=set('rgba')) or max(('xyzw' if set(s)<=set('xyzw') else 'rgba').index(c) for c in s)>=n:raise ValueError('invalid swizzle')
    left=('swizzle',left,s,next(k for k,v in TYPES.items() if v==(base,len(s))));continue
   op=self.peek();prec={'+':10,'-':10,'*':20,'/':20}.get(op,-1)
   if prec<minimum:break
   self.take();right=self.expr(prec+1);a,b=left[-1],right[-1]
   if a not in TYPES or b not in TYPES or TYPES[a][0]!=TYPES[b][0]:raise ValueError('arithmetic type mismatch')
   if a!=b and TYPES[a][1]!=1 and TYPES[b][1]!=1:raise ValueError('arithmetic dimension mismatch')
   ty=a if TYPES[a][1]>=TYPES[b][1] else b;left=('binary',op,left,right,ty)
  return left
 def calltype(self,name,args):
  ts=[x[-1] for x in args]
  if name in TYPES:
   if not ts or any(t not in TYPES for t in ts):raise ValueError('bad constructor')
   total=sum(TYPES[t][1] for t in ts)
   if total!=TYPES[name][1] and not(len(ts)==1 and TYPES[ts[0]][1]==1):raise ValueError('constructor dimensions')
   return name
  if name=='texelFetch':
   if ts!=['sampler2D','ivec2','int'] or args[2][:2]!=('literal','0'):raise ValueError('only sampler2D lod-zero texelFetch admitted')
   coord=args[1]
   expected=('binary','-',('call','ivec2',[('swizzle',('name','gl_FragCoord','vec4'),'xy','vec2')],'ivec2'),('name','viewPos','ivec2'),'ivec2')
   if coord!=expected:raise ValueError('texelFetch outside original screen-copy coordinate transport')
   return 'vec4'
  if name=='dot' and len(ts)==2 and ts[0]==ts[1] and ts[0] in TYPES and TYPES[ts[0]][0]=='float':return 'float'
  if name in ('pow','min','max') and len(ts)==2 and ts[0]==ts[1] and TYPES.get(ts[0],('',))[0]=='float':return ts[0]
  if name in ('clamp','mix') and len(ts)==3 and len(set(ts))==1 and TYPES.get(ts[0],('',))[0]=='float':return ts[0]
  raise ValueError('unsupported function/signature '+name+str(ts))
 def parse(self):
  while self.peek()=='uniform':
   self.take();ty=self.take()
   if ty not in TYPES and ty!='sampler2D':raise ValueError('unsupported uniform type '+ty)
   name=self.ident()
   if name in self.env:raise ValueError('duplicate uniform')
   default=None
   if self.peek()=='=':self.take();default=self.expr()
   if default is not None and default[-1]!=ty:raise ValueError('default type differs')
   self.take(';');self.env[name]=ty;self.fields.append((name,ty,default))
  self.take('out');self.take('vec4');self.output=self.ident();self.take(';')
  self.env['gl_FragCoord']='vec4';self.env[self.output]='vec4'
  self.take('void');self.take('main');self.take('(');self.take(')');self.take('{');body=[];written=False
  while self.peek()!='}':
   if self.peek() in TYPES:
    ty=self.take();name=self.ident()
    if name in self.env:raise ValueError('duplicate local')
    self.take('=');expr=self.expr()
    if ty!=expr[-1]:raise ValueError('local type differs')
    self.take(';');self.env[name]=ty;body.append(('declare',name,ty,expr))
   else:
    name=self.take()
    if name!=self.output or written:raise ValueError('only one output assignment admitted')
    self.take('=');expr=self.expr();self.take(';')
    if expr[-1]!='vec4':raise ValueError('output type differs')
    body.append(('output',expr));written=True
  self.take('}');self.take('<eof>')
  if not written:raise ValueError('missing output')
  return body
 def emit(self,x):
  tag=x[0]
  if tag=='literal':return x[1]+('f' if x[-1]=='float' else '')
  if tag=='name':
   if x[1]=='gl_FragCoord':return 'pilotFragCoord'
   if x[1]==self.output:return 'pilotResult'
   if x[1] in {f[0] for f in self.fields}:
    return x[1] if x[-1]=='sampler2D' else 'u.'+x[1]+'.'+'xyzw'[:TYPES[x[-1]][1]]
   return x[1]
  if tag=='unary':return '('+x[1]+self.emit(x[2])+')'
  if tag=='binary':return '('+self.emit(x[2])+x[1]+self.emit(x[3])+')'
  if tag=='swizzle':return '('+self.emit(x[1])+').'+x[2]
  if tag=='call':
   name=MSL.get(x[1],x[1]);args=[self.emit(a) for a in x[2]]
   if name=='texelFetch':name='pilotFetch';args=args[:2]
   return name+'('+','.join(args)+')'
  raise ValueError('internal expression')
def defaults(x):
 if x is None:return None
 if x[0]=='literal':return [float(x[1]) if x[-1]=='float' else int(x[1])]
 if x[0]=='unary':return [(-v if x[1]=='-' else v) for v in defaults(x[2])]
 if x[0]=='call' and x[1] in TYPES:
  v=sum((defaults(a) for a in x[2]),[]);n=TYPES[x[1]][1]
  return v*n if len(v)==1 else v
 raise ValueError('only literal/constructor constant defaults admitted')
def fnv(s):
 h=14695981039346656037
 for b in s.encode():h=((h^b)*1099511628211)&((1<<64)-1)
 return h
def compile_sources(vertex,fragment):
 # Vertex transport is deliberately pinned, with all accepted input consumed.
 admitted='''#version 330
 const vec2 vertices[3] = vec2[3](vec2(-1.0,-1.0),vec2(3.0,-1.0),vec2(-1.0,3.0));
 void main(){gl_Position=vec4(vertices[gl_VertexID],0.0,1.0);}'''
 if tokens(vertex)!=tokens(admitted):raise ValueError('vertex outside pinned fullscreen transport subset')
 p=Parser(fragment);body=p.parse();resources=[f for f in p.fields if f[1]=='sampler2D']
 if len(resources)!=1 or resources[0][2] is not None:raise ValueError('exactly one read-only sampler2D required')
 fields=[];struct=[];offset=48
 for name,ty,default in p.fields:
  if ty=='sampler2D':continue
  base,n=TYPES[ty];value=defaults(default) or [0]*n
  if any(not math.isfinite(x) or (abs(x)>3.402823466e38 if base=='float' else x<-2147483648 or x>2147483647) for x in value):raise ValueError('uniform default outside typed finite range')
  fields.append(dict(name=name,scalar=base,components=n,offset=offset,defaults=value,blockSlot=1,count=1,stride=16,stages=2));offset+=16;struct.append(f' {base}4 {name};')
 if offset>4096:raise ValueError('bounded uniform block exceeds 4096 bytes')
 if not any(f['name']=='viewPos' and f['scalar']=='int' and f['components']==2 for f in fields):raise ValueError('screen-copy address bounds require ivec2 viewPos')
 code='\n'.join((' '+MSL[s[2]]+' '+s[1]+'='+p.emit(s[3])+';' if s[0]=='declare' else ' pilotResult='+p.emit(s[1])+';') for s in body)
 sampler=resources[0][0]
 msl='''struct SharedSepiaUniforms {float4 target;uint4 alpha;float4 raster;\n'''+ '\n'.join(struct)+'''\n};
struct SharedSepiaVarying {float4 position [[position]];};
vertex SharedSepiaVarying SharedSepiaVertex(uint id [[vertex_id]],constant SharedSepiaUniforms& u [[buffer(1)]]) {
 const float2 vertices[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(vertices[id],u.raster.y,1)};
}
float4 pilotFetch(texture2d<float,access::read> source,int2 p){return source.read(uint2(p.x,int(source.get_height())-1-p.y));}
fragment float4 SharedSepiaFragment(SharedSepiaVarying input [[stage_in]],texture2d<float,access::read> '''+sampler+''' [[texture(0)]],constant SharedSepiaUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float4 pilotFragCoord=float4(input.position.x*u.target.z,(u.target.y-input.position.y)*u.target.w,input.position.z,input.position.w);
 float4 pilotResult;
'''+code+'''
 if(u.alpha.x){float r=clamp(u.raster.x,0.0f,1.0f);bool pass=false;switch(u.alpha.y){case 0:break;case 1:pass=pilotResult.a<r;break;case 2:pass=pilotResult.a==r;break;case 3:pass=pilotResult.a<=r;break;case 4:pass=pilotResult.a>r;break;case 5:pass=pilotResult.a!=r;break;case 6:pass=pilotResult.a>=r;break;default:pass=true;}if(!pass)discard_fragment();}
 return pilotResult;
}
'''
 msl=msl.replace('struct SharedSepiaVarying', 'static_assert(sizeof(SharedSepiaUniforms)=='+str(offset)+');\n'+''.join('static_assert(__builtin_offsetof(SharedSepiaUniforms,'+f['name']+')=='+str(f['offset'])+');\n' for f in fields)+'struct SharedSepiaVarying')
 identity=hashlib.sha256((REVISION+'\0'+vertex+'\0'+fragment).encode()).hexdigest()
 manifest=dict(revision=REVISION,identity=identity,vertexHash=fnv(vertex),fragmentHash=fnv(fragment),uniformBytes=offset,fields=fields,sampler=sampler,vertexEntry='SharedSepiaVertex',fragmentEntry='SharedSepiaFragment')
 return msl,manifest

def header(vertex,fragment,msl,m):
 fields=[];writes=[]
 for f in m['fields']:
  scalar='Float' if f['scalar']=='float' else 'Int'
  fields.append('{"'+f['name']+'", sharedpilot::Scalar::'+scalar+','+str(f['offset'])+','+str(f['components'])+',1,16,sharedpilot::Fragment,1}')
  for i,v in enumerate(f['defaults']):
   ctype='float' if scalar=='Float' else 'std::int32_t';val=repr(float(v))+'f' if scalar=='Float' else str(int(v))
   writes.append(f' {{const {ctype} value={val};std::memcpy(bytes.data()+{f["offset"]+i*4},&value,4);}}')
 return '''// Generated by tools/sepia-pilot/compile_subset.py; DO NOT EDIT.
#pragma once
#include "Rendering/Backend/NativeSharedProgramPilot.h"
#include <cstring>
namespace rendering::sepiaGenerated {
inline constexpr char VertexSource[]=R"PILOTGLSL('''+vertex+''')PILOTGLSL";
inline constexpr char FragmentSource[]=R"PILOTGLSL('''+fragment+''')PILOTGLSL";
inline constexpr char Identity[]="'''+m['identity']+'''";
inline constexpr std::uint32_t UniformBytes='''+str(m['uniformBytes'])+''';
inline constexpr std::uint32_t ViewPosOffset='''+str(next(f['offset'] for f in m['fields'] if f['name']=='viewPos'))+''';
inline std::shared_ptr<const sharedpilot::ProgramInterface> Link() {
 auto p=std::make_shared<sharedpilot::ProgramInterface>();p->generation=sharedpilot::NextProgramGeneration();p->sourceIdentity=Identity;p->uniformBytes=UniformBytes;
 p->blocks={{1,UniformBytes,std::uint8_t(sharedpilot::Vertex|sharedpilot::Fragment)}};
 p->fields={'''+','.join(fields)+'''};
 p->resources={{"'''+m['sampler']+'''",0,0,0,sharedpilot::TextureKind::Image2D,sharedpilot::Fragment}};
 return p;
}
inline std::vector<std::uint8_t> Defaults(){std::vector<std::uint8_t> bytes(UniformBytes);
'''+ '\n'.join(writes)+'''
 return bytes;}
inline constexpr char MSL[]=R"PILOTMSL('''+msl+''')PILOTMSL";
}
'''
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--vertex',type=Path);ap.add_argument('--fragment',type=Path);ap.add_argument('--output',type=Path);ap.add_argument('--check',action='store_true');a=ap.parse_args()
 here=Path(__file__).resolve().parent;v=(a.vertex or here/'sepia.vert.glsl').read_text();f=(a.fragment or here/'sepia.frag.glsl').read_text();msl,m=compile_sources(v,f)
 output=a.output or here.parents[1]/'rts/Rendering/Metal4/NativeSepiaGeneratedProgram.h'
 lua='-- Generated from GLSL by compile_subset.py; Beherith original shader, GPL v2 or later.\nreturn {vertex=[=['+v+']=],fragment=[=['+f+']=],uniformInt={['+json.dumps(m['sampler'])+']=0}}\n'
 products={output:header(v,f,msl,m),output.with_suffix('.json'):json.dumps(m,indent=2)+'\n',output.with_suffix('.metal'):msl,output.parents[3]/'data/LuaUI/NativeWidgets/sepia_pilot_program.lua':lua}
 for path,text in products.items():
  if a.check:
   if path.read_text()!=text:raise SystemExit('generated output differs: '+str(path))
  else:path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text)
 print(('checked' if a.check else 'generated')+' '+m['identity'])
if __name__=='__main__':main()
