#version 330

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
