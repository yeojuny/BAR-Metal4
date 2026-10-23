/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeSampleShadingShaders.h"

namespace rendering::metal4 {
// Original SMF forward equations and the explicitly bounded original CUS
// direct/ambient contribution. Environment convolution/reflection and dynamic
// damage/noise producers are independent required stages, not constant inputs.
inline constexpr const char* WORLD_SHADER = RECOIL_NATIVE_SAMPLE_SHADING_MSL R"MSL(
#include <metal_stdlib>
using namespace metal;
// Matches the actual retained shadow-color publication selected by the host.
constant bool WorldHasShadowColor [[function_constant(0)]];
struct WorldUniform {
 float4x4 viewProjection, world, piece;
 float4 team;
 uint4 terrain, options;
 float4 parameters;
};
struct SceneUniform {
 float4 sun, cameraPosition, groundAmbient, groundDiffuse, groundSpecular;
 float4 modelAmbient, modelDiffuse, modelSpecular, fogColor, fogParameters;
 float4 waterBase, waterMinimum, waterAbsorb, parameters;
 float4x4 shadow;
 uint4 flags;
 float4 splatScales, splatMultipliers, mapDimensions;
 float4 modelClipPlane, terrainClipPlane;
};
struct TileInstance { uint x, z, layer, padding; };
struct WorldVertex {
 packed_float3 position, normal, sTangent, tTangent;
 packed_float2 uv[2];
 ushort boneIds[4]; uchar boneWeights[4];
};
struct WorldVarying {
 float4 clip [[position]];
 float clipDistance [[clip_distance]];
 float2 uv [[RECOIL_SAMPLE_PERSPECTIVE]];
 uint layer [[flat]];
 float3 worldPosition [[RECOIL_SAMPLE_PERSPECTIVE]], worldNormal [[RECOIL_SAMPLE_PERSPECTIVE]], worldTangent [[RECOIL_SAMPLE_PERSPECTIVE]], halfDirection [[RECOIL_SAMPLE_PERSPECTIVE]];
 float fogFactor [[RECOIL_SAMPLE_PERSPECTIVE]];
 // Original CUS vertex-stage values; interpolate with the same geometry/UVs.
 float vertexAO [[RECOIL_SAMPLE_PERSPECTIVE]], selfIllumination [[RECOIL_SAMPLE_PERSPECTIVE]];
};
float4 WorldClip(float3 position, constant WorldUniform& u) {
 float4 clip = u.viewProjection * float4(position, 1);
 if (u.options.x & 1u) clip.z = (clip.z + clip.w) * .5f;
 if (u.options.x & 2u) clip.y = -clip.y;
 return clip;
}
float WorldFog(float3 position, constant SceneUniform& s) {
 return clamp((s.fogParameters.y-length(position-s.cameraPosition.xyz))*s.fogParameters.w,0.f,1.f);
}
float3 Hash31(float p) {
 float3 p3=fract(float3(p)*float3(.1031f,.1030f,.0973f));
 p3+=dot(p3,p3.yzx+19.19f);
 return fract((p3.xxy+p3.yzz)*p3.zyx);
}
float3 TreeWind(float3 p,float id,float frame) {
 float f=floor(frame*.001333f),q=fract(frame*.001333f);
 float2 wind=mix(sin(float2(0,1.56f)+f*5.f)*10.f,sin(float2(0,1.56f)+(f+1)*5.f)*10.f,smoothstep(0.f,1.f,q));
 float3 hash=clamp(fract(16*Hash31(id)),.4f,1.f);
 float4 phase=float4(.02f*frame*hash.x+p.x,.02f*frame*hash.z/3+hash.x,.02f*frame+p.z,0);
 float4 tri=abs(fract(phase+.5f)*2-1),wave=tri*tri*(3-2*tri);
 float limit=clamp(p.x*p.z*p.y/3000,0.f,.2f);
 float diff2=wave.y*clamp(p.y/30,.05f,.2f);
 p+=wave.z*limit*clamp(abs(wind.x)+abs(wind.y),1.2f,1.7f);
 p.xz+=wave.x*limit+diff2*wind;
 return p;
}
WorldVarying TerrainVertex(uint vid,uint iid,device const float* heights,constant WorldUniform& u,device const TileInstance* tiles,constant SceneUniform& s) {
 constexpr uint2 corners[6]={uint2(0,0),uint2(0,1),uint2(1,0),uint2(1,0),uint2(0,1),uint2(1,1)};
 TileInstance tile=tiles[iid]; uint cell=vid/6;
 uint2 local=uint2(cell%4,cell/4)+corners[vid%6],grid=uint2(tile.x,tile.z)*4+local;
 float3 p=float3(grid.x*u.parameters.x,heights[grid.y*u.terrain.x+grid.x],grid.y*u.parameters.x);
 WorldVarying out;out.clipDistance=dot(float4(p,1),s.terrainClipPlane);
 // Original1024-square UVs: spatial neighbours share the same filtering
 // domain at all four supplied levels, including internal SMT boundaries.
 out.clip=WorldClip(p,u);out.uv=(float2(tile.x%32,tile.z%32)+float2(local)*.25f)/32.f;out.layer=tile.layer;
 out.worldPosition=p;out.worldNormal=float3(0,1,0);out.worldTangent=float3(1,0,0);
 out.halfDirection=normalize(s.sun.xyz+normalize(s.cameraPosition.xyz-p));out.fogFactor=WorldFog(p,s);out.vertexAO=out.selfIllumination=1;return out;
}
float ShadowComparison(float3 p,depth2d<float> shadow,constant SceneUniform& s,sampler cmp,float bias=0) {
 if(s.flags.x==0)return 1;
 float4 h=s.shadow*float4(p,1);h.xy+=.5f;
 // Original shadow coordinates use bottom-left XY and already normalized Z.
 return shadow.sample_compare(cmp,float2(h.x/h.w,1-h.y/h.w),h.z/h.w-bias);
}
// Exact engine ModelFragProg/ModelFragProgGL4 RGB shadow multiplier.
// Selected30770 CUS has no shadowColor sampler and deliberately does not call this.
float3 EngineModelShadowMultiplier(float3 p,float nl,depth2d<float> depth,texture2d<float> color,sampler filter,constant SceneUniform& s) {
 if(!WorldHasShadowColor || s.flags.x==0)return float3(1);
 float4 h=s.shadow*float4(p,1);h.xy+=.5f;const float2 uv=float2(h.x/h.w,1-h.y/h.w);
 const float shade=min(depth.sample_compare(filter,uv,h.z/h.w),smoothstep(0.f,.35f,nl));
 return mix(1.f,shade,s.parameters.z)*color.sample(filter,uv,level(0)).rgb;
}
float4 SourceTextureSample(texture2d<float> image,sampler filter,float2 uv,bool noMip) {
 return noMip?image.sample(filter,uv,level(0)):image.sample(filter,uv);
}
float4 SourceTerrainDetailSample(texture2d<float> image,sampler filter,sampler baseFilter,float2 uv,bool noMip) {
 // Original GL_LINEAR is still derivative-based anisotropic sampling; an
 // explicit level(0) would lose that footprint even with all source mips kept.
 return noMip?image.sample(baseFilter,uv):image.sample(filter,uv);
}
fragment float4 TerrainFragmentMain(WorldVarying in [[stage_in]],constant SceneUniform& s [[buffer(3)]],
 texture2d_array<float> tiles [[texture(0)]],texture2d<float> normals [[texture(2)]],
 texture2d<float> detail [[texture(3)]],texture2d<float> specular [[texture(4)]],
 texture2d<float> blendNormal [[texture(5)]],texture2d<float> emission [[texture(6)]],
 depth2d<float> shadow [[texture(7)]],texture2d<float> splatDetail [[texture(8)]],
 texture2d<float> splatDistribution [[texture(9)]],texture2d<float> splatN0 [[texture(10)]],
 texture2d<float> splatN1 [[texture(11)]],texture2d<float> splatN2 [[texture(12)]],texture2d<float> splatN3 [[texture(13)]],texture2d<float> metalInfo [[texture(14)]],sampler shadowSampler [[sampler(0)]],
 sampler diffuseFilter [[sampler(1)]],sampler detailFilter [[sampler(2)]],sampler splatFilter [[sampler(3)]],
 sampler detailBaseFilter [[sampler(4)]],sampler splatBaseFilter [[sampler(5)]],
 texture2d<float> standardShading [[texture(15)]],sampler standardShadeFilter [[sampler(6)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler ts(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 constexpr sampler ns(coord::normalized,address::clamp_to_edge,min_filter::nearest,mag_filter::linear);
 float2 mapUV=in.worldPosition.xz/s.mapDimensions.xy;
 // Original GLSL_SHADER_FWD_STD: plain detail + RGBA8 shadingTex,
 // diffuse alpha and fog. No DNTS/splat/specular/emission/shadow variant.
 if(s.flags.y&64u){
  float4 tex=tiles.sample(diffuseFilter,in.uv,in.layer,bias(s.mapDimensions.w));
  if(s.flags.z&16384u){
   constexpr sampler infoFilter(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
   const float intensity=(s.flags.z&32768u)?1.f:2.f;
   tex.rgb+=metalInfo.sample(infoFilter,in.worldPosition.xz/(float2(metalInfo.get_width(),metalInfo.get_height())*8.f)).rgb*intensity;
   tex.rgb-=float3(.5f)*float(intensity==1.f);
  }
  float3 detailColor=SourceTerrainDetailSample(detail,detailFilter,detailBaseFilter,in.worldPosition.xz*.02f,bool(s.flags.w&1u)).rgb*2-1;
  float3 color=(tex.rgb+detailColor)*standardShading.sample(standardShadeFilter,mapUV).rgb;
  if((s.flags.z&3u)&&tex.a<=s.mapDimensions.z)discard_fragment();
  return float4(mix(s.fogColor.rgb,color,in.fogFactor),tex.a);
 }
 float2 xz=normals.sample(ns,mapUV).rg; float3 n=float3(xz.x,sqrt(1-dot(xz,xz)),xz.y);
 float3 bt=normalize(cross(n,float3(-1,0,0))),st=cross(n,bt);float3x3 tbn=float3x3(st,bt,n);
 if(s.flags.y&1u){float4 b=SourceTextureSample(blendNormal,ts,mapUV,bool(s.flags.w&4u));n=normalize(mix(n,tbn*(b.xyz*2-1),b.a));}
 float3 dc=0;
 float4 cof=0;
 if(s.flags.y&8u)cof=SourceTerrainDetailSample(splatDistribution,splatFilter,splatBaseFilter,mapUV,bool(s.flags.w&64u))*s.splatMultipliers;
 if(s.flags.y&16u){
  float4 sn=(SourceTerrainDetailSample(splatN0,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.x,bool(s.flags.w&128u))*2-1)*cof.r;
  sn+=(SourceTerrainDetailSample(splatN1,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.y,bool(s.flags.w&256u))*2-1)*cof.g;
  sn+=(SourceTerrainDetailSample(splatN2,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.z,bool(s.flags.w&512u))*2-1)*cof.b;
  sn+=(SourceTerrainDetailSample(splatN3,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.w,bool(s.flags.w&1024u))*2-1)*cof.a;
  sn.y=max(sn.y,.01f);n=normalize(mix(n,normalize(tbn*sn.xyz),min(1.f,dot(cof,float4(1)))));
  if(s.flags.y&32u)dc=float3(clamp(sn.a,-1.f,1.f));
 } else if(s.flags.y&8u){
  float4 v=float4(SourceTerrainDetailSample(splatDetail,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.x,bool(s.flags.w&32u)).r,SourceTerrainDetailSample(splatDetail,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.y,bool(s.flags.w&32u)).g,
   SourceTerrainDetailSample(splatDetail,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.z,bool(s.flags.w&32u)).b,SourceTerrainDetailSample(splatDetail,splatFilter,splatBaseFilter,in.worldPosition.xz*s.splatScales.w,bool(s.flags.w&32u)).a)*2-1;
  dc=float3(dot(v,cof));
 } else dc=SourceTerrainDetailSample(detail,detailFilter,detailBaseFilter,in.worldPosition.xz*.02f,bool(s.flags.w&1u)).rgb*2-1;
 float4 tex=tiles.sample(diffuseFilter,in.uv,in.layer,bias(s.mapDimensions.w));
 // Original SMF overlays modify diffuse BEFORE light/detail/specular/fog.
 // Original infoTexIntensityMul is1 for LOS,2 for metal; subtract .5 only for LOS.
 if(s.flags.z&16384u){
  constexpr sampler infoFilter(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::nearest);
  const float intensity=(s.flags.z&32768u)?1.f:2.f;
   tex.rgb+=metalInfo.sample(infoFilter,in.worldPosition.xz/(float2(metalInfo.get_width(),metalInfo.get_height())*8.f)).rgb*intensity;
   tex.rgb-=float3(.5f)*float(intensity==1.f);
 }
 float ndl=clamp(dot(s.sun.xyz,n),0.f,1.f);
 float shadowFactor=mix(1.f,ShadowComparison(in.worldPosition,shadow,s,shadowSampler),s.parameters.y);
 float3 light=(s.groundAmbient.rgb+s.groundDiffuse.rgb*ndl*shadowFactor)*(210.f/255.f);
 float alpha=1;
 if(s.flags.z&1u)alpha=float(in.worldPosition.y>=0);
 if(s.flags.z&2u)alpha=tex.a;
 if((s.flags.z&4u)&&in.worldPosition.y<0){
  float a=abs(in.worldPosition.y)*.1f,decay=.2f+a*.1f,h=min(1023.f,-in.worldPosition.y),wl=min(ndl*2+.4f,1.f);
  a=min(1.f,a+float(in.worldPosition.y<=-10));
  float3 water=max(s.waterMinimum.rgb,s.waterBase.rgb-s.waterAbsorb.rgb*h)*(210.f/255.f)*wl;
  water*=1-decay*(1-shadowFactor);light=mix(light,water,a);
 }
 float3 color=(tex.rgb+dc)*light;
 if(s.flags.y&4u){float4 e=SourceTextureSample(emission,ts,mapUV,bool(s.flags.w&8u));color=color*(1-e.a)+e.rgb;}
 float4 sp=(s.flags.y&2u)?SourceTextureSample(specular,ts,mapUV,bool(s.flags.w&2u)):float4(s.groundSpecular.rgb,1);
 float exponent=(s.flags.y&2u)?sp.a*16:s.parameters.x;
 color+=sp.rgb*pow(clamp(dot(normalize(in.halfDirection),n),.001f,1.f),exponent)*shadowFactor;
 if((s.flags.z&3u)&&alpha<=s.mapDimensions.z)discard_fragment();
 return float4(mix(s.fogColor.rgb,color,in.fogFactor),alpha);
}
WorldVarying ModelVertex(uint vid,device const WorldVertex* vertices,constant WorldUniform& u,constant SceneUniform& s) {
 WorldVertex v=vertices[vid];float3 p=float3(v.position);
 if(u.options.w&16u)p*=fract(float(u.terrain.w)*.01f)*.2f+.9f;
 if(u.options.w&1u)p=TreeWind(p,float(u.terrain.w),u.parameters.y);
 float4 wp=u.world*u.piece*float4(p,1);float3x3 transform=float3x3((u.world*u.piece)[0].xyz,(u.world*u.piece)[1].xyz,(u.world*u.piece)[2].xyz);
 float3 tangent=float3(v.sTangent);
 if(dot(tangent,tangent)<.1f||dot(float3(v.tTangent),float3(v.tTangent))<.1f)tangent=float3(1,0,0);
 WorldVarying out;out.clipDistance=dot(wp,s.modelClipPlane);out.clip=WorldClip(wp.xyz,u);out.uv=float2(v.uv[0]);out.layer=0;
 out.worldPosition=wp.xyz;out.worldNormal=transform*float3(v.normal);out.worldTangent=transform*tangent;
 // Exact30770 cus_gl4.vert.glsl:713-724. AO is encoded in the
 // fractional low bits of the ORIGINAL vertex U, never sampled per fragment.
 out.vertexAO=(u.options.w&512u)?clamp(fract(float(v.uv[0].x)*16384.f),.1f,1.f):1.f;
 out.selfIllumination=(u.options.w&1024u)?max(-.2f,sin(u.parameters.y*2.f/30.f+float(u.terrain.w)*.1f))+.2f:1.f;
 out.halfDirection=0;out.fogFactor=WorldFog(wp.xyz,s);return out;
}
float3 Fresnel(float3 a,float3 b,float v){return a+(b-a)*pow(clamp(1-v,0.f,1.f),5.f);}
fragment float4 UnitFragmentMain(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]],
 texture2d<float> color [[texture(0)]],texture2d<float> other [[texture(1)]],texture2d<float> normal [[texture(2)]],
 texture2d<float> brdf [[texture(3)]],depth2d<float> shadow [[texture(7)]],texturecube<float> reflection [[texture(14)]],sampler shadowSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler us(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 constexpr sampler bs(coord::normalized,address::clamp_to_edge,filter::linear);
 float2 uv1=float2(in.uv.x,(u.options.y&1u)?1-in.uv.y:in.uv.y),uv2=float2(in.uv.x,(u.options.z&1u)?1-in.uv.y:in.uv.y);
 float4 t1=(u.options.y&2u)?color.sample(us,uv1,level(0)):color.sample(us,uv1,bias(-.5f));
 float4 t2=(u.options.z&2u)?other.sample(us,uv2,level(0)):other.sample(us,uv2,bias(-.5f));
 if((u.options.w&2u)&&t2.a<.5f)discard_fragment();
 float3 N=normalize(in.worldNormal),T=normalize(in.worldTangent),B=normalize(cross(N,T));
 float2 uvn=float2(in.uv.x,(u.options.w&128u)?1-in.uv.y:in.uv.y);
 float3 tbnNormal=(u.options.w&8u)?SourceTextureSample(normal,us,uvn,bool(u.options.w&256u)).xyz*2-1:float3(0,0,1);
 // Exact CUS fragment normalizes before lighting AND roughness adjustment.
 tbnNormal=normalize(tbnNormal);
 N=normalize(float3x3(T,B,N)*tbnNormal);
 if(u.options.w&16u){float saturation=clamp(dot(abs(t1.rgb-t1.gbr),float3(1)),0.f,1.f);t1.rgb*=fract(float(u.terrain.w)*float3(.01f,.013f,.017f)-.5f)*saturation+1;}
 float3 albedo=mix(t1.rgb,u.team.rgb,t1.a);
 float emission=clamp(in.selfIllumination*t2.r,0.f,1.f),metal=clamp(t2.g,0.f,1.f),rough=t2.b;
 if(u.options.w&4u){emission=.2f;rough=.8f;metal=.1f;}
 if(u.options.w&16u){float charAmount=clamp(1-as_type<float>(u.terrain.z),0.f,1.f)*step(.5f,u.parameters.y);
  if(charAmount>.002f){albedo=mix(albedo,float3(.018f,.015f,.013f),charAmount);rough=max(rough,mix(rough,.9f,charAmount));metal=min(metal,mix(metal,0.f,charAmount));}}
 float3 L=normalize(s.sun.xyz),V=normalize(s.cameraPosition.xyz-in.worldPosition),H=normalize(L+V);
 float nlU=dot(N,L),nl=clamp(nlU,0.f,1.f),nh=clamp(dot(N,H),0.f,1.f),nv=clamp(dot(N,V),1e-4f,1.f),vh=clamp(dot(V,H),0.f,1.f);
 float nlen2=dot(tbnNormal,tbnNormal);
 if(nlen2<1){float nlen=sqrt(nlen2),kappa=(3*nlen-nlen2*nlen)/(1-nlen2);rough=min(1.f,sqrt(rough*rough+1/kappa));}
 rough=clamp(rough,.04f,1.f);float r2=rough*rough,r4=r2*r2;
 float texelBias=.5f/shadow.get_width()*tan(acos(nl));texelBias=clamp(texelBias,0.f,2.5f/shadow.get_width());
 float sh=mix(1.f,min(smoothstep(0.f,.35f,nlU),ShadowComparison(in.worldPosition,shadow,s,shadowSampler,texelBias)),s.parameters.z);
 float3 f0=mix(float3(.04f),albedo,metal),f90=float3(clamp(max(f0.r,max(f0.g,f0.b))*50,0.f,1.f));
 float2 e=brdf.sample(bs,float2(nv,rough),level(0)).rg;
 float3 energy=clamp(1+f0*(1/max(e.x,1e-4f)-1),float3(1),float3(2));
 float3 F=Fresnel(f0,f90,vh),maxSun=mix(s.modelSpecular.rgb,s.modelDiffuse.rgb,step(dot(s.modelSpecular.rgb,float3(.2126f,.7152f,.0722f)),dot(s.modelDiffuse.rgb,float3(.2126f,.7152f,.0722f))));
 float denom=nl*(nv*(1-r2)+r2)+nv*(nl*(1-r2)+r2),vis=denom>0?.5f/denom:0;
 float d=(nh*r4-nh)*nh+1,distribution=r4/(d*d);
 float3 direct=maxSun*(1-F)*(1-metal)*albedo*nl*sh+F*vis*distribution*maxSun*nl*sh*energy;
 // Exact30770's TextureEnvBlured call is commented: iblDiffuse and
 // iblSpecular remain zero before sun-ambient mixing. The real reflection
 // cube still contributes the active Split-Sum path below.
 float3 ambientF=Fresnel(f0,max(f90-float3(rough),f0),vh),environmentSpecular=0;
 if(s.flags.z&65536u){
  constexpr sampler env(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
  // Exact30770 CUS getSpecularDominantDirection, before sampling.
  float3 rv=mix(reflect(-V,N),N,r4);float lodBias=(log2(float(reflection.get_width()))-4)*rough;
  float3 value=(s.flags.z&131072u)?reflection.sample(env,rv,bias(lodBias)).rgb:reflection.sample(env,rv,level(0)).rgb;
  value*=1+.4f*smoothstep(.55f,1.f,dot(float3(.2126f,.7152f,.0722f),value));
  value*=1-rough; // USE_ENVIRONMENT_SPECULAR=1, iblSpecular is source-zero.
  float ao=clamp(pow(nv+in.vertexAO,exp2(-16*r2-1))-1+in.vertexAO,0.f,1.f);
  environmentSpecular=value*(f0*e.x+f90*e.y)*ao*energy;
 }
 float3 outColor=(1-ambientF)*(1-metal)*(s.modelAmbient.rgb*.77f)*albedo*in.vertexAO+environmentSpecular+direct+emission*albedo;
 // Actual30770 shader-effective constants; saved config is separate metadata.
 outColor=(outColor*(4.85f*outColor+.85f))/(outColor*(3.5f*outColor+.85f)+1);
 outColor=mix(s.fogColor.rgb,outColor,in.fogFactor)*u.parameters.w;
 return float4(outColor,t2.a*u.team.a);
}
vertex WorldVarying ShadowModelVertex(uint vid [[vertex_id]],device const WorldVertex* vertices [[buffer(0)]],
 constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]]) {
 WorldVarying out=ModelVertex(vid,vertices,u,s);out.clipDistance=1;
 float3 normal=float3(vertices[vid].normal);
 float ndl=clamp(dot(normal,float3(0,0,1)),0.f,1.f),b=clamp(5e-5f*tan(acos(ndl)),0.f,2.5e-4f);
 // CUS source applies this bias to the original piece-local normal.
 float4 lp=s.shadow*float4(out.worldPosition,1);lp.xy+=.5f;lp.z+=b;
 out.clip=u.viewProjection*lp;
 if(u.options.x==1)out.clip.z=(out.clip.z+out.clip.w)*.5f;
 return out;
}
vertex WorldVarying ShadowTerrainVertex(uint vid [[vertex_id]],uint iid [[instance_id]],
 device const float* heights [[buffer(0)]],constant WorldUniform& u [[buffer(1)]],
 device const TileInstance* tiles [[buffer(2)]],constant SceneUniform& s [[buffer(3)]]) {
 WorldVarying out=TerrainVertex(vid,iid,heights,u,tiles,s);out.clipDistance=1;
 float4 lp=s.shadow*float4(out.worldPosition,1);lp.xy+=.5f;out.clip=u.viewProjection*lp;
 if(u.options.x==1)out.clip.z=(out.clip.z+out.clip.w)*.5f;return out;
}
fragment void ShadowTerrainFragment( RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {}
vertex WorldVarying UnitVertexMain(uint vid [[vertex_id]],device const WorldVertex* vertices [[buffer(0)]],constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]]) {return ModelVertex(vid,vertices,u,s);}
vertex WorldVarying TerrainVertexMain(uint vid [[vertex_id]],uint iid [[instance_id]],device const float* heights [[buffer(0)]],constant WorldUniform& u [[buffer(1)]],device const TileInstance* tiles [[buffer(2)]],constant SceneUniform& s [[buffer(3)]]) {return TerrainVertex(vid,iid,heights,u,tiles,s);}
// Original ModelFragProgGL4 normal-shading forward family. No CUS material
// effects are retained when its original controller is disabled.
fragment float4 EngineStandardModelFragmentMain(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]],
 texture2d<float> color [[texture(0)]],texture2d<float> other [[texture(1)]],depth2d<float> shadow [[texture(7)]],texturecube<float> reflection [[texture(14)]],texture2d<float> shadowColor [[texture(16), function_constant(WorldHasShadowColor)]],sampler shadowSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler image(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 float2 uv1=float2(in.uv.x,(u.options.y&1u)?1-in.uv.y:in.uv.y),uv2=float2(in.uv.x,(u.options.z&1u)?1-in.uv.y:in.uv.y);
 float4 t1=SourceTextureSample(color,image,uv1,bool(u.options.y&2u)),t2=SourceTextureSample(other,image,uv2,bool(u.options.z&2u));
 float alpha=u.team.a*float(t2.a>=.5f);
 if(alpha<=u.parameters.z)discard_fragment();
 float3 albedo=mix(t1.rgb,u.team.rgb,t1.a);
 float3 N=normalize(in.worldNormal),L=normalize(s.sun.xyz),V=normalize(s.cameraPosition.xyz-in.worldPosition),H=normalize(L+V);
 float nl=clamp(dot(N,L),0.f,1.f),nh=clamp(dot(N,H),0.f,1.f);
 float3 shadowMult=EngineModelShadowMultiplier(in.worldPosition,nl,shadow,shadowColor,shadowSampler,s);
 float3 light=s.modelAmbient.rgb+nl*s.modelDiffuse.rgb;
 float3 specular=s.modelSpecular.rgb*min(pow(nh,2.5f*s.parameters.x)+.3f*pow(nh,6.f),1.f);
 specular*=t2.g*4.f*shadowMult;
 light=mix(s.modelAmbient.rgb,light,shadowMult);
 if(s.flags.z&65536u){
  constexpr sampler environment(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
  float3 R=-reflect(V,N);
  float3 refl=(s.flags.z&131072u)?reflection.sample(environment,R).rgb:reflection.sample(environment,R,level(0)).rgb;
  light=mix(light,refl,t2.g);
 }
 light+=t2.r;
 return float4(mix(s.fogColor.rgb,albedo*light+specular,in.fogFactor),alpha*((u.options.w&2048u)?.6f:1.f));
}
vertex WorldVarying EngineStandardShadowVertex(uint vid [[vertex_id]],device const WorldVertex* vertices [[buffer(0)]],
 constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]]) {
 WorldVarying out=ModelVertex(vid,vertices,u,s);out.clipDistance=1;
 float4 light=s.shadow*float4(out.worldPosition,1);
 float3 normal=normalize(float3x3(s.shadow[0].xyz,s.shadow[1].xyz,s.shadow[2].xyz)*out.worldNormal);
 float nl=clamp(normal.z,0.f,1.f),bias=1e-5f*clamp(tan(acos(nl)),0.f,30.f);
 light.xy+=.5f;light.z+=bias;out.clip=u.viewProjection*light;
 if(u.options.x==1)out.clip.z=(out.clip.z+out.clip.w)*.5f;
 return out;
}
fragment void EngineStandardShadowFragment(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],texture2d<float> other [[texture(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler image(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 float2 uv=float2(in.uv.x,(u.options.z&1u)?1-in.uv.y:in.uv.y);
 if(SourceTextureSample(other,image,uv,bool(u.options.z&2u)).a<=.5f)discard_fragment();
}


// Pinned ModelVertProg/ModelFragProg used specifically by the engine's
// DrawIndividualDefAlpha preview. Blueprint's unlit shader is not this family.
// Exact ModelFragProg legacy dynamic-light input after original handler updates.
struct NativeModelLightUniform {
 float4 position,halfVector,directionCutoff,ambient,diffuse,specular,attenuation;
};
float3 LegacyDynamicModelLighting(float3 position,float3 normal,float3 diffuse,float3 specular,device const uchar* bytes) {
 device const uint4* header=reinterpret_cast<device const uint4*>(bytes);
 device const NativeModelLightUniform* lights=reinterpret_cast<device const NativeModelLightUniform*>(bytes+16);
 float3 rgb=0;
 for(uint i=0;i<header->x;++i){
  NativeModelLightUniform light=lights[i];
  float3 vector=light.position.xyz-position;
  float radius=light.attenuation.x,distance=length(vector);
  float scale=float(distance<=radius);
  float cosDiffuse=clamp(dot(normal,vector/distance),0.f,1.f);
  float cosSpecular=clamp(dot(normal,normalize(light.halfVector.xyz)),0.f,1.f);
  float attenuation=1.f-min(1.f,(distance*distance)/(radius*radius));
  float spot=dot(-vector/distance,light.directionCutoff.xyz);
  scale*=float(spot>=light.directionCutoff.w);
  rgb+=scale*light.ambient.rgb;
  rgb+=scale*attenuation*diffuse*light.diffuse.rgb*cosDiffuse;
  rgb+=scale*attenuation*specular*light.specular.rgb*pow(cosSpecular,4.f);
 }
 return rgb;
}
struct PlacementSampleUniform {float4 legacyCamera;};
vertex WorldVarying PlacementSampleVertex(uint vid [[vertex_id]],device const WorldVertex* vertices [[buffer(0)]],constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]],constant PlacementSampleUniform& p [[buffer(4)]]) {
 WorldVertex v=vertices[vid];float4x4 m=u.world*u.piece;float4 wp=m*float4(float3(v.position),1);
 float3 a=m[0].xyz,b=m[1].xyz,c=m[2].xyz;
 float3x3 normalMatrix=float3x3(cross(b,c),cross(c,a),cross(a,b))/dot(a,cross(b,c));
 WorldVarying out;out.clipDistance=1;out.clip=WorldClip(wp.xyz,u);out.uv=float2(v.uv[0]);out.layer=0;
 out.worldPosition=wp.xyz;out.worldNormal=normalMatrix*float3(v.normal);out.worldTangent=0;
 out.halfDirection=wp.xyz-p.legacyCamera.xyz;
 out.fogFactor=clamp((s.fogParameters.y-length(out.halfDirection))*s.fogParameters.w,0.f,1.f);
 out.vertexAO=out.selfIllumination=1;return out;
}
fragment float4 PlacementSampleFragment(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]],device const uchar* modelLights [[buffer(5)]],
 texture2d<float> color [[texture(0)]],texture2d<float> other [[texture(1)]],depth2d<float> shadow [[texture(7)]],texturecube<float> reflection [[texture(14)]],texturecube<float> specular [[texture(15)]],texture2d<float> shadowColor [[texture(16), function_constant(WorldHasShadowColor)]],sampler shadowSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler image(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 constexpr sampler env(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
 float2 uv1=float2(in.uv.x,(u.options.y&1u)?1-in.uv.y:in.uv.y),uv2=float2(in.uv.x,(u.options.z&1u)?1-in.uv.y:in.uv.y);
 float4 diffuse=SourceTextureSample(color,image,uv1,bool(u.options.y&2u)),extra=SourceTextureSample(other,image,uv2,bool(u.options.z&2u));
 float alpha=u.team.a*extra.a;if(alpha<=.1f)discard_fragment();
 float3 normal=normalize(in.worldNormal);float nl=max(dot(normal,s.sun.xyz),.001f);
 float3 light=nl*s.modelDiffuse.rgb+s.modelAmbient.rgb;
 float3 reflectDir=reflect(in.halfDirection,normal);
 float3 reflected=(s.flags.z&131072u)?reflection.sample(env,reflectDir).rgb:reflection.sample(env,reflectDir,level(0)).rgb;
 float3 highlight=specular.sample(env,reflectDir,level(0)).rgb*s.modelSpecular.rgb;
 float3 shadowMult=EngineModelShadowMultiplier(in.worldPosition,nl,shadow,shadowColor,shadowSampler,s);
 highlight*=extra.g*4.f;highlight*=shadowMult;
 light=mix(s.modelAmbient.rgb,light,shadowMult);
 reflected=mix(light,reflected,extra.g)+extra.rrr;
 float3 value=mix(diffuse.rgb,u.team.rgb,diffuse.a)*reflected+highlight;
 value+=LegacyDynamicModelLighting(in.worldPosition,normal,diffuse.rgb,highlight,modelLights);
 return float4(mix(s.fogColor.rgb,value,in.fogFactor),alpha);
}

fragment float4 EngineModelFragmentMain(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],constant SceneUniform& s [[buffer(3)]],
 texture2d<float> color [[texture(0)]],texture2d<float> other [[texture(1)]],depth2d<float> shadow [[texture(7)]],texturecube<float> reflection [[texture(14)]],texturecube<float> specular [[texture(15)]],sampler shadowSampler [[sampler(0)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler original(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 float4 t1=SourceTextureSample(color,original,in.uv,bool(u.options.y&2u)),t2=SourceTextureSample(other,original,in.uv,bool(u.options.z&2u));
 float alpha=t2.a*u.team.a;if(alpha<=.5f)discard_fragment();
 float3 n=normalize(in.worldNormal);float nl=max(dot(n,s.sun.xyz),.001f);
 float factor=mix(1.f,ShadowComparison(in.worldPosition,shadow,s,shadowSampler),s.parameters.z);
 float3 lighting=mix(s.modelAmbient.rgb,s.modelAmbient.rgb+nl*s.modelDiffuse.rgb,factor);
 float3 albedo=mix(t1.rgb,u.team.rgb,t1.a),value=albedo*(lighting*(1-t2.g)+t2.r);
 if(s.flags.z&65536u){
  constexpr sampler env(coord::normalized,address::clamp_to_edge,filter::linear,mip_filter::linear);
  float3 rv=reflect(normalize(in.worldPosition-s.cameraPosition.xyz),n);
  float3 reflected=(s.flags.z&131072u)?reflection.sample(env,rv).rgb:reflection.sample(env,rv,level(0)).rgb;
  float3 spec=specular.sample(env,rv,level(0)).rgb*s.modelSpecular.rgb*(t2.g*4)*factor;
  value=albedo*(mix(lighting,reflected,t2.g)+t2.r)+spec;
 }
 return float4(mix(s.fogColor.rgb,value,in.fogFactor),alpha);
}
fragment void ShadowModelFragment(WorldVarying in [[stage_in]],constant WorldUniform& u [[buffer(1)]],texture2d<float> other [[texture(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
	RECOIL_SAMPLE_FIX_POSITION(in.clip,get_sample_position(recoilOriginalSampleID));
 constexpr sampler us(coord::normalized,address::repeat,filter::linear,mip_filter::linear);
 float2 uv=float2(in.uv.x,(u.options.z&1u)?1-in.uv.y:in.uv.y);
 if((u.options.w&2u)&&SourceTextureSample(other,us,uv,bool(u.options.z&2u)).a<.5f)discard_fragment();
}
// Same-frame original glCopyTexSubImage2D: exact texel fetch performs only
// BGRA/RGBA channel-format conversion. No rescale, smoothing or stale image.
kernel void WaterSceneColorCopy(texture2d<float,access::read> source [[texture(0)]],
 texture2d<float,access::write> destination [[texture(1)]],constant uint2& origin [[buffer(0)]],
 uint2 pixel [[thread_position_in_grid]]) {
 if(any(pixel>=uint2(destination.get_width(),destination.get_height())))return;
 destination.write(source.read(pixel+origin),pixel);
}
// The donor glCopyTexSubImage2D is 1:1. For an independent native raster,
// preserve its logical consumer with nearest logical pixel-center adaptation.
// CPU-generated integer coordinates avoid fractional viewport/scissor drift.
uint2 LogicalSceneCopyPixel(uint2 pixel,constant uint* coordinates) {
 return uint2(coordinates[1+pixel.x],coordinates[1+coordinates[0]+pixel.y]);
}
kernel void LogicalSceneColorCopy(texture2d<float,access::read> source [[texture(0)]],
 texture2d<float,access::write> destination [[texture(1)]],constant uint* coordinates [[buffer(0)]],
 uint2 pixel [[thread_position_in_grid]]) {
 if(any(pixel>=uint2(destination.get_width(),destination.get_height())))return;
 destination.write(source.read(LogicalSceneCopyPixel(pixel,coordinates)),pixel);
}
struct LogicalSceneDepthCopyVertexOutput {float4 position [[position]];};
vertex LogicalSceneDepthCopyVertexOutput LogicalSceneDepthCopyVertex(uint vid [[vertex_id]]) {
 const float2 positions[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(positions[vid],0,1)};
}
struct LogicalSceneDepthCopyOutput {float depth [[depth(any)]];};
fragment LogicalSceneDepthCopyOutput LogicalSceneDepthCopyFragment(float4 position [[position]],
 depth2d<float> source [[texture(0)]],constant uint* coordinates [[buffer(0)]]) {
 return {source.read(LogicalSceneCopyPixel(uint2(position.xy),coordinates))};
}
float3 HeightVertex(int2 point,device const float* heights,uint2 dimensions) {
 point=clamp(point,int2(0),int2(dimensions));
 float h=all(point<int2(dimensions))?heights[uint(point.y)*dimensions.x+uint(point.x)]:0;
 return float3(point.x*8,h,point.y*8);
}
kernel void WorldTerrainNormals(uint2 point [[thread_position_in_grid]],device const float* heights [[buffer(0)]],
 texture2d<float,access::write> result [[texture(0)]]) {
 uint2 size=uint2(result.get_width(),result.get_height());if(any(point>=size))return;
 int2 p=int2(point);float3 c=HeightVertex(p,heights,size);
 float3 bl=HeightVertex(p+int2(-1,-1),heights,size)-c,bm=HeightVertex(p+int2(0,-1),heights,size)-c,br=HeightVertex(p+int2(1,-1),heights,size)-c;
 float3 ml=HeightVertex(p+int2(-1,0),heights,size)-c,mr=HeightVertex(p+int2(1,0),heights,size)-c;
 float3 tl=HeightVertex(p+int2(-1,1),heights,size)-c,tm=HeightVertex(p+int2(0,1),heights,size)-c,tr=HeightVertex(p+int2(1,1),heights,size)-c;
 float3 n=normalize(cross(tr,mr)+cross(mr,br)+cross(br,bm)+cross(bm,bl)+cross(bl,ml)+cross(ml,tl)+cross(tl,tm)+cross(tm,tr));
 result.write(float4(n.x,n.z,0,1),point);
}
)MSL";
} // namespace rendering::metal4
