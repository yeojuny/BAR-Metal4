/* Original GenEnvLut.lua is PD. Complete original SH equations, selected30770. */
#pragma once
namespace rendering::metal4 {
inline constexpr const char* PBR_ENVIRONMENT_SHADER=R"MSL(
float3 PBRGoldenPoint(float i,float count) {
 float theta=i*2.3999632297286533222315555066336f;
 float z=(1.f-1.f/count)*(1.f-2.f*i/(count-1.f));
 float radius=sqrt(1.f-z*z);return float3(radius*float2(cos(theta),sin(theta)),z);
}
float PBRSH(float3 n,uint2 idx) {
 if(idx.x==0&&idx.y==0)return .2820947918f;
 if(idx.x==0&&idx.y==1)return .4886025119f*n.y;
 if(idx.x==0&&idx.y==2)return .4886025119f*n.z;
 if(idx.x==1&&idx.y==0)return .4886025119f*n.x;
 if(idx.x==1&&idx.y==1)return 1.0925484306f*n.x*n.y;
 if(idx.x==1&&idx.y==2)return 1.0925484306f*n.y*n.z;
 if(idx.x==2&&idx.y==0)return .3153915652f*(3.f*n.z*n.z-1.f);
 if(idx.x==2&&idx.y==1)return 1.0925484306f*n.x*n.z;
 if(idx.x==2&&idx.y==2)return .5462742153f*(n.x*n.x-n.y*n.y);
 return 0.f;
}
kernel void OriginalEnvLUT(uint2 idx [[thread_position_in_grid]],constant int& count [[buffer(0)]],
 texturecube<float> reflectTex [[texture(0)]],texture2d<float,access::write> result [[texture(1)]],sampler reflectionSampler [[sampler(0)]]) {
 if(any(idx>=uint2(3)))return;
 float maxMipMap=log2(float(max(reflectTex.get_width(),reflectTex.get_height())));
 float lodBias=maxMipMap-6.f;float3 sum=0.f;
 for(int i=0;i<count;++i){float3 p=PBRGoldenPoint(float(i),float(count));
  float3 color=reflectTex.sample(reflectionSampler,p,level(lodBias)).rgb;
  color*=1.f+.4f*smoothstep(.55f,1.f,dot(float3(.2126f,.7152f,.0722f),color));
  sum+=PBRSH(p,idx)*color;}
 sum*=4.f*3.1415926535897932384626433832795f/float(count);
 // GL_RGB16F has no alpha channel. The physical RGBA16F view exposes A=1.
 result.write(float4(sum,1.f),idx);
}
)MSL";
}
