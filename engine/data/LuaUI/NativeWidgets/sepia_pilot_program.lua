-- Generated from GLSL by compile_subset.py; Beherith original shader, GPL v2 or later.
return {vertex=[=[#version 330
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
