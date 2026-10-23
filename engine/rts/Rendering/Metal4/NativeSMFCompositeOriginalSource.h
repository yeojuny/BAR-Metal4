/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
namespace rendering::metal4::smforiginal {
inline constexpr const char* COMPOSITE_VERTEX_SOURCE=R"SMFORIG(#version 130

out vec2 texCoords;

void main() {
	texCoords = gl_Vertex.xy * 0.5 + 0.5;
	gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
}
)SMFORIG";
inline constexpr const char* COMPOSITE_FRAGMENT_SOURCE=R"SMFORIG(#version 130

uniform sampler2D colorTex;
uniform sampler2D depthTex;

in vec2 texCoords;

out vec4 fragColor;

void main() {
	float depth = texture(depthTex, texCoords).r;
	vec4 color = texture(colorTex, texCoords);

	// only ground fragments carry a color; cleared, alpha-tested and Lua-drawn G-buffer pixels stay for later passes
	if (depth >= 1.0 || color.a == 0.0)
		discard;

	fragColor = color;
	gl_FragDepth = depth;
}
)SMFORIG";
}
