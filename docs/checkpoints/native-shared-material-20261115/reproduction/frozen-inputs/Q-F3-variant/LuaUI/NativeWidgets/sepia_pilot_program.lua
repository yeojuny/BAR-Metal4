-- Generated from GLSL by compile_subset.py; Beherith original shader, GPL v2 or later.
local generated = {vertex=[=[#version 330
// full screen triangle
const vec2 vertices[3] = vec2[3](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));

void main() {
	gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
}
]=],fragment=[=[#version 330

uniform sampler2D screenCopyTex;
uniform ivec2 viewPos;
uniform float exposure = 1.0;
uniform vec3 bias = vec3(0.0);
uniform float gamma;
// rows of the color matrix: contrast * saturation, blended toward sepia
uniform vec3 colorR;
uniform vec3 colorG;
uniform vec3 colorB;

out vec4 fragColor;

void main() {
	vec3 color = pow(texelFetch(screenCopyTex, ivec2(gl_FragCoord.xy) - viewPos, 0).rgb, vec3(gamma)) * exposure + bias;
	fragColor = vec4(dot(colorR, color), dot(colorG, color), dot(colorB, color), 1.0);
}
]=],uniformInt={["screenCopyTex"]=0}}

local exposure = Spring.GetConfigFloat('V2PilotExposure',1.0)
local red = Spring.GetConfigFloat('V2PilotBiasR',0.0)
local green = Spring.GetConfigFloat('V2PilotBiasG',0.0)
local blue = Spring.GetConfigFloat('V2PilotBiasB',0.0)
generated.uniforms = {
 exposure={values={exposure},integer=false},
 bias={values={red,green,blue},integer=false},
}
Spring.Echo(string.format('[V2GeneratedUniforms] exposure=%.3f bias=%.3f,%.3f,%.3f source=private_generated_module',
 exposure,red,green,blue))
return generated
