/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include "NativeObjectSpotlightPrograms.h"
namespace rendering::commanderwreck {
inline constexpr std::uint64_t VertexHash=14865373688864505665ULL,FragmentHash=7944047687275521872ULL;
inline constexpr std::string_view Vertex=R"WRECK(    #version 420
    #line 10000

    //__ENGINEUNIFORMBUFFERDEFS__

    layout (location = 0) in vec3 local_pos;

    layout (location = 1) in vec3 world_pos;
    layout (location = 2) in float radius;
    layout (location = 3) in float height;
    layout (location = 4) in vec4 color;

    out DataVS {
        vec4 vertex_color;
        float local_y;
        float cameraDistance;
    };

    #line 15000
    void main() {
        cameraDistance = length(world_pos.xyz - cameraViewInv[3].xyz);

        float effectiveRadius = radius * clamp(cameraDistance / 4500, 1.0, 2.5);

        vec2 result_pos_xz = local_pos.xz * effectiveRadius + world_pos.xz;
        vec3 result_pos = vec3(result_pos_xz.x, world_pos.y + local_pos.y * height, result_pos_xz.y);

        gl_Position = cameraViewProj * vec4(result_pos, 1.0);

        vertex_color = color;
        local_y = local_pos.y;
    }
)WRECK",Fragment=R"WRECK(    #version 420
    #line 20000

    in DataVS {
        vec4 vertex_color;
        float local_y;
        float cameraDistance;
    };

    out vec4 output_color;

    #line 25000
    void main() {
        output_color.rgba = vec4(
            vertex_color.rgb,
            clamp(
                vertex_color.a
                * (1 - sqrt(local_y)) // more opacity near the ground
                * clamp(cameraDistance / 4500, 0.25, 1.1), // more opacity from far away
                0.0,
                1.0
            )
        );
    }
)WRECK";
inline constexpr objectspotlight::ArchiveSource ArchiveSources[]={
 {"LuaUI/Widgets/gfx_highlight_commander_wrecks.lua",5674525446992391721ULL},
 {"modules/graphics/luashader.lua",15173167305575988042ULL},
 {"modules/graphics/instancevbotable.lua",10155353677033079655ULL},
};
inline void ValidateProgram(std::string_view v,std::string_view f){if(v!=objectspotlight::Expanded(Vertex)||f!=Fragment)throw std::runtime_error("Commander Wreck requires the exact original expanded shader pair");}
}
