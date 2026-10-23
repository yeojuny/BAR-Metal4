#include "Throttler.h"
#include <algorithm>
#include <chrono>

Throttler::Throttler(unsigned req_per_sec, unsigned burst_size_)
	: req_per_msec(static_cast<double>(req_per_sec) / 1000.0)
	, burst_size(burst_size_)
	, bucket(burst_size_)
	, start_time(std::chrono::steady_clock::now())
{
}

void Throttler::refill_bucket()
{
	auto since_start = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::steady_clock::now() - start_time);
	unsigned new_generated = since_start.count() * req_per_msec;
	bucket = std::min(burst_size, bucket + (new_generated - generated));
	generated = new_generated;
}

bool Throttler::get_token()
{
	if (req_per_msec == 0.0) {
		return true;
	}
	if (bucket == 0) {
		return false;
	}
	--bucket;
	return true;
}
