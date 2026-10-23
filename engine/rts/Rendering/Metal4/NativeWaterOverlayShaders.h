/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
// Native transport of the pinned lava/lava_fog_light GLSL programs.
// Original shaders: Copyright (c) 2024 Beherith (mysterme@gmail.com), MIT.
#define RECOIL_NATIVE_WATER_OVERLAY_MSL R"WATERMSL(
struct WaterU {
 float4x4 viewProj,viewProjInv,shadowView;
 float4 camera,mapSize,time,sun,view,parameters;
 uint4 flags;
 uint4 images[6];
 float4 alpha;
};
struct WaterOut {
 float4 position [[position]];
 float4 worldPos [[RECOIL_SAMPLE_PERSPECTIVE]];
 float4 worldUV [[RECOIL_SAMPLE_PERSPECTIVE]];
 float inboundsness [[RECOIL_SAMPLE_PERSPECTIVE]];
 float2 screenUV [[RECOIL_SAMPLE_NOPERSPECTIVE]];
};
float WaterRand(float2 p){return fract(sin(dot(p,float2(12.9898,78.233)))*43758.5453);}
WaterOut WaterVertex(V v,constant WaterU& u,bool fog){
 WaterOut o;const float2 plane=float4(v.clip).xy;
 o.worldPos=float4((1.5f*plane.x+.5f)*u.mapSize.x,u.parameters.x,(1.5f*plane.y+.5f)*u.mapSize.y,1);
 o.worldUV=float4(1.5f*plane+.5f,0,0);o.worldUV.y*=u.mapSize.y/u.mapSize.x;
 if(!fog){
  const float t=u.time.x+u.time.w;const float frequency=u.parameters.w>.5f?.008f:.025f;
  const float2 r=float2(WaterRand(o.worldPos.xz),WaterRand(o.worldPos.xz*float2(17.876234,9.283)));
  o.worldUV.zw=sin(r+t*frequency*(.5f+r));
  o.worldUV.xy+=float2(sin(t*.0001f),cos(t*.0001f))*.05f;
 }
 const float2 m=min(o.worldPos.xz,u.mapSize.xy-o.worldPos.xz)/u.mapSize.xy;o.inboundsness=min(m.x,m.y);
 o.position=u.viewProj*o.worldPos;o.screenUV=o.position.xy/o.position.w*.5f+.5f;
 if(!u.flags.x)o.position.z=(o.position.z+o.position.w)*.5f;
 return o;
}
vertex WaterOut WaterSurfaceVertex(uint id [[vertex_id]],device const V* vertices [[buffer(0)]],constant WaterU& u [[buffer(1)]]){return WaterVertex(vertices[id],u,false);}
vertex WaterOut WaterFogVertex(uint id [[vertex_id]],device const V* vertices [[buffer(0)]],constant WaterU& u [[buffer(1)]]){return WaterVertex(vertices[id],u,true);}
float2 WaterUV(float2 uv,uint4 f){if(f.x)uv.y=1-uv.y;if(f.y)uv.x=clamp(uv.x,0.f,1.f);if(f.z)uv.y=clamp(uv.y,0.f,1.f);return uv;}
float4 WaterSample(texture2d<float> t,sampler s,float2 uv,uint4 f){
 if(f.x)uv.y=1-uv.y;const float2 dx=dfdx(uv),dy=dfdy(uv);f.x=0;uv=WaterUV(uv,f);return t.sample(s,uv,gradient2d(dx,dy));
}
fragment OUTPUT_TYPE WaterSurfaceFragment(WaterOut in [[stage_in]],constant WaterU& u [[buffer(1)]],
 texture2d<float> heightmap [[texture(0)]],sampler hs [[sampler(0)]],
 texture2d<float> diffuse [[texture(1)]],sampler ds [[sampler(1)]],
 texture2d<float> normal [[texture(2)]],sampler ns [[sampler(2)]],
 texture2d<float> distortionMap [[texture(3)]],sampler xs [[sampler(3)]],
 depth2d<float> shadowMap [[texture(4)]],sampler ss [[sampler(4)]],
 texture2d<float> info [[texture(5)]],sampler ls [[sampler(5)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const bool acid=u.parameters.w>.5f;const float lava=u.parameters.x,coastWidth=acid?16.f:25.f,uvScale=acid?3.f:2.f;
 const float3 worldtocam=u.camera.xyz-in.worldPos.xyz;
 float localheight=-100;
 if(in.inboundsness>0){float2 w=in.worldPos.xz;w+=float2(-8)*(w/u.mapSize.xy)+float2(4);const float2 uv=clamp(w,float2(8),u.mapSize.xy-float2(8))/u.mapSize.xy;localheight=heightmap.sample(hs,WaterUV(uv,u.images[0]),level(0)).x;}
 if(localheight>lava-2.f)discard_fragment();
 const float4 nodiffuse=WaterSample(diffuse,ds,in.worldUV.xy*uvScale,u.images[1]);
 const float2 rotate=in.worldUV.zw*(acid?.01f:.003f);
 float coast=clamp((localheight-lava+coastWidth+2.f)/coastWidth,0.f,1.f);
 if(coast>.90f){coast=9.f*(1.f-coast);coast=pow(coast/.9f,1.f);}else coast=pow(coast/.9f,3.f);
 float4 projected=u.shadowView*float4(in.worldPos.xyz,1);projected.xy+=float2(.5f);
 const float2 shadowUV=projected.xy/projected.w;float reference=projected.z/projected.w;
 if(u.flags.y)reference=clamp(reference,0.f,1.f);
 const float shadow=clamp(shadowMap.sample_compare(ss,WaterUV(shadowUV,u.images[4]),reference),0.f,1.f);
 const float2 losUV=clamp(in.worldPos.xz,float2(0),u.mapSize.xy)/u.mapSize.zw;
 float los=dot(float3(.33f),WaterSample(info,ls,losUV,u.images[5]).rgb);los=clamp(los*4.f-1.f,.5f,1.f);if(in.inboundsness<0)los=1;
 const float2 camshift=u.parameters.yz*.001f;
 const float4 distortionTex=WaterSample(distortionMap,xs,(in.worldUV.xy+camshift)*45.2f,u.images[3]);
 float2 distortion=distortionTex.xy*.2f*.02f;distortion*=clamp(nodiffuse.a*.5f+coast,.2f,2.f);
 float2 uv=in.worldUV.xy*uvScale+distortion+rotate;
 float4 normalHeight=WaterSample(normal,ns,uv,u.images[2]);
 const float3 viewDir=normalize(worldtocam);
 const float pdepth=(acid?24.f:16.f)*((acid?.15f:.5f)-normalHeight.a)*(1.f-coast);
 uv-=pdepth*viewDir.xz*.002f;normalHeight=WaterSample(normal,ns,uv,u.images[2]);
 const float4 diffuseEmit=WaterSample(diffuse,ds,uv,u.images[1]);float4 result=diffuseEmit;
 float3 n=normalHeight.xzy*2.f-1.f;n.z=-n.z;n=normalize(n);
 const float light=clamp(dot(u.sun.xyz,n),.2f,1.f)*max(.5f,shadow);result.rgb*=light;
 result.rgb+=(acid?float3(.3f,1.5f,.2f):float3(2,.5f,0))*coast;
 const float3 reflected=reflect(-normalize(u.sun.xyz),n);
 const float specular=clamp(pow(dot(viewDir,reflected),acid?12.f:64.f),0.f,1.f)*shadow;
 result.rgb+=result.rgb*specular;result.rgb+=result.rgb*(diffuseEmit.a*distortion.y*700.f);result.rgb*=los;
 result.a=clamp(in.inboundsness*2.f+2.f,0.f,1.f);
 if(acid)result.rgb*=float3(.15f,1.f,.45f);
 if(u.alpha.z&&!Alpha(result.a,u.alpha.x,uint(u.alpha.y)))discard_fragment();
 return OUTPUT_VALUE(result);
}
fragment OUTPUT_TYPE WaterFogFragment(WaterOut in [[stage_in]],constant WaterU& u [[buffer(1)]],
 depth2d<float> mapDepth [[texture(0)]],sampler ms [[sampler(0)]],
 depth2d<float> modelDepth [[texture(1)]],sampler ds [[sampler(1)]],
 texture2d<float> distortionMap [[texture(2)]],sampler xs [[sampler(2)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT){
 RECOIL_SAMPLE_FIX_POSITION(in.position,get_sample_position(recoilOriginalSampleID));
 const bool acid=u.parameters.w>.5f;const float lava=u.parameters.x;
 const float2 screenUV=clamp(in.screenUV,1.f/u.view.xy,1.f-1.f/u.view.xy);
 const float depth=min(mapDepth.sample(ms,WaterUV(screenUV,u.images[0]),level(0)),modelDepth.sample(ds,WaterUV(screenUV,u.images[1]),level(0)));
 float4 world=u.viewProjInv*float4(screenUV*2.f-1.f,depth,1);world.xyz/=world.w;const float trueHeight=world.y;
 float above=1;
 if(world.y<lava-20.f-2.f){float3 ray=world.xyz-u.camera.xyz;ray=20.f*ray/abs(ray.y);world.xyz=in.worldPos.xyz+ray;above=acid?.1f:1.f;}
 const float actualDepth=length(world.xyz-in.worldPos.xyz),factor=acid?.1f:.06f;
 float fog=(1.f-exp2(-factor*factor*actualDepth*.5f))*above;
 const float4 distortion=WaterSample(distortionMap,xs,in.worldUV.xy*22.f+u.parameters.yz*.01f,u.images[2]);
 const float strength=acid?1.f:4.f;fog*=(strength+distortion.x+distortion.y)/strength;
 float coast=clamp(1.f-abs(trueHeight-(lava-20.f-2.f))/(acid?16.f:25.f),0.f,1.f);coast=pow(coast,3.f)*(acid?.5f:.6f);fog+=coast;
 float4 result=float4(acid?float3(.3f,1.2f,.2f):float3(2,.5f,0),fog*clamp(in.inboundsness*2.f+2.f,0.f,1.f));
 if(acid)result.rgb*=float3(.15f,1.f,.45f);
 if(u.alpha.z&&!Alpha(result.a,u.alpha.x,uint(u.alpha.y)))discard_fragment();
 return OUTPUT_VALUE(result);
}
)WATERMSL"
