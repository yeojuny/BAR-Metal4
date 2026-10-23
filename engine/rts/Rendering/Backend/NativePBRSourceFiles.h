/* Exact selected31657 original module provenance. */
#pragma once
#include <array>
#include <string_view>
namespace rendering::pbrsource {
struct File {std::string_view path,sha512;};
inline constexpr std::array<File,3> Files={{
{"luarules/gadgets/api_pbr_enabler.lua","85a22392c707363cdc5e8505ad3a794d0b1ce159cfc007dc9586420fb9690648d8c6fe8eb9d6f4b31fa24e97b4587dd5ecd50f699a411300dea70ed10576807a"},
{"luarules/gadgets/include/genbrdflut.lua","bf2448a1b6ed6006b960fb23cdb552a374268ce6df7108ebff593d344428d0164832270415c1cc4638bea257ec45a7b898c2f4fd44d56f690665e952090dbe93"},
{"luarules/gadgets/include/genenvlut.lua","f43ddf0abad78b573aabfe6ba09105ae374a00f6417d0bd7d2560392d2908730d888c477e753d1fcc7c75b718f98eaf0098977e484343eaecece736c7c77dbff"},
}};
}
