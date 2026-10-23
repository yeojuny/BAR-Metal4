/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Rendering/Textures/NativeUITextureQualifiers.h"
#include <iostream>

using namespace rendering::ui;
namespace {
unsigned checks=0;
void Check(bool value,const char* reason) { ++checks; if (!value) throw std::runtime_error(reason); }
template<class F> void Reject(F action,const char* reason)
{
	bool failed=false; try { action(); } catch(const std::exception&) { failed=true; } Check(failed,reason);
}
}
int main()
{
	try {
		auto q=ParseNativeUITextureQualifiers(":lr48,64:LuaUI/Images/metal.png");
		Check(q.linear && !q.nearest && !q.clamped,"actual TopBar filter/repeat qualifiers differ");
		Check(q.resize==std::pair<std::uint32_t,std::uint32_t>{48,64},"r was not parsed as original resize");
		Check(q.name=="LuaUI/Images/metal.png","qualified VFS name damaged");
		q=ParseNativeUITextureQualifiers(":lcr0,7m:icon.png");
		Check(q.linear && q.clamped && q.mipNear && q.resize==std::pair<std::uint32_t,std::uint32_t>{1,7},"clamp/mip/zero resize contract differs");
		q=ParseNativeUITextureQualifiers(":nl:icon.png");Check(q.nearest && q.linear,"independent nearest/linear flags lost");
		q=ParseNativeUITextureQualifiers(":g:LuaUI/Images/rgbnoise.png");Check(q.greyed && q.name=="LuaUI/Images/rgbnoise.png" && !q.clamped,"actual TopBar grayscale qualifier differs");
		q=ParseNativeUITextureQualifiers(":gr48,64lc:icon.png");Check(q.greyed && q.linear && q.clamped && q.resize==std::pair<std::uint32_t,std::uint32_t>{48,64},"combined resize/grayscale state lost");
		q=ParseNativeUITextureQualifiers("icon.png");Check(q.name=="icon.png" && !q.resize && !q.clamped && !q.nearest && !q.linear,"unqualified original defaults differ");
		q=ParseNativeUITextureQualifiers(":lt0.3,0.3,0.3:LuaUI/Images/groupicons/energy.png");
		Check(q.linear && q.tint==std::array<float,3>{.3f,.3f,.3f} && q.name=="LuaUI/Images/groupicons/energy.png","actual disabled GridMenu tint differs");
		q=ParseNativeUITextureQualifiers(":lt0.63,0.63,0.63:LuaUI/Images/groupicons/weapon.png");
		Check(q.linear && q.tint==std::array<float,3>{.63f,.63f,.63f},"actual under-construction tint differs");
		q=ParseNativeUITextureQualifiers(":t +3e-1, 0x1p-1,.63gr48,64l:icon.png");
		Check(q.tint==std::array<float,3>{.3f,.5f,.63f} && q.greyed && q.resize==std::pair<std::uint32_t,std::uint32_t>{48,64} && q.linear,"original strtod sign/exponent/hex grammar or following qualifiers lost");
		q=ParseNativeUITextureQualifiers(":t0.3,0.3,0.3t2,1,0:icon.png");
		Check(q.tint==std::array<float,3>{2,1,0},"original final tint qualifier did not win");
		q=ParseNativeUITextureQualifiers(":t-0,1e-5000,1:icon.png");
		Check(q.tint==std::array<float,3>{0,0,1},"original signed zero/underflow grammar differs");
		Reject([]{ParseNativeUITextureQualifiers(":lr48,64");},"unterminated qualifiers accepted");
		Reject([]{ParseNativeUITextureQualifiers(":r48:icon.png");},"missing height accepted");
		Reject([]{ParseNativeUITextureQualifiers(":r,64:icon.png");},"missing width accepted");
		Reject([]{ParseNativeUITextureQualifiers(":r16385,64:icon.png");},"unbounded resize accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1,1:icon.png");},"missing tint blue accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1,,1:icon.png");},"empty tint green accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1,1,:icon.png");},"empty tint blue accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1;1,1:icon.png");},"invalid tint separators accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1 ,1,1:icon.png");},"nonoriginal whitespace before comma accepted");
		Reject([]{ParseNativeUITextureQualifiers(":tnan,1,1:icon.png");},"NaN tint accepted");
		Reject([]{ParseNativeUITextureQualifiers(":tinf,1,1:icon.png");},"infinite tint accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t-1,1,1:icon.png");},"undefined negative byte tint accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1e300,1,1:icon.png");},"overflowing float tint accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1e20,1,1:icon.png");},"overflowing uint32 byte tint accepted");
		Reject([]{ParseNativeUITextureQualifiers(":t1,1,1x:icon.png");},"unsupported qualifier suffix silently ignored");
		Reject([]{ParseNativeUITextureQualifiers(":i:icon.png");},"unrelated unsupported transform silently accepted");
		std::cout << "NativeUITextureQualifiers: " << checks << " checks passed\n";return 0;
	} catch(const std::exception& error) { std::cerr << error.what() << '\n';return 1; }
}
