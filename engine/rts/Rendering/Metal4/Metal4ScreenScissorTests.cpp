/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#if defined(METAL4_SCREEN_SCISSOR_TEST_MAIN)
#include "Metal4ScreenScissor.h"
#include <iostream>
#include <limits>
#include <stdexcept>

int main()
{
	try {
		using rendering::ScreenScissor;
		using rendering::metal4::ClampScreenScissor;
		unsigned checks = 0;
		auto check = [&](bool value) { ++checks; if (!value) throw std::runtime_error("Screen scissor CPU check failed"); };
		auto rect = ClampScreenScissor(std::nullopt, 128, 96);
		check(rect && rect->x == 0 && rect->y == 0 && rect->width == 128 && rect->height == 96);
		rect = ClampScreenScissor(ScreenScissor{20, 25, 50, 40}, 128, 96);
		check(rect && rect->x == 20 && rect->y == 31 && rect->width == 50 && rect->height == 40);
		rect = ClampScreenScissor(ScreenScissor{-5, -6, 10, 12}, 64, 64);
		check(rect && rect->x == 0 && rect->y == 58 && rect->width == 5 && rect->height == 6);
		rect = ClampScreenScissor(ScreenScissor{60, 60, 20, 20}, 64, 64);
		check(rect && rect->x == 60 && rect->y == 0 && rect->width == 4 && rect->height == 4);
		check(!ClampScreenScissor(ScreenScissor{100, 10, 5, 5}, 64, 64));
		check(!ClampScreenScissor(ScreenScissor{-100, -100, 10, 10}, 64, 64));
		check(!ClampScreenScissor(ScreenScissor{10, 10, 0, 10}, 64, 64));
		check(!ClampScreenScissor(ScreenScissor{10, 10, 10, 0}, 64, 64));
		check(!ClampScreenScissor(std::nullopt, 0, 64));
		rect = ClampScreenScissor(ScreenScissor{std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::min(), std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()}, 64, 64);
		check(rect && rect->x == 0 && rect->y == 0 && rect->width == 64 && rect->height == 64);
		check(!ClampScreenScissor(ScreenScissor{std::numeric_limits<int32_t>::max(), 0, std::numeric_limits<uint32_t>::max(), 10}, 64, 64));
		std::cout << "Screen scissor CPU checks passed: " << checks << '\n';
		return 0;
	} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
#endif
