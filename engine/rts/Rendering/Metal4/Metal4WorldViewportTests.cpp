/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#include "Metal4WorldViewport.h"

#ifdef METAL4_WORLD_VIEWPORT_TEST_MAIN
#include <iostream>
#include <limits>

int main()
{
	using rendering::metal4::MapWorldViewport;
	unsigned checks = 0;
	auto check = [&](bool condition, const char* name) {
		++checks; if (!condition) throw std::runtime_error(name);
	};
	try {
		const auto full = MapWorldViewport({0,0,1280,720},1280,720,1024,1024);
		check(full && full->viewport == std::array<double,4>{0,0,1024,1024}, "Independent square target retains full logical camera viewport");
		check(full->scissor == std::array<uint32_t,4>{0,0,1024,1024}, "Full target scissor");
		const auto sub = MapWorldViewport({320,180,640,360},1280,720,1024,512);
		check(sub && sub->viewport == std::array<double,4>{256,128,512,256}, "Logical subview scales proportionally and flips y");
		check(sub->scissor == std::array<uint32_t,4>{256,128,512,256}, "Subview scissor matches mapped viewport");
		const auto bottom = MapWorldViewport({0,0,640,180},1280,720,1024,512);
		check(bottom && bottom->viewport == std::array<double,4>{0,384,512,128}, "Bottom-left logical subview appears at target bottom");
		const auto negative = MapWorldViewport({-100,-50,200,100},1000,500,1000,500);
		check(negative && negative->viewport == std::array<double,4>{-100,450,200,100}, "Negative viewport is not silently clipped/reprojected");
		check(negative->scissor == std::array<uint32_t,4>{0,450,100,50}, "Only negative viewport scissor clamps");
		const auto overflow = MapWorldViewport({900,450,200,100},1000,500,1000,500);
		check(overflow && overflow->viewport == std::array<double,4>{900,-50,200,100}, "Top-right viewport remains intact");
		check(overflow->scissor == std::array<uint32_t,4>{900,0,100,50}, "Top-right scissor clamps safely");
		check(!MapWorldViewport({1000,0,10,10},1000,500,1024,512), "Fully right viewport skips");
		check(!MapWorldViewport({0,-100,10,10},1000,500,1024,512), "Fully below viewport skips");
		check(!MapWorldViewport({std::numeric_limits<int32_t>::max(),0,std::numeric_limits<int32_t>::max(),10},1000,500,1024,512), "Signed endpoint addition does not overflow");
		const auto fractional = MapWorldViewport({1,1,1,1},3,3,10,10);
		check(fractional && fractional->scissor == std::array<uint32_t,4>{3,3,4,4}, "Fractional viewport scissor conservatively covers pixel edges");
		bool rejected = false;
		try { (void)MapWorldViewport({0,0,10,10},0,500,1024,512); } catch (const std::runtime_error&) { rejected = true; }
		check(rejected, "Zero logical extent rejects");
		rejected = false;
		try { (void)MapWorldViewport({0,0,-1,10},1000,500,1024,512); } catch (const std::runtime_error&) { rejected = true; }
		check(rejected, "Invalid camera extent rejects");
		std::cout << "World viewport: " << checks << " checks passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "World viewport failed: " << error.what() << '\n';
		return 1;
	}
}
#endif
