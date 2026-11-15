struct SharedSepiaUniforms {float4 target;uint4 alpha;float4 raster;
 int4 viewPos;
 float4 exposure;
 float4 bias;
 float4 gamma;
 float4 colorR;
 float4 colorG;
 float4 colorB;
};
static_assert(sizeof(SharedSepiaUniforms)==160);
static_assert(__builtin_offsetof(SharedSepiaUniforms,viewPos)==48);
static_assert(__builtin_offsetof(SharedSepiaUniforms,exposure)==64);
static_assert(__builtin_offsetof(SharedSepiaUniforms,bias)==80);
static_assert(__builtin_offsetof(SharedSepiaUniforms,gamma)==96);
static_assert(__builtin_offsetof(SharedSepiaUniforms,colorR)==112);
static_assert(__builtin_offsetof(SharedSepiaUniforms,colorG)==128);
static_assert(__builtin_offsetof(SharedSepiaUniforms,colorB)==144);
struct SharedSepiaVarying {float4 position [[position]];};
vertex SharedSepiaVarying SharedSepiaVertex(uint id [[vertex_id]],constant SharedSepiaUniforms& u [[buffer(1)]]) {
 const float2 vertices[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};
 return {float4(vertices[id],u.raster.y,1)};
}
float4 pilotFetch(texture2d<float,access::read> source,int2 p){return source.read(uint2(p.x,int(source.get_height())-1-p.y));}
fragment float4 SharedSepiaFragment(SharedSepiaVarying input [[stage_in]],texture2d<float,access::read> screenCopyTex [[texture(0)]],constant SharedSepiaUniforms& u [[buffer(1)]] RECOIL_SAMPLE_ID_ARGUMENT RECOIL_SAMPLE_POSITION_ARGUMENT) {
 RECOIL_SAMPLE_FIX_POSITION(input.position,get_sample_position(recoilOriginalSampleID));
 float4 pilotFragCoord=float4(input.position.x*u.target.z,(u.target.y-input.position.y)*u.target.w,input.position.z,input.position.w);
 float4 pilotResult;
 float3 color=((pow((pilotFetch(screenCopyTex,(int2((pilotFragCoord).xy)-u.viewPos.xy))).rgb,float3(u.gamma.x))*u.exposure.x)+u.bias.xyz);
 pilotResult=float4(dot(u.colorR.xyz,color),dot(u.colorG.xyz,color),dot(u.colorB.xyz,color),1.0f);
 if(u.alpha.x){float r=clamp(u.raster.x,0.0f,1.0f);bool pass=false;switch(u.alpha.y){case 0:break;case 1:pass=pilotResult.a<r;break;case 2:pass=pilotResult.a==r;break;case 3:pass=pilotResult.a<=r;break;case 4:pass=pilotResult.a>r;break;case 5:pass=pilotResult.a!=r;break;case 6:pass=pilotResult.a>=r;break;default:pass=true;}if(!pass)discard_fragment();}
 return pilotResult;
}
