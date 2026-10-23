// Native API transport only. The original material equations follow unchanged.
// Empty sampling is legal only after the actual source history proves an empty
// object. False retains a binding and never sets this mask on its own.
uniform uvec4 NativeCUSImageMasks; // empty, V inversion, sampled depth, fixed-request compare clamp
uniform uint NativeCUSSampleCodes[12]; // depth mode and original RGBA swizzle
bool NativeCUSEmpty(int slot) { return (NativeCUSImageMasks.x & (1u << uint(slot))) != 0u; }
vec2 NativeCUSUV(int slot, vec2 uv) {
 if ((NativeCUSImageMasks.y & (1u << uint(slot))) != 0u) uv.y = 1.0 - uv.y;
 return uv;
}
float NativeCUSChannel(vec4 value, uint channel) {
 if (channel < 4u) return value[int(channel)];
 return channel == 5u ? 1.0 : 0.0;
}
vec4 NativeCUSChannels(int slot, vec4 value) {
 uint code = NativeCUSSampleCodes[slot];
 if ((NativeCUSImageMasks.z & (1u << uint(slot))) != 0u) {
  uint mode = code >> 16u;
  value = mode == 0u ? vec4(value.rrr,1.0) : mode == 1u ? vec4(value.r) : vec4(0.0,0.0,0.0,value.r);
 }
 return vec4(NativeCUSChannel(value,code & 15u),NativeCUSChannel(value,(code>>4u)&15u),NativeCUSChannel(value,(code>>8u)&15u),NativeCUSChannel(value,(code>>12u)&15u));
}
vec4 NativeCUSTexture(sampler2D image,int slot,vec2 uv) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,texture(image,NativeCUSUV(slot,uv)));}
vec4 NativeCUSTexture(sampler2D image,int slot,vec2 uv,float bias) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,texture(image,NativeCUSUV(slot,uv),bias));}
vec4 NativeCUSTexture(samplerCube image,int slot,vec3 uv) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,texture(image,uv));}
vec4 NativeCUSTexture(samplerCube image,int slot,vec3 uv,float bias) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,texture(image,uv,bias));}
vec4 NativeCUSTexture(sampler3D image,int slot,vec3 uv) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,texture(image,uv));}
float NativeCUSTexture(sampler2DShadow image,int slot,vec3 uv) {uv.xy=NativeCUSUV(slot,uv.xy);if((NativeCUSImageMasks.w&(1u<<uint(slot)))!=0u)uv.z=clamp(uv.z,0.0,1.0);return texture(image,uv);}
vec4 NativeCUSLod(sampler2D image,int slot,vec2 uv,float lod) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,textureLod(image,NativeCUSUV(slot,uv),lod));}
vec4 NativeCUSLod(samplerCube image,int slot,vec3 uv,float lod) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,textureLod(image,uv,lod));}
vec4 NativeCUSLod(sampler3D image,int slot,vec3 uv,float lod) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);return NativeCUSChannels(slot,textureLod(image,uv,lod));}
vec4 NativeCUSFetch(sampler2D image,int slot,ivec2 uv,int lod) {if(NativeCUSEmpty(slot))return vec4(0,0,0,1);if((NativeCUSImageMasks.y&(1u<<uint(slot)))!=0u)uv.y=textureSize(image,lod).y-1-uv.y;return NativeCUSChannels(slot,texelFetch(image,uv,lod));}
ivec2 NativeCUSSize(sampler2D image,int slot,int lod) {return NativeCUSEmpty(slot)?ivec2(0):textureSize(image,lod);}
ivec2 NativeCUSSize(samplerCube image,int slot,int lod) {return NativeCUSEmpty(slot)?ivec2(0):textureSize(image,lod);}
ivec2 NativeCUSSize(sampler2DShadow image,int slot,int lod) {return textureSize(image,lod);}
